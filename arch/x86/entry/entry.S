/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Common place for both 32- and 64-bit entry routines.
 *
 * 32 位和 64 位入口代码共用的低层汇编辅助例程。
 */

#include <linux/export.h>
#include <linux/linkage.h>
#include <asm/msr-index.h>
#include <asm/unwind_hints.h>
#include <asm/segment.h>
#include <asm/cache.h>
#include <asm/cpufeatures.h>
#include <asm/nospec-branch.h>

#include "calling.h"

.pushsection .noinstr.text, "ax"        /* 进入 noinstr 可执行段，避免入口安全路径被插桩。 */

SYM_FUNC_START(entry_ibpb)              /* 定义入口路径使用的 IBPB 刷新函数。 */
        movl    $MSR_IA32_PRED_CMD, %ecx /* ECX 指定 IA32_PRED_CMD MSR。 */
        movl    $PRED_CMD_IBPB, %eax    /* EAX 低 32 位写入 IBPB 命令。 */
        xorl    %edx, %edx              /* EDX 高 32 位清零。 */
        wrmsr                           /* 写 MSR，触发间接分支预测屏障。 */

	/* Make sure IBPB clears return stack preductions too. */
        FILL_RETURN_BUFFER %rax, RSB_CLEAR_LOOPS, X86_BUG_IBPB_NO_RET /* 对受影响 CPU 额外填充 RSB。 */
        RET                             /* 返回调用者。 */
SYM_FUNC_END(entry_ibpb)                /* 标记 entry_ibpb 函数结束。 */
/* For KVM */
EXPORT_SYMBOL_GPL(entry_ibpb);

.popsection                             /* 退出 noinstr.text 段。 */

/*
 * Define the VERW operand that is disguised as entry code so that
 * it can be referenced with KPTI enabled. This ensure VERW can be
 * used late in exit-to-user path after page tables are switched.
 */
.pushsection .entry.text, "ax"          /* 放入入口代码段，确保 KPTI 切页表后仍可访问。 */

.align L1_CACHE_BYTES, 0xcc             /* 按缓存行对齐，并用 int3 填充间隙。 */
SYM_CODE_START_NOALIGN(mds_verw_sel)    /* 定义伪装成入口代码的 VERW 段选择子操作数。 */
        UNWIND_HINT_UNDEFINED           /* 该位置不是普通可展开函数帧。 */
        ANNOTATE_NOENDBR                /* 不需要 ENDBR 标注。 */
        .word __KERNEL_DS               /* VERW 使用的内核数据段选择子。 */
.align L1_CACHE_BYTES, 0xcc             /* 末尾同样缓存行对齐，隔离该操作数。 */
SYM_CODE_END(mds_verw_sel);             /* 标记 mds_verw_sel 结束。 */
/* For KVM */
EXPORT_SYMBOL_GPL(mds_verw_sel);

.popsection                             /* 退出 entry.text 段。 */

THUNK warn_thunk_thunk, __warn_thunk    /* 生成跳转到 __warn_thunk 的 thunk 包装。 */

#ifndef CONFIG_X86_64
/*
 * Clang's implementation of TLS stack cookies requires the variable in
 * question to be a TLS variable. If the variable happens to be defined as an
 * ordinary variable with external linkage in the same compilation unit (which
 * amounts to the whole of vmlinux with LTO enabled), Clang will drop the
 * segment register prefix from the references, resulting in broken code. Work
 * around this by avoiding the symbol used in -mstack-protector-guard-symbol=
 * entirely in the C code, and use an alias emitted by the linker script
 * instead.
 */
#ifdef CONFIG_STACKPROTECTOR
EXPORT_SYMBOL(__ref_stack_chk_guard);
#endif
#endif
