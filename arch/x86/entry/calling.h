/* SPDX-License-Identifier: GPL-2.0 */
#include <linux/jump_label.h>
#include <asm/unwind_hints.h>
#include <asm/cpufeatures.h>
#include <asm/page_types.h>
#include <asm/percpu.h>
#include <asm/asm-offsets.h>
#include <asm/processor-flags.h>
#include <asm/ptrace-abi.h>
#include <asm/msr.h>
#include <asm/nospec-branch.h>

/*

 x86 function call convention, 64-bit:
 -------------------------------------
  arguments           |  callee-saved      | extra caller-saved | return
 [callee-clobbered]   |                    | [callee-clobbered] |
 ---------------------------------------------------------------------------
 rdi rsi rdx rcx r8-9 | rbx rbp [*] r12-15 | r10-11             | rax, rdx [**]

 ( rsp is obviously invariant across normal function calls. (gcc can 'merge'
   functions when it sees tail-call optimization possibilities) rflags is
   clobbered. Leftover arguments are passed over the stack frame.)

 [*]  In the frame-pointers case rbp is fixed to the stack frame.

 [**] for struct return values wider than 64 bits the return convention is a
      bit more complex: up to 128 bits width we return small structures
      straight in rax, rdx. For structures larger than that (3 words or
      larger) the caller puts a pointer to an on-stack return struct
      [allocated in the caller's stack frame] into the first argument - i.e.
      into rdi. All other arguments shift up by one in this case.
      Fortunately this case is rare in the kernel.

For 32-bit we have the following conventions - kernel is built with
-mregparm=3 and -freg-struct-return:

 x86 function calling convention, 32-bit:
 ----------------------------------------
  arguments         | callee-saved        | extra caller-saved | return
 [callee-clobbered] |                     | [callee-clobbered] |
 -------------------------------------------------------------------------
 eax edx ecx        | ebx edi esi ebp [*] | <none>             | eax, edx [**]

 ( here too esp is obviously invariant across normal function calls. eflags
   is clobbered. Leftover arguments are passed over the stack frame. )

 [*]  In the frame-pointers case ebp is fixed to the stack frame.

 [**] We build with -freg-struct-return, which on 32-bit means similar
      semantics as on 64-bit: edx can be used for a second return value
      (i.e. covering integer and structure sizes up to 64 bits) - after that
      it gets more complex and more expensive: 3-word or larger struct returns
      get done in the caller's frame and the pointer to the return struct goes
      into regparm0, i.e. eax - the other arguments shift up and the
      function's register parameters degenerate to regparm=2 in essence.

*/
/*
 * 中文说明:
 * 本文件定义 x86 入口汇编共用的调用约定、寄存器保存/清理、KPTI CR3 切换、
 * Spectre/MDS 缓解辅助宏，以及 C 函数 thunk 包装。
 */

#ifdef CONFIG_X86_64

/*
 * 64-bit system call stack frame layout defines and helpers,
 * for assembly code:
 */

.macro PUSH_REGS rdx=%rdx rcx=%rcx rax=%rax save_ret=0 unwind_hint=1
	.if \save_ret
	pushq	%rsi		/* pt_regs->si */
	movq	8(%rsp), %rsi	/* temporarily store the return address in %rsi */
	movq	%rdi, 8(%rsp)	/* pt_regs->di (overwriting original return address) */
	.else
	pushq   %rdi		/* pt_regs->di */
	pushq   %rsi		/* pt_regs->si */
	.endif
	pushq	\rdx		/* pt_regs->dx */
	pushq   \rcx		/* pt_regs->cx */
	pushq   \rax		/* pt_regs->ax */
	pushq   %r8		/* pt_regs->r8 */
	pushq   %r9		/* pt_regs->r9 */
	pushq   %r10		/* pt_regs->r10 */
	pushq   %r11		/* pt_regs->r11 */
	pushq	%rbx		/* pt_regs->rbx */
	pushq	%rbp		/* pt_regs->rbp */
	pushq	%r12		/* pt_regs->r12 */
	pushq	%r13		/* pt_regs->r13 */
	pushq	%r14		/* pt_regs->r14 */
	pushq	%r15		/* pt_regs->r15 */

	.if \unwind_hint
	UNWIND_HINT_REGS
	.endif

	.if \save_ret
	pushq	%rsi		/* return address on top of stack */
	.endif
.endm

.macro CLEAR_REGS clear_bp=1
	/*
	 * Sanitize registers of values that a speculation attack might
	 * otherwise want to exploit. The lower registers are likely clobbered
	 * well before they could be put to use in a speculative execution
	 * gadget.
	 */
	xorl	%esi,  %esi	/* nospec si  */
	xorl	%edx,  %edx	/* nospec dx  */
	xorl	%ecx,  %ecx	/* nospec cx  */
	xorl	%r8d,  %r8d	/* nospec r8  */
	xorl	%r9d,  %r9d	/* nospec r9  */
	xorl	%r10d, %r10d	/* nospec r10 */
	xorl	%r11d, %r11d	/* nospec r11 */
	xorl	%ebx,  %ebx	/* nospec rbx */
	.if \clear_bp
	xorl	%ebp,  %ebp	/* nospec rbp */
	.endif
	xorl	%r12d, %r12d	/* nospec r12 */
	xorl	%r13d, %r13d	/* nospec r13 */
	xorl	%r14d, %r14d	/* nospec r14 */
	xorl	%r15d, %r15d	/* nospec r15 */

.endm

.macro PUSH_AND_CLEAR_REGS rdx=%rdx rcx=%rcx rax=%rax save_ret=0 clear_bp=1 unwind_hint=1
        /* 先保存寄存器到 pt_regs，再清空推测执行可能利用的寄存器值。 */
	PUSH_REGS rdx=\rdx, rcx=\rcx, rax=\rax, save_ret=\save_ret unwind_hint=\unwind_hint
	CLEAR_REGS clear_bp=\clear_bp
.endm

.macro POP_REGS pop_rdi=1
        popq %r15                       /* 从 pt_regs 恢复 r15。 */
        popq %r14                       /* 从 pt_regs 恢复 r14。 */
        popq %r13                       /* 从 pt_regs 恢复 r13。 */
        popq %r12                       /* 从 pt_regs 恢复 r12。 */
        popq %rbp                       /* 从 pt_regs 恢复 rbp。 */
        popq %rbx                       /* 从 pt_regs 恢复 rbx。 */
        popq %r11                       /* 从 pt_regs 恢复 r11。 */
        popq %r10                       /* 从 pt_regs 恢复 r10。 */
        popq %r9                        /* 从 pt_regs 恢复 r9。 */
        popq %r8                        /* 从 pt_regs 恢复 r8。 */
        popq %rax                       /* 从 pt_regs 恢复 rax。 */
        popq %rcx                       /* 从 pt_regs 恢复 rcx。 */
        popq %rdx                       /* 从 pt_regs 恢复 rdx。 */
        popq %rsi                       /* 从 pt_regs 恢复 rsi。 */
	.if \pop_rdi
        popq %rdi                       /* 需要时从 pt_regs 恢复 rdi。 */
	.endif
.endm

#ifdef CONFIG_MITIGATION_PAGE_TABLE_ISOLATION

/*
 * MITIGATION_PAGE_TABLE_ISOLATION PGDs are 8k.  Flip bit 12 to switch between the two
 * halves:
 */
#define PTI_USER_PGTABLE_BIT		PAGE_SHIFT
#define PTI_USER_PGTABLE_MASK		(1 << PTI_USER_PGTABLE_BIT)
#define PTI_USER_PCID_BIT		X86_CR3_PTI_PCID_USER_BIT
#define PTI_USER_PCID_MASK		(1 << PTI_USER_PCID_BIT)
#define PTI_USER_PGTABLE_AND_PCID_MASK  (PTI_USER_PCID_MASK | PTI_USER_PGTABLE_MASK)

.macro SET_NOFLUSH_BIT	reg:req
        bts     $X86_CR3_PCID_NOFLUSH_BIT, \reg /* 设置 CR3 PCID no-flush 位。 */
.endm

.macro ADJUST_KERNEL_CR3 reg:req
        ALTERNATIVE "", "SET_NOFLUSH_BIT \reg", X86_FEATURE_PCID /* 支持 PCID 时避免不必要 TLB flush。 */
	/* Clear PCID and "MITIGATION_PAGE_TABLE_ISOLATION bit", point CR3 at kernel pagetables: */
        andq    $(~PTI_USER_PGTABLE_AND_PCID_MASK), \reg /* 清除用户 PGD/PCID 位，指向内核页表。 */
.endm

.macro SWITCH_TO_KERNEL_CR3 scratch_reg:req
        ALTERNATIVE "jmp .Lend_\@", "", X86_FEATURE_PTI /* 未启用 PTI 时跳过切换。 */
        mov     %cr3, \scratch_reg      /* 读取当前 CR3。 */
        ADJUST_KERNEL_CR3 \scratch_reg  /* 把 CR3 调整为内核页表版本。 */
        mov     \scratch_reg, %cr3      /* 写回 CR3，切到内核页表。 */
.Lend_\@:                              /* CR3 切换结束标签。 */
.endm

#define THIS_CPU_user_pcid_flush_mask   \
	PER_CPU_VAR(cpu_tlbstate + TLB_STATE_user_pcid_flush_mask)

.macro SWITCH_TO_USER_CR3 scratch_reg:req scratch_reg2:req
        mov     %cr3, \scratch_reg      /* 读取当前内核 CR3。 */

        ALTERNATIVE "jmp .Lwrcr3_\@", "", X86_FEATURE_PCID /* 无 PCID 时直接写用户 CR3。 */

	/*
	 * Test if the ASID needs a flush.
	 */
        movq    \scratch_reg, \scratch_reg2 /* 保存原 CR3。 */
        andq    $(0x7FF), \scratch_reg          /* mask ASID */ /* 提取当前 ASID/PCID。 */
        bt      \scratch_reg, THIS_CPU_user_pcid_flush_mask /* 测试该用户 PCID 是否需要 flush。 */
        jnc     .Lnoflush_\@            /* 不需要 flush 则走 no-flush 路径。 */

	/* Flush needed, clear the bit */
        btr     \scratch_reg, THIS_CPU_user_pcid_flush_mask /* 清除待 flush 标志位。 */
        movq    \scratch_reg2, \scratch_reg /* 恢复原 CR3 值，保留 flush 语义。 */
        jmp     .Lwrcr3_pcid_\@         /* 跳到 PCID 写 CR3 路径。 */

.Lnoflush_\@:
        movq    \scratch_reg2, \scratch_reg /* 恢复原 CR3。 */
        SET_NOFLUSH_BIT \scratch_reg    /* 设置 no-flush 位以保留 TLB。 */

.Lwrcr3_pcid_\@:
	/* Flip the ASID to the user version */
        orq     $(PTI_USER_PCID_MASK), \scratch_reg /* 切换到用户 PCID。 */

.Lwrcr3_\@:
	/* Flip the PGD to the user version */
        orq     $(PTI_USER_PGTABLE_MASK), \scratch_reg /* 切换到用户页表半区。 */
        mov     \scratch_reg, %cr3      /* 写回 CR3，切到用户页表。 */
.endm

.macro SWITCH_TO_USER_CR3_NOSTACK scratch_reg:req scratch_reg2:req
        ALTERNATIVE "jmp .Lend_\@", "", X86_FEATURE_PTI /* 未启用 PTI 时跳过。 */
        SWITCH_TO_USER_CR3 \scratch_reg \scratch_reg2 /* 无栈临时保存版本的用户 CR3 切换。 */
.Lend_\@:                              /* 宏结束标签。 */
.endm

.macro SWITCH_TO_USER_CR3_STACK	scratch_reg:req
        ALTERNATIVE "jmp .Lend_\@", "", X86_FEATURE_PTI /* 未启用 PTI 时跳过。 */
        pushq   %rax                    /* 借用 RAX 前先保存。 */
        SWITCH_TO_USER_CR3 scratch_reg=\scratch_reg scratch_reg2=%rax /* 用栈保存 RAX 的用户 CR3 切换。 */
        popq    %rax                    /* 恢复 RAX。 */
.Lend_\@:                              /* 宏结束标签。 */
.endm

.macro SAVE_AND_SWITCH_TO_KERNEL_CR3 scratch_reg:req save_reg:req
	ALTERNATIVE "jmp .Ldone_\@", "", X86_FEATURE_PTI /* 未启用 PTI 时无需保存/切换。 */
	movq	%cr3, \scratch_reg	/* 读取当前 CR3。 */
	movq	\scratch_reg, \save_reg /* 保存原 CR3 供返回时恢复。 */
	/*
	 * Test the user pagetable bit. If set, then the user page tables
	 * are active. If clear CR3 already has the kernel page table
	 * active.
	 */
	bt	$PTI_USER_PGTABLE_BIT, \scratch_reg /* 检测当前是否使用用户页表半区。 */
	jnc	.Ldone_\@		/* 已经是内核页表则无需切换。 */

	ADJUST_KERNEL_CR3 \scratch_reg	/* 调整为内核 CR3。 */
	movq	\scratch_reg, %cr3	/* 写回 CR3 切到内核页表。 */

.Ldone_\@:				/* 保存/切换完成标签。 */
.endm

/* Restore CR3 from a kernel context. May restore a user CR3 value. */
.macro PARANOID_RESTORE_CR3 scratch_reg:req save_reg:req
	ALTERNATIVE "jmp .Lend_\@", "", X86_FEATURE_PTI /* 未启用 PTI 时无需恢复。 */

	/*
	 * If CR3 contained the kernel page tables at the paranoid exception
	 * entry, then there is nothing to restore as CR3 is not modified while
	 * handling the exception.
	 */
	bt	$PTI_USER_PGTABLE_BIT, \save_reg /* 原 CR3 若不是用户页表，则无需恢复。 */
	jnc	.Lend_\@		/* 跳过恢复。 */

	ALTERNATIVE "jmp .Lwrcr3_\@", "", X86_FEATURE_PCID /* 无 PCID 时直接写 CR3。 */

	/*
	 * Check if there's a pending flush for the user ASID we're
	 * about to set.
	 */
	movq	\save_reg, \scratch_reg /* 复制待恢复的用户 CR3。 */
	andq	$(0x7FF), \scratch_reg /* 提取用户 ASID/PCID。 */
	btr	\scratch_reg, THIS_CPU_user_pcid_flush_mask /* 清除并测试待 flush 位。 */
	jc	.Lwrcr3_\@		/* 如果需要 flush，就不设置 no-flush 位。 */

	SET_NOFLUSH_BIT \save_reg	/* 无需 flush 时设置 no-flush 位。 */

.Lwrcr3_\@:
	movq	\save_reg, %cr3		/* 恢复原 CR3。 */
.Lend_\@:				/* 恢复结束标签。 */
.endm

#else /* CONFIG_MITIGATION_PAGE_TABLE_ISOLATION=n: */

.macro SWITCH_TO_KERNEL_CR3 scratch_reg:req
.endm
.macro SWITCH_TO_USER_CR3_NOSTACK scratch_reg:req scratch_reg2:req
.endm
.macro SWITCH_TO_USER_CR3_STACK scratch_reg:req
.endm
.macro SAVE_AND_SWITCH_TO_KERNEL_CR3 scratch_reg:req save_reg:req
.endm
.macro PARANOID_RESTORE_CR3 scratch_reg:req save_reg:req
.endm

#endif

/*
 * IBRS kernel mitigation for Spectre_v2.
 *
 * Assumes full context is established (PUSH_REGS, CR3 and GS) and it clobbers
 * the regs it uses (AX, CX, DX). Must be called before the first RET
 * instruction (NOTE! UNTRAIN_RET includes a RET instruction)
 *
 * The optional argument is used to save/restore the current value,
 * which is used on the paranoid paths.
 *
 * Assumes x86_spec_ctrl_{base,current} to have SPEC_CTRL_IBRS set.
 */
.macro IBRS_ENTER save_reg
#ifdef CONFIG_MITIGATION_IBRS_ENTRY
	ALTERNATIVE "jmp .Lend_\@", "", X86_FEATURE_KERNEL_IBRS
	movl	$MSR_IA32_SPEC_CTRL, %ecx

.ifnb \save_reg
	rdmsr
	shl	$32, %rdx
	or	%rdx, %rax
	mov	%rax, \save_reg
	test	$SPEC_CTRL_IBRS, %eax
	jz	.Ldo_wrmsr_\@
	lfence
	jmp	.Lend_\@
.Ldo_wrmsr_\@:
.endif

	movq	PER_CPU_VAR(x86_spec_ctrl_current), %rdx
	movl	%edx, %eax
	shr	$32, %rdx
	wrmsr
.Lend_\@:
#endif
.endm

/*
 * Similar to IBRS_ENTER, requires KERNEL GS,CR3 and clobbers (AX, CX, DX)
 * regs. Must be called after the last RET.
 */
.macro IBRS_EXIT save_reg
#ifdef CONFIG_MITIGATION_IBRS_ENTRY
	ALTERNATIVE "jmp .Lend_\@", "", X86_FEATURE_KERNEL_IBRS
	movl	$MSR_IA32_SPEC_CTRL, %ecx

.ifnb \save_reg
	mov	\save_reg, %rdx
.else
	movq	PER_CPU_VAR(x86_spec_ctrl_current), %rdx
	andl	$(~SPEC_CTRL_IBRS), %edx
.endif

	movl	%edx, %eax
	shr	$32, %rdx
	wrmsr
.Lend_\@:
#endif
.endm

/*
 * Mitigate Spectre v1 for conditional swapgs code paths.
 *
 * FENCE_SWAPGS_USER_ENTRY is used in the user entry swapgs code path, to
 * prevent a speculative swapgs when coming from kernel space.
 *
 * FENCE_SWAPGS_KERNEL_ENTRY is used in the kernel entry non-swapgs code path,
 * to prevent the swapgs from getting speculatively skipped when coming from
 * user space.
 */
.macro FENCE_SWAPGS_USER_ENTRY
	ALTERNATIVE "", "lfence", X86_FEATURE_FENCE_SWAPGS_USER
.endm
.macro FENCE_SWAPGS_KERNEL_ENTRY
	ALTERNATIVE "", "lfence", X86_FEATURE_FENCE_SWAPGS_KERNEL
.endm

.macro STACKLEAK_ERASE_NOCLOBBER
#ifdef CONFIG_GCC_PLUGIN_STACKLEAK
	PUSH_AND_CLEAR_REGS
	call stackleak_erase
	POP_REGS
#endif
.endm

.macro SAVE_AND_SET_GSBASE scratch_reg:req save_reg:req
	rdgsbase \save_reg
	GET_PERCPU_BASE \scratch_reg
	wrgsbase \scratch_reg
.endm

#else /* CONFIG_X86_64 */
# undef		UNWIND_HINT_IRET_REGS
# define	UNWIND_HINT_IRET_REGS
#endif /* !CONFIG_X86_64 */

.macro STACKLEAK_ERASE
#ifdef CONFIG_GCC_PLUGIN_STACKLEAK
	call stackleak_erase
#endif
.endm

#ifdef CONFIG_SMP

/*
 * CPU/node NR is loaded from the limit (size) field of a special segment
 * descriptor entry in GDT.
 */
.macro LOAD_CPU_AND_NODE_SEG_LIMIT reg:req
	movq	$__CPUNODE_SEG, \reg
	lsl	\reg, \reg
.endm

/*
 * Fetch the per-CPU GSBASE value for this processor and put it in @reg.
 * We normally use %gs for accessing per-CPU data, but we are setting up
 * %gs here and obviously can not use %gs itself to access per-CPU data.
 *
 * Do not use RDPID, because KVM loads guest's TSC_AUX on vm-entry and
 * may not restore the host's value until the CPU returns to userspace.
 * Thus the kernel would consume a guest's TSC_AUX if an NMI arrives
 * while running KVM's run loop.
 */
.macro GET_PERCPU_BASE reg:req
	LOAD_CPU_AND_NODE_SEG_LIMIT \reg
	andq	$VDSO_CPUNODE_MASK, \reg
	movq	__per_cpu_offset(, \reg, 8), \reg
.endm

#else

.macro GET_PERCPU_BASE reg:req
	movq	pcpu_unit_offsets(%rip), \reg
.endm

#endif /* CONFIG_SMP */

#ifdef CONFIG_X86_64

/* rdi:	arg1 ... normal C conventions. rax is saved/restored. */
.macro THUNK name, func
SYM_FUNC_START(\name)
	pushq %rbp
	movq %rsp, %rbp

	pushq %rdi
	pushq %rsi
	pushq %rdx
	pushq %rcx
	pushq %rax
	pushq %r8
	pushq %r9
	pushq %r10
	pushq %r11

	call \func

	popq %r11
	popq %r10
	popq %r9
	popq %r8
	popq %rax
	popq %rcx
	popq %rdx
	popq %rsi
	popq %rdi
	popq %rbp
	RET
SYM_FUNC_END(\name)
	_ASM_NOKPROBE(\name)
.endm

#else /* CONFIG_X86_32 */

/* put return address in eax (arg1) */
.macro THUNK name, func, put_ret_addr_in_eax=0
SYM_CODE_START_NOALIGN(\name)
	pushl %eax
	pushl %ecx
	pushl %edx

	.if \put_ret_addr_in_eax
	/* Place EIP in the arg1 */
	movl 3*4(%esp), %eax
	.endif

	call \func
	popl %edx
	popl %ecx
	popl %eax
	RET
	_ASM_NOKPROBE(\name)
SYM_CODE_END(\name)
	.endm

#endif
