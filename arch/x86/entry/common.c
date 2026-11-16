// SPDX-License-Identifier: GPL-2.0-only
/*
 * common.c - C code for kernel entry and exit
 *
 * x86 内核入口/退出的 C 侧公共代码，承接汇编入口完成 syscall、int80、
 * SYSENTER/SYSCALL32、FRED 兼容入口和 Xen PV upcall 的分派。
 * Copyright (c) 2015 Andrew Lutomirski
 *
 * Based on asm and ptrace code by many authors.  The code here originated
 * in ptrace.c and signal.c.
 */

#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/sched/task_stack.h>
#include <linux/entry-common.h>
#include <linux/mm.h>
#include <linux/smp.h>
#include <linux/errno.h>
#include <linux/ptrace.h>
#include <linux/export.h>
#include <linux/nospec.h>
#include <linux/syscalls.h>
#include <linux/uaccess.h>
#include <linux/init.h>

#ifdef CONFIG_XEN_PV
#    include <xen/xen-ops.h>
#    include <xen/events.h>
#endif

#include <asm/apic.h>
#include <asm/desc.h>
#include <asm/traps.h>
#include <asm/vdso.h>
#include <asm/cpufeature.h>
#include <asm/fpu/api.h>
#include <asm/nospec-branch.h>
#include <asm/io_bitmap.h>
#include <asm/syscall.h>
#include <asm/irq_stack.h>

#ifdef CONFIG_X86_64

static __always_inline bool do_syscall_x64 (struct pt_regs* regs, int nr)
{
    /*
     * Convert negative numbers to very high and thus out of range
     * numbers for comparisons.
     *
     * 将负 syscall 号转换为很大的无符号数，统一走越界判断。
     */
    unsigned int unr = nr;

    if (likely (unr < NR_syscalls)) {
        /* array_index_nospec 防止越界 syscall 号参与推测执行。 */
        unr      = array_index_nospec (unr, NR_syscalls);
        regs->ax = x64_sys_call (regs, unr);
        return true;
    }
    return false;
}

static __always_inline bool do_syscall_x32 (struct pt_regs* regs, int nr)
{
    /*
     * Adjust the starting offset of the table, and convert numbers
     * < __X32_SYSCALL_BIT to very high and thus out of range
     * numbers for comparisons.
     *
     * x32 syscall 号带 __X32_SYSCALL_BIT，需要先减去偏移后再查 x32 表。
     */
    unsigned int xnr = nr - __X32_SYSCALL_BIT;

    if (IS_ENABLED (CONFIG_X86_X32_ABI) && likely (xnr < X32_NR_syscalls)) {
        xnr      = array_index_nospec (xnr, X32_NR_syscalls);
        regs->ax = x32_sys_call (regs, xnr);
        return true;
    }
    return false;
}

/* Returns true to return using SYSRET, or false to use IRET */
__visible noinstr bool do_syscall_64 (struct pt_regs* regs, int nr)
{
    /* 64 位 syscall 入口: 进入 syscall 框架、分派 x64/x32、再返回用户态。 */
    add_random_kstack_offset ();
    nr = syscall_enter_from_user_mode (regs, nr);

    instrumentation_begin ();

    if (!do_syscall_x64 (regs, nr) && !do_syscall_x32 (regs, nr) && nr != -1) {
        /* Invalid system call, but still a system call. */
        /* 未知 syscall 仍按系统调用处理，返回 -ENOSYS。 */
        regs->ax = __x64_sys_ni_syscall (regs);
    }

    instrumentation_end ();
    syscall_exit_to_user_mode (regs);

    /*
     * Check that the register state is valid for using SYSRET to exit
     * to userspace.  Otherwise use the slower but fully capable IRET
     * exit path.
     */

    /* XEN PV guests always use the IRET path */
    /* Xen PV 不能使用 SYSRET，必须走 IRET。 */
    if (cpu_feature_enabled (X86_FEATURE_XENPV))
        return false;

    /* SYSRET requires RCX == RIP and R11 == EFLAGS */
    /* SYSRET 硬件约束要求 RCX/R11 与 RIP/EFLAGS 完全匹配。 */
    if (unlikely (regs->cx != regs->ip || regs->r11 != regs->flags))
        return false;

    /* CS and SS must match the values set in MSR_STAR */
    /* SYSRET 使用 MSR_STAR 中的用户段选择子，pt_regs 必须一致。 */
    if (unlikely (regs->cs != __USER_CS || regs->ss != __USER_DS))
        return false;

    /*
     * On Intel CPUs, SYSRET with non-canonical RCX/RIP will #GP
     * in kernel space.  This essentially lets the user take over
     * the kernel, since userspace controls RSP.
     *
     * TASK_SIZE_MAX covers all user-accessible addresses other than
     * the deprecated vsyscall page.
     */
    if (unlikely (regs->ip >= TASK_SIZE_MAX))
        /* 非 canonical/非用户地址 RIP 会让 Intel SYSRET 在内核态 #GP。 */
        return false;

    /*
     * SYSRET cannot restore RF.  It can restore TF, but unlike IRET,
     * restoring TF results in a trap from userspace immediately after
     * SYSRET.
     */
    if (unlikely (regs->flags & (X86_EFLAGS_RF | X86_EFLAGS_TF)))
        return false;

    /* Use SYSRET to exit to userspace */
    /* 所有约束满足时使用更快的 SYSRET 返回。 */
    return true;
}
#endif

#if defined(CONFIG_X86_32) || defined(CONFIG_IA32_EMULATION)
static __always_inline int syscall_32_enter (struct pt_regs* regs)
{
    /* 32 位 syscall 入口设置 compat 状态，并取出 orig_ax 中的 syscall 号。 */
    if (IS_ENABLED (CONFIG_IA32_EMULATION))
        current_thread_info ()->status |= TS_COMPAT;

    return (int)regs->orig_ax;
}

#    ifdef CONFIG_IA32_EMULATION
bool __ia32_enabled __ro_after_init =
        !IS_ENABLED (CONFIG_IA32_EMULATION_DEFAULT_DISABLED);

static int ia32_emulation_override_cmdline (char* arg)
{
    /* ia32_emulation= 可在启动时开关 IA32 emulation。 */
    return kstrtobool (arg, &__ia32_enabled);
}
early_param ("ia32_emulation", ia32_emulation_override_cmdline);
#    endif

/*
 * Invoke a 32-bit syscall.  Called with IRQs on in CT_STATE_KERNEL.
 */
static __always_inline void do_syscall_32_irqs_on (struct pt_regs* regs, int nr)
{
    /*
     * Convert negative numbers to very high and thus out of range
     * numbers for comparisons.
     *
     * 与 64 位路径相同，负 syscall 号转成无符号大数统一越界。
     */
    unsigned int unr = nr;

    if (likely (unr < IA32_NR_syscalls)) {
        unr      = array_index_nospec (unr, IA32_NR_syscalls);
        /* 分派到 IA32 syscall 表。 */
        regs->ax = ia32_sys_call (regs, unr);
    } else if (nr != -1) {
        regs->ax = __ia32_sys_ni_syscall (regs);
    }
}

#    ifdef CONFIG_IA32_EMULATION
static __always_inline bool int80_is_external (void)
{
    const unsigned int offs = (0x80 / 32) * 0x10;
    const u32          bit  = BIT (0x80 % 32);

    /* The local APIC on XENPV guests is fake */
    /* Xen PV 的 local APIC 是模拟的，不能用 ISR 判断 int80 来源。 */
    if (cpu_feature_enabled (X86_FEATURE_XENPV))
        return false;

    /*
     * If vector 0x80 is set in the APIC ISR then this is an external
     * interrupt. Either from broken hardware or injected by a VMM.
     *
     * Note: In guest mode this is only valid for secure guests where
     * the secure module fully controls the vAPIC exposed to the guest.
     */
    return apic_read (APIC_ISR + offs) & bit;
}

/**
 * do_int80_emulation - 32-bit legacy syscall C entry from asm
 *
 * This entry point can be used by 32-bit and 64-bit programs to perform
 * 32-bit system calls.  Instances of INT $0x80 can be found inline in
 * various programs and libraries.  It is also used by the vDSO's
 * __kernel_vsyscall fallback for hardware that doesn't support a faster
 * entry method.  Restarted 32-bit system calls also fall back to INT
 * $0x80 regardless of what instruction was originally used to do the
 * system call.
 *
 * This is considered a slow path.  It is not used by most libc
 * implementations on modern hardware except during process startup.
 *
 * The arguments for the INT $0x80 based syscall are on stack in the
 * pt_regs structure:
 *   eax:                system call number
 *   ebx, ecx, edx, esi, edi, ebp:    arg1 - arg 6
 */
__visible noinstr void do_int80_emulation (struct pt_regs* regs)
{
    int nr;

    /* Kernel does not use INT $0x80! */
    /* 内核态不应执行 INT $0x80，出现则说明是异常外部中断/错误路径。 */
    if (unlikely (!user_mode (regs))) {
        irqentry_enter (regs);
        instrumentation_begin ();
        panic ("Unexpected external interrupt 0x80\n");
    }

    /*
     * Establish kernel context for instrumentation, including for
     * int80_is_external() below which calls into the APIC driver.
     * Identical for soft and external interrupts.
     */
    enter_from_user_mode (regs);

    instrumentation_begin ();
    add_random_kstack_offset ();

    /* Validate that this is a soft interrupt to the extent possible */
    /* 尽最大可能确认这是用户态软件中断，而不是外部中断伪装。 */
    if (unlikely (int80_is_external ()))
        panic ("Unexpected external interrupt 0x80\n");

    /*
     * The low level idtentry code pushed -1 into regs::orig_ax
     * and regs::ax contains the syscall number.
     *
     * User tracing code (ptrace or signal handlers) might assume
     * that the regs::orig_ax contains a 32-bit number on invoking
     * a 32-bit syscall.
     *
     * Establish the syscall convention by saving the 32bit truncated
     * syscall number in regs::orig_ax and by invalidating regs::ax.
     */
    regs->orig_ax = regs->ax & GENMASK (31, 0);
    /* 建立 IA32 syscall 约定: orig_ax 保存 32 位 syscall 号，ax 先置 -ENOSYS。 */
    regs->ax      = -ENOSYS;

    nr            = syscall_32_enter (regs);

    local_irq_enable ();
    nr = syscall_enter_from_user_mode_work (regs, nr);
    do_syscall_32_irqs_on (regs, nr);

    instrumentation_end ();
    syscall_exit_to_user_mode (regs);
}

#        ifdef CONFIG_X86_FRED
/*
 * A FRED-specific INT80 handler is warranted for the follwing reasons:
 *
 * 1) As INT instructions and hardware interrupts are separate event
 *    types, FRED does not preclude the use of vector 0x80 for external
 *    interrupts. As a result, the FRED setup code does not reserve
 *    vector 0x80 and calling int80_is_external() is not merely
 *    suboptimal but actively incorrect: it could cause a system call
 *    to be incorrectly ignored.
 *
 * 2) It is called only for handling vector 0x80 of event type
 *    EVENT_TYPE_SWINT and will never be called to handle any external
 *    interrupt (event type EVENT_TYPE_EXTINT).
 *
 * 3) FRED has separate entry flows depending on if the event came from
 *    user space or kernel space, and because the kernel does not use
 *    INT insns, the FRED kernel entry handler fred_entry_from_kernel()
 *    falls through to fred_bad_type() if the event type is
 *    EVENT_TYPE_SWINT, i.e., INT insns. So if the kernel is handling
 *    an INT insn, it can only be from a user level.
 *
 * 4) int80_emulation() does a CLEAR_BRANCH_HISTORY. While FRED will
 *    likely take a different approach if it is ever needed: it
 *    probably belongs in either fred_intx()/ fred_other() or
 *    asm_fred_entrypoint_user(), depending on if this ought to be done
 *    for all entries from userspace or only system
 *    calls.
 *
 * 5) INT $0x80 is the fast path for 32-bit system calls under FRED.
 */
DEFINE_FREDENTRY_RAW (int80_emulation)
{
    int nr;

    enter_from_user_mode (regs);

    /* FRED 下 INT80 只会来自用户 SWINT 事件，不需要 APIC ISR 外部中断判断。 */
    instrumentation_begin ();
    add_random_kstack_offset ();

    /*
     * FRED pushed 0 into regs::orig_ax and regs::ax contains the
     * syscall number.
     *
     * User tracing code (ptrace or signal handlers) might assume
     * that the regs::orig_ax contains a 32-bit number on invoking
     * a 32-bit syscall.
     *
     * Establish the syscall convention by saving the 32bit truncated
     * syscall number in regs::orig_ax and by invalidating regs::ax.
     */
    regs->orig_ax = regs->ax & GENMASK (31, 0);
    /* 与传统 int80 一样建立 32 位 syscall pt_regs 约定。 */
    regs->ax      = -ENOSYS;

    nr            = syscall_32_enter (regs);

    local_irq_enable ();
    nr = syscall_enter_from_user_mode_work (regs, nr);
    do_syscall_32_irqs_on (regs, nr);

    instrumentation_end ();
    syscall_exit_to_user_mode (regs);
}
#        endif
#    else  /* CONFIG_IA32_EMULATION */

/* Handles int $0x80 on a 32bit kernel */
__visible noinstr void do_int80_syscall_32 (struct pt_regs* regs)
{
    /* 32 位内核上的 int $0x80 系统调用入口。 */
    int nr = syscall_32_enter (regs);

    add_random_kstack_offset ();
    /*
     * Subtlety here: if ptrace pokes something larger than 2^31-1 into
     * orig_ax, the int return value truncates it. This matches
     * the semantics of syscall_get_nr().
     */
    nr = syscall_enter_from_user_mode (regs, nr);
    instrumentation_begin ();

    do_syscall_32_irqs_on (regs, nr);

    instrumentation_end ();
    syscall_exit_to_user_mode (regs);
}
#    endif /* !CONFIG_IA32_EMULATION */

static noinstr bool __do_fast_syscall_32 (struct pt_regs* regs)
{
    int nr = syscall_32_enter (regs);
    int res;

    add_random_kstack_offset ();
    /*
     * This cannot use syscall_enter_from_user_mode() as it has to
     * fetch EBP before invoking any of the syscall entry work
     * functions.
     *
     * fast 32 位入口必须先从 vDSO 保存位置取回 EBP，再执行通用 syscall work。
     */
    syscall_enter_from_user_mode_prepare (regs);

    instrumentation_begin ();
    /* Fetch EBP from where the vDSO stashed it. */
    /* vDSO 按约定把用户 EBP 暂存在用户栈顶。 */
    if (IS_ENABLED (CONFIG_X86_64)) {
        /*
         * Micro-optimization: the pointer we're following is
         * explicitly 32 bits, so it can't be out of range.
         */
        res = __get_user (*(u32*)&regs->bp,
                          (u32 __user __force*)(unsigned long)(u32)regs->sp);
    } else {
        res = get_user (*(u32*)&regs->bp,
                        (u32 __user __force*)(unsigned long)(u32)regs->sp);
    }

    if (res) {
        /* User code screwed up. */
        /* 用户栈不可读时返回 -EFAULT，并退回 IRET 路径。 */
        regs->ax = -EFAULT;

        local_irq_disable ();
        instrumentation_end ();
        irqentry_exit_to_user_mode (regs);
        return false;
    }

    nr = syscall_enter_from_user_mode_work (regs, nr);

    /* Now this is just like a normal syscall. */
    do_syscall_32_irqs_on (regs, nr);

    instrumentation_end ();
    syscall_exit_to_user_mode (regs);
    return true;
}

/* Returns true to return using SYSEXIT/SYSRETL, or false to use IRET */
__visible noinstr bool do_fast_syscall_32 (struct pt_regs* regs)
{
    /*
     * Called using the internal vDSO SYSENTER/SYSCALL32 calling
     * convention.  Adjust regs so it looks like we entered using int80.
     *
     * vDSO fast path 进入后，把寄存器修正成类似 int80 的 pt_regs 形态。
     */
    unsigned long landing_pad = (unsigned long)current->mm->context.vdso +
                                vdso_image_32.sym_int80_landing_pad;

    /*
     * SYSENTER loses EIP, and even SYSCALL32 needs us to skip forward
     * so that 'regs->ip -= 2' lands back on an int $0x80 instruction.
     * Fix it up.
     */
    regs->ip = landing_pad;

    /* Invoke the syscall. If it failed, keep it simple: use IRET. */
    /* syscall 处理失败时走更通用的 IRET 返回。 */
    if (!__do_fast_syscall_32 (regs))
        return false;

    /*
     * Check that the register state is valid for using SYSRETL/SYSEXIT
     * to exit to userspace.  Otherwise use the slower but fully capable
     * IRET exit path.
     */

    /* XEN PV guests always use the IRET path */
    if (cpu_feature_enabled (X86_FEATURE_XENPV))
        return false;

    /* EIP must point to the VDSO landing pad */
    if (unlikely (regs->ip != landing_pad))
        return false;

    /* CS and SS must match the values set in MSR_STAR */
    if (unlikely (regs->cs != __USER32_CS || regs->ss != __USER_DS))
        return false;

    /* If the TF, RF, or VM flags are set, use IRET */
    if (unlikely (regs->flags & (X86_EFLAGS_RF | X86_EFLAGS_TF | X86_EFLAGS_VM)))
        return false;

    /* Use SYSRETL/SYSEXIT to exit to userspace */
    /* 所有约束满足时使用 SYSRETL/SYSEXIT 快速返回。 */
    return true;
}

/* Returns true to return using SYSEXIT/SYSRETL, or false to use IRET */
__visible noinstr bool do_SYSENTER_32 (struct pt_regs* regs)
{
    /* SYSENTER loses RSP, but the vDSO saved it in RBP. */
    /* SYSENTER 不保存用户栈指针，vDSO 已提前放在 RBP。 */
    regs->sp = regs->bp;

    /* SYSENTER clobbers EFLAGS.IF.  Assume it was set in usermode. */
    regs->flags |= X86_EFLAGS_IF;

    return do_fast_syscall_32 (regs);
}
#endif

SYSCALL_DEFINE0 (ni_syscall)
{
    /* 默认未实现 syscall 返回 -ENOSYS。 */
    return -ENOSYS;
}

#ifdef CONFIG_XEN_PV
#    ifndef CONFIG_PREEMPTION
/*
 * Some hypercalls issued by the toolstack can take many 10s of
 * seconds. Allow tasks running hypercalls via the privcmd driver to
 * be voluntarily preempted even if full kernel preemption is
 * disabled.
 *
 * Such preemptible hypercalls are bracketed by
 * xen_preemptible_hcall_begin() and xen_preemptible_hcall_end()
 * calls.
 */
DEFINE_PER_CPU (bool, xen_in_preemptible_hcall);
EXPORT_SYMBOL_GPL (xen_in_preemptible_hcall);

/*
 * In case of scheduling the flag must be cleared and restored after
 * returning from schedule as the task might move to a different CPU.
 */
static __always_inline bool get_and_clear_inhcall (void)
{
    /* 调度前清除 per-CPU 标志，避免任务迁移后污染目标 CPU。 */
    bool inhcall = __this_cpu_read (xen_in_preemptible_hcall);

    __this_cpu_write (xen_in_preemptible_hcall, false);
    return inhcall;
}

static __always_inline void restore_inhcall (bool inhcall)
{
    __this_cpu_write (xen_in_preemptible_hcall, inhcall);
}
#    else
static __always_inline bool get_and_clear_inhcall (void)
{
    return false;
}
static __always_inline void restore_inhcall (bool inhcall)
{
}
#    endif

static void __xen_pv_evtchn_do_upcall (struct pt_regs* regs)
{
    /* 在 irq_regs 上下文中执行 Xen event channel upcall。 */
    struct pt_regs* old_regs = set_irq_regs (regs);

    inc_irq_stat (irq_hv_callback_count);

    xen_evtchn_do_upcall ();

    set_irq_regs (old_regs);
}

__visible noinstr void xen_pv_evtchn_do_upcall (struct pt_regs* regs)
{
    irqentry_state_t state = irqentry_enter (regs);
    bool             inhcall;

    instrumentation_begin ();
    /* 条件性切换到 IRQ 栈运行 Xen upcall。 */
    run_sysvec_on_irqstack_cond (__xen_pv_evtchn_do_upcall, regs);

    inhcall = get_and_clear_inhcall ();
    if (inhcall && !WARN_ON_ONCE (state.exit_rcu)) {
        irqentry_exit_cond_resched ();
        instrumentation_end ();
        restore_inhcall (inhcall);
    } else {
        instrumentation_end ();
        irqentry_exit (regs, state);
    }
}
#endif /* CONFIG_XEN_PV */
