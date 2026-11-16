/* SPDX-License-Identifier: GPL-2.0 */
/*
 * The actual FRED entry points.
 *
 * FRED 事件投递后的实际 64 位汇编入口。
 */

#include <linux/export.h>

#include <asm/asm.h>
#include <asm/fred.h>
#include <asm/segment.h>

#include "calling.h"

	.code64				/* 后续按 64 位模式汇编。 */
	.section .noinstr.text, "ax"	/* FRED 入口放入 noinstr 可执行段，避免插桩。 */

.macro FRED_ENTER			/* FRED 入口公共序言。 */
	UNWIND_HINT_END_OF_STACK		/* 告诉 objtool 当前栈是入口栈底。 */
	ENDBR				/* CET IBT 入口标记。 */
	PUSH_AND_CLEAR_REGS		/* 保存通用寄存器并清理可推测使用的值。 */
	movq	%rsp, %rdi	/* %rdi -> pt_regs */ /* 将 pt_regs 指针作为第一个 C 参数。 */
.endm

.macro FRED_EXIT			/* FRED 入口公共尾声。 */
	UNWIND_HINT_REGS		/* 告诉 objtool 栈顶是 pt_regs。 */
	POP_REGS			/* 恢复通用寄存器。 */
.endm

/*
 * The new RIP value that FRED event delivery establishes is
 * IA32_FRED_CONFIG & ~FFFH for events that occur in ring 3.
 * Thus the FRED ring 3 entry point must be 4K page aligned.
 */
	.align 4096			/* 用户态 FRED 入口必须 4K 对齐。 */

SYM_CODE_START_NOALIGN(asm_fred_entrypoint_user) /* 用户态事件 FRED 汇编入口。 */
	FRED_ENTER			/* 保存现场并构造 pt_regs。 */
	call	fred_entry_from_user	/* 调用 C 侧用户态 FRED 分派器。 */
SYM_INNER_LABEL(asm_fred_exit_user, SYM_L_GLOBAL) /* 用户态 FRED 返回出口标签。 */
	FRED_EXIT			/* 恢复现场。 */
1:	ERETU				/* 从 FRED 事件返回用户态。 */

	_ASM_EXTABLE_TYPE(1b, asm_fred_entrypoint_user, EX_TYPE_ERETU) /* ERETU 失败时回入口处理。 */
SYM_CODE_END(asm_fred_entrypoint_user)	/* 标记用户态 FRED 入口结束。 */

/*
 * The new RIP value that FRED event delivery establishes is
 * (IA32_FRED_CONFIG & ~FFFH) + 256 for events that occur in
 * ring 0, i.e., asm_fred_entrypoint_user + 256.
 */
	.org asm_fred_entrypoint_user + 256, 0xcc /* 内核态入口固定在用户入口后 256 字节。 */
SYM_CODE_START_NOALIGN(asm_fred_entrypoint_kernel) /* 内核态事件 FRED 汇编入口。 */
	FRED_ENTER			/* 保存现场并构造 pt_regs。 */
	call	fred_entry_from_kernel	/* 调用 C 侧内核态 FRED 分派器。 */
	FRED_EXIT			/* 恢复现场。 */
	ERETS				/* 从 FRED 事件返回内核态。 */
SYM_CODE_END(asm_fred_entrypoint_kernel) /* 标记内核态 FRED 入口结束。 */

#if IS_ENABLED(CONFIG_KVM_INTEL)
SYM_FUNC_START(asm_fred_entry_from_kvm)	/* KVM VMX 注入 FRED 事件的辅助入口。 */
	push %rbp			/* 保存调用者帧指针。 */
	mov %rsp, %rbp			/* 建立当前栈帧。 */

	UNWIND_HINT_SAVE		/* 保存 objtool unwind 状态。 */

	/*
	 * Both IRQ and NMI from VMX can be handled on current task stack
	 * because there is no need to protect from reentrancy and the call
	 * stack leading to this helper is effectively constant and shallow
	 * (relatively speaking). Do the same when FRED is active, i.e., no
	 * need to check current stack level for a stack switch.
	 *
	 * Emulate the FRED-defined redzone and stack alignment.
	 */
	sub $(FRED_CONFIG_REDZONE_AMOUNT << 6), %rsp /* 模拟 FRED 定义的 redzone。 */
	and $FRED_STACK_FRAME_RSP_MASK, %rsp /* 调整栈对齐到 FRED 栈帧要求。 */

	/*
	 * Start to push a FRED stack frame, which is always 64 bytes:
	 *
	 * +--------+-----------------+
	 * | Bytes  | Usage           |
	 * +--------+-----------------+
	 * | 63:56  | Reserved        |
	 * | 55:48  | Event Data      |
	 * | 47:40  | SS + Event Info |
	 * | 39:32  | RSP             |
	 * | 31:24  | RFLAGS          |
	 * | 23:16  | CS + Aux Info   |
	 * |  15:8  | RIP             |
	 * |   7:0  | Error Code      |
	 * +--------+-----------------+
	 */
	push $0				/* Reserved, must be 0 */ /* FRED 栈帧保留字段必须为 0。 */
	push $0				/* Event data, 0 for IRQ/NMI */ /* IRQ/NMI 没有额外事件数据。 */
	push %rdi			/* fred_ss handed in by the caller */ /* 调用者传入的 fred_ss。 */
	push %rbp			/* 保存返回时使用的 RSP。 */
	pushf				/* 保存当前 RFLAGS。 */
	mov $__KERNEL_CS, %rax		/* 准备内核代码段选择子。 */
	push %rax			/* 压入 CS + aux info 字段。 */

	/*
	 * Unlike the IDT event delivery, FRED _always_ pushes an error code
	 * after pushing the return RIP, thus the CALL instruction CANNOT be
	 * used here to push the return RIP, otherwise there is no chance to
	 * push an error code before invoking the IRQ/NMI handler.
	 *
	 * Use LEA to get the return RIP and push it, then push an error code.
	 */
	lea 1f(%rip), %rax		/* 计算 ERETS 返回后的继续执行地址。 */
	push %rax				/* Return RIP */ /* 压入返回 RIP。 */
	push $0					/* Error code, 0 for IRQ/NMI */ /* IRQ/NMI 错误码为 0。 */

	PUSH_AND_CLEAR_REGS clear_bp=0 unwind_hint=0 /* 保存寄存器并清理推测敏感值，保留 bp。 */
	movq %rsp, %rdi				/* %rdi -> pt_regs */ /* 将 pt_regs 指针传给 C 入口。 */
	call __fred_entry_from_kvm		/* Call the C entry point */ /* 调用 KVM 专用 C 分派器。 */
	POP_REGS			/* 恢复通用寄存器。 */
	ERETS				/* 按 FRED 语义返回内核上下文。 */
1:					/* ERETS 返回后的普通函数尾声落点。 */
	/*
	 * Objtool doesn't understand what ERETS does, this hint tells it that
	 * yes, we'll reach here and with what stack state. A save/restore pair
	 * isn't strictly needed, but it's the simplest form.
	 */
	UNWIND_HINT_RESTORE		/* 恢复前面保存的 unwind 状态。 */
	pop %rbp			/* 恢复调用者帧指针。 */
	RET				/* 返回调用者。 */

SYM_FUNC_END(asm_fred_entry_from_kvm)	/* 标记 KVM FRED 辅助入口结束。 */
EXPORT_SYMBOL_GPL(asm_fred_entry_from_kvm); /* 导出给 KVM 使用。 */
#endif
