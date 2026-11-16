/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Compatibility mode system call entry point for x86-64.
 *
 * x86-64 内核上的兼容模式系统调用入口，处理 32 位 SYSENTER、32 位 SYSCALL
 * 和 int $0x80 进入内核的低层汇编路径。
 *
 * Copyright 2000-2002 Andi Kleen, SuSE Labs.
 */
#include <asm/asm-offsets.h>
#include <asm/current.h>
#include <asm/errno.h>
#include <asm/thread_info.h>
#include <asm/segment.h>
#include <asm/irqflags.h>
#include <asm/asm.h>
#include <asm/smap.h>
#include <asm/nospec-branch.h>
#include <linux/linkage.h>
#include <linux/err.h>

#include "calling.h"

	.section .entry.text, "ax"	/* 低层入口代码放入 entry.text 段。 */

/*
 * 32-bit SYSENTER entry.
 *
 * 32-bit system calls through the vDSO's __kernel_vsyscall enter here
 * on 64-bit kernels running on Intel CPUs.
 *
 * The SYSENTER instruction, in principle, should *only* occur in the
 * vDSO.  In practice, a small number of Android devices were shipped
 * with a copy of Bionic that inlined a SYSENTER instruction.  This
 * never happened in any of Google's Bionic versions -- it only happened
 * in a narrow range of Intel-provided versions.
 *
 * SYSENTER loads SS, RSP, CS, and RIP from previously programmed MSRs.
 * IF and VM in RFLAGS are cleared (IOW: interrupts are off).
 * SYSENTER does not save anything on the stack,
 * and does not save old RIP (!!!), RSP, or RFLAGS.
 *
 * Arguments:
 * eax  system call number
 * ebx  arg1
 * ecx  arg2
 * edx  arg3
 * esi  arg4
 * edi  arg5
 * ebp  user stack
 * 0(%ebp) arg6
 */
SYM_CODE_START(entry_SYSENTER_compat)	/* 32 位兼容 SYSENTER 入口。 */
	UNWIND_HINT_ENTRY		/* 告诉 objtool 这是入口栈状态。 */
	ENDBR				/* CET IBT 间接分支入口标记。 */
	/* Interrupts are off on entry. */
	swapgs				/* 切换到内核 GS base。 */

	pushq	%rax			/* 保存 syscall 号，借用 RAX 切换 CR3。 */
	SWITCH_TO_KERNEL_CR3 scratch_reg=%rax /* 从用户页表切到内核页表。 */
	popq	%rax			/* 恢复 syscall 号。 */

	movq	PER_CPU_VAR(pcpu_hot + X86_top_of_stack), %rsp /* 切到当前 CPU 内核栈顶。 */

	/* Construct struct pt_regs on stack */
	pushq	$__USER_DS		/* pt_regs->ss */ /* 保存用户数据段。 */
	pushq	$0			/* pt_regs->sp = 0 (placeholder) */ /* SYSENTER 不保存用户 SP，先占位。 */

	/*
	 * Push flags.  This is nasty.  First, interrupts are currently
	 * off, but we need pt_regs->flags to have IF set.  Second, if TS
	 * was set in usermode, it's still set, and we're singlestepping
	 * through this code.  do_SYSENTER_32() will fix up IF.
	 */
	pushfq				/* pt_regs->flags (except IF = 0) */ /* 保存当前 RFLAGS，IF 稍后修正。 */
	pushq	$__USER32_CS		/* pt_regs->cs */ /* 保存 32 位用户代码段。 */
	pushq	$0			/* pt_regs->ip = 0 (placeholder) */ /* SYSENTER 不保存返回 RIP，先占位。 */
SYM_INNER_LABEL(entry_SYSENTER_compat_after_hwframe, SYM_L_GLOBAL) /* 硬件帧构造完成标签。 */

	/*
	 * User tracing code (ptrace or signal handlers) might assume that
	 * the saved RAX contains a 32-bit number when we're invoking a 32-bit
	 * syscall.  Just in case the high bits are nonzero, zero-extend
	 * the syscall number.  (This could almost certainly be deleted
	 * with no ill effects.)
	 */
	movl	%eax, %eax		/* syscall 号零扩展到 64 位，丢弃高 32 位。 */

	pushq	%rax			/* pt_regs->orig_ax */ /* 保存原始 syscall 号。 */
	PUSH_AND_CLEAR_REGS rax=$-ENOSYS /* 保存通用寄存器，并把返回值初始化为 -ENOSYS。 */
	UNWIND_HINT_REGS		/* 告诉 objtool 栈顶是 pt_regs。 */

	cld				/* 清除方向标志，保证字符串操作向前。 */

	/*
	 * SYSENTER doesn't filter flags, so we need to clear NT and AC
	 * ourselves.  To save a few cycles, we can check whether
	 * either was set instead of doing an unconditional popfq.
	 * This needs to happen before enabling interrupts so that
	 * we don't get preempted with NT set.
	 *
	 * If TF is set, we will single-step all the way to here -- do_debug
	 * will ignore all the traps.  (Yes, this is slow, but so is
	 * single-stepping in general.  This allows us to avoid having
	 * a more complicated code to handle the case where a user program
	 * forces us to single-step through the SYSENTER entry code.)
	 *
	 * NB.: .Lsysenter_fix_flags is a label with the code under it moved
	 * out-of-line as an optimization: NT is unlikely to be set in the
	 * majority of the cases and instead of polluting the I$ unnecessarily,
	 * we're keeping that code behind a branch which will predict as
	 * not-taken and therefore its instructions won't be fetched.
	 */
	testl	$X86_EFLAGS_NT|X86_EFLAGS_AC|X86_EFLAGS_TF, EFLAGS(%rsp) /* 检查 NT/AC/TF 是否需要修正。 */
	jnz	.Lsysenter_fix_flags	/* 有风险标志时跳到慢路径修正。 */
.Lsysenter_flags_fixed:		/* 标志位已安全的继续执行点。 */

	/*
	 * CPU bugs mitigations mechanisms can call other functions. They
	 * should be invoked after making sure TF is cleared because
	 * single-step is ignored only for instructions inside the
	 * entry_SYSENTER_compat function.
	 */
	IBRS_ENTER			/* 进入内核 IBRS 缓解状态。 */
	UNTRAIN_RET			/* 清理返回预测训练状态。 */
	CLEAR_BRANCH_HISTORY		/* 清理分支历史，缓解 BHI。 */

	movq	%rsp, %rdi		/* 将 pt_regs 指针作为第一个参数。 */
	call	do_SYSENTER_32		/* 调用 C 侧 SYSENTER32 处理函数。 */
	jmp	sysret32_from_system_call /* 跳到 32 位快速返回公共路径。 */

.Lsysenter_fix_flags:
	pushq	$X86_EFLAGS_FIXED	/* 准备安全的固定 EFLAGS。 */
	popfq				/* 清除 NT/AC/TF 等不安全标志。 */
	jmp	.Lsysenter_flags_fixed	/* 回到主路径。 */
SYM_INNER_LABEL(__end_entry_SYSENTER_compat, SYM_L_GLOBAL) /* SYSENTER 入口末尾标签。 */
SYM_CODE_END(entry_SYSENTER_compat)	/* 标记 SYSENTER 兼容入口结束。 */

/*
 * 32-bit SYSCALL entry.
 *
 * 32-bit system calls through the vDSO's __kernel_vsyscall enter here
 * on 64-bit kernels running on AMD CPUs.
 *
 * The SYSCALL instruction, in principle, should *only* occur in the
 * vDSO.  In practice, it appears that this really is the case.
 * As evidence:
 *
 *  - The calling convention for SYSCALL has changed several times without
 *    anyone noticing.
 *
 *  - Prior to the in-kernel X86_BUG_SYSRET_SS_ATTRS fixup, anything
 *    user task that did SYSCALL without immediately reloading SS
 *    would randomly crash.
 *
 *  - Most programmers do not directly target AMD CPUs, and the 32-bit
 *    SYSCALL instruction does not exist on Intel CPUs.  Even on AMD
 *    CPUs, Linux disables the SYSCALL instruction on 32-bit kernels
 *    because the SYSCALL instruction in legacy/native 32-bit mode (as
 *    opposed to compat mode) is sufficiently poorly designed as to be
 *    essentially unusable.
 *
 * 32-bit SYSCALL saves RIP to RCX, clears RFLAGS.RF, then saves
 * RFLAGS to R11, then loads new SS, CS, and RIP from previously
 * programmed MSRs.  RFLAGS gets masked by a value from another MSR
 * (so CLD and CLAC are not needed).  SYSCALL does not save anything on
 * the stack and does not change RSP.
 *
 * Note: RFLAGS saving+masking-with-MSR happens only in Long mode
 * (in legacy 32-bit mode, IF, RF and VM bits are cleared and that's it).
 * Don't get confused: RFLAGS saving+masking depends on Long Mode Active bit
 * (EFER.LMA=1), NOT on bitness of userspace where SYSCALL executes
 * or target CS descriptor's L bit (SYSCALL does not read segment descriptors).
 *
 * Arguments:
 * eax  system call number
 * ecx  return address
 * ebx  arg1
 * ebp  arg2	(note: not saved in the stack frame, should not be touched)
 * edx  arg3
 * esi  arg4
 * edi  arg5
 * esp  user stack
 * 0(%esp) arg6
 */
SYM_CODE_START(entry_SYSCALL_compat)	/* 32 位兼容 SYSCALL 入口。 */
	UNWIND_HINT_ENTRY		/* 告诉 objtool 这是入口栈状态。 */
	ENDBR				/* CET IBT 间接分支入口标记。 */
	/* Interrupts are off on entry. */
	swapgs				/* 切换到内核 GS base。 */

	/* Stash user ESP */
	movl	%esp, %r8d		/* 暂存 32 位用户栈指针 ESP。 */

	/* Use %rsp as scratch reg. User ESP is stashed in r8 */
	SWITCH_TO_KERNEL_CR3 scratch_reg=%rsp /* 使用 RSP 作临时寄存器切到内核页表。 */

	/* Switch to the kernel stack */
	movq	PER_CPU_VAR(pcpu_hot + X86_top_of_stack), %rsp /* 切到当前 CPU 内核栈顶。 */

SYM_INNER_LABEL(entry_SYSCALL_compat_safe_stack, SYM_L_GLOBAL) /* 已位于安全内核栈的标签。 */
	ANNOTATE_NOENDBR		/* 该内部标签不是间接分支入口。 */

	/* Construct struct pt_regs on stack */
	pushq	$__USER_DS		/* pt_regs->ss */ /* 保存用户数据段。 */
	pushq	%r8			/* pt_regs->sp */ /* 保存前面暂存的用户 ESP。 */
	pushq	%r11			/* pt_regs->flags */ /* 保存 SYSCALL 写入 R11 的用户 RFLAGS。 */
	pushq	$__USER32_CS		/* pt_regs->cs */ /* 保存 32 位用户代码段。 */
	pushq	%rcx			/* pt_regs->ip */ /* 保存 SYSCALL 写入 RCX 的返回地址。 */
SYM_INNER_LABEL(entry_SYSCALL_compat_after_hwframe, SYM_L_GLOBAL) /* 硬件帧构造完成标签。 */
	movl	%eax, %eax		/* discard orig_ax high bits */ /* syscall 号零扩展。 */
	pushq	%rax			/* pt_regs->orig_ax */ /* 保存原始 syscall 号。 */
	PUSH_AND_CLEAR_REGS rcx=%rbp rax=$-ENOSYS /* 保存寄存器，pt_regs->cx 使用用户 EBP。 */
	UNWIND_HINT_REGS		/* 告诉 objtool 栈顶是 pt_regs。 */

	IBRS_ENTER			/* 进入内核 IBRS 缓解状态。 */
	UNTRAIN_RET			/* 清理返回预测训练状态。 */
	CLEAR_BRANCH_HISTORY		/* 清理分支历史，缓解 BHI。 */

	movq	%rsp, %rdi		/* 将 pt_regs 指针作为第一个参数。 */
	call	do_fast_syscall_32	/* 调用 C 侧 fast 32 位 syscall 处理。 */

sysret32_from_system_call:		/* 32 位兼容 syscall 快速返回公共路径。 */
	/* XEN PV guests always use IRET path */
	ALTERNATIVE "testb %al, %al; jz swapgs_restore_regs_and_return_to_usermode", \
		    "jmp swapgs_restore_regs_and_return_to_usermode", X86_FEATURE_XENPV /* Xen PV 强制走 IRET 返回。 */

	/*
	 * Opportunistic SYSRET
	 *
	 * We are not going to return to userspace from the trampoline
	 * stack. So let's erase the thread stack right now.
	 */
	STACKLEAK_ERASE			/* 在返回用户态前擦除内核线程栈残留。 */

	IBRS_EXIT			/* 退出内核 IBRS 缓解状态。 */

	movq	RBX(%rsp), %rbx		/* pt_regs->rbx */ /* 恢复用户 RBX。 */
	movq	RBP(%rsp), %rbp		/* pt_regs->rbp */ /* 恢复用户 RBP。 */
	movq	EFLAGS(%rsp), %r11	/* pt_regs->flags (in r11) */ /* SYSRETL 要求 RFLAGS 在 R11。 */
	movq	RIP(%rsp), %rcx		/* pt_regs->ip (in rcx) */ /* SYSRETL 要求返回 EIP 在 RCX。 */
	addq	$RAX, %rsp		/* Skip r8-r15 */ /* 跳过 pt_regs 中 r8-r15 到 rax。 */
	popq	%rax			/* pt_regs->rax */ /* 恢复 syscall 返回值。 */
	popq	%rdx			/* Skip pt_regs->cx */ /* 跳过保存的 CX/EBP 槽。 */
	popq	%rdx			/* pt_regs->dx */ /* 恢复用户 EDX。 */
	popq	%rsi			/* pt_regs->si */ /* 恢复用户 ESI。 */
	popq	%rdi			/* pt_regs->di */ /* 恢复用户 EDI。 */

        /*
         * USERGS_SYSRET32 does:
         *  GSBASE = user's GS base
         *  EIP = ECX
         *  RFLAGS = R11
         *  CS = __USER32_CS
         *  SS = __USER_DS
         *
	 * ECX will not match pt_regs->cx, but we're returning to a vDSO
	 * trampoline that will fix up RCX, so this is okay.
	 *
	 * R12-R15 are callee-saved, so they contain whatever was in them
	 * when the system call started, which is already known to user
	 * code.  We zero R8-R10 to avoid info leaks.
         */
	movq	RSP-ORIG_RAX(%rsp), %rsp /* 恢复原始用户 RSP/ESP。 */
SYM_INNER_LABEL(entry_SYSRETL_compat_unsafe_stack, SYM_L_GLOBAL) /* 已切回用户栈值但仍在内核页表的标签。 */
	ANNOTATE_NOENDBR		/* 该内部标签不是间接分支入口。 */

	/*
	 * The original userspace %rsp (RSP-ORIG_RAX(%rsp)) is stored
	 * on the process stack which is not mapped to userspace and
	 * not readable after we SWITCH_TO_USER_CR3.  Delay the CR3
	 * switch until after after the last reference to the process
	 * stack.
	 *
	 * %r8/%r9 are zeroed before the sysret, thus safe to clobber.
	 */
	SWITCH_TO_USER_CR3_NOSTACK scratch_reg=%r8 scratch_reg2=%r9 /* 切回用户页表，不再访问内核栈。 */

	xorl	%r8d, %r8d		/* 清零 R8，避免向用户泄露内核值。 */
	xorl	%r9d, %r9d		/* 清零 R9，避免向用户泄露内核值。 */
	xorl	%r10d, %r10d		/* 清零 R10，避免向用户泄露内核值。 */
	swapgs				/* 切回用户 GS base。 */
	CLEAR_CPU_BUFFERS		/* 返回用户态前清理 CPU 微架构缓冲区。 */
	sysretl				/* 以 32 位 SYSRET 返回用户态。 */
SYM_INNER_LABEL(entry_SYSRETL_compat_end, SYM_L_GLOBAL) /* SYSRETL 返回区末尾标签。 */
	ANNOTATE_NOENDBR		/* 该内部标签不是间接分支入口。 */
	int3				/* 防止意外落入后续代码。 */
SYM_CODE_END(entry_SYSCALL_compat)	/* 标记兼容 SYSCALL 入口结束。 */

/*
 * int 0x80 is used by 32 bit mode as a system call entry. Normally idt entries
 * point to C routines, however since this is a system call interface the branch
 * history needs to be scrubbed to protect against BHI attacks, and that
 * scrubbing needs to take place in assembly code prior to entering any C
 * routines.
 */
SYM_CODE_START(int80_emulation)		/* int $0x80 汇编跳板入口。 */
	ANNOTATE_NOENDBR		/* 该入口由 IDT 路径到达，不需要 ENDBR。 */
	UNWIND_HINT_FUNC		/* 告诉 objtool 这是普通函数形态。 */
	CLEAR_BRANCH_HISTORY		/* 进入 C 处理前清理分支历史。 */
	jmp do_int80_emulation		/* 跳转到 C 侧 int80 emulation 处理器。 */
SYM_CODE_END(int80_emulation)		/* 标记 int80 跳板结束。 */
