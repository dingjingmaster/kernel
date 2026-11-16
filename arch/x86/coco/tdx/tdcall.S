/* SPDX-License-Identifier: GPL-2.0 */
#include <asm/asm-offsets.h>            /* 引入结构体字段偏移，供汇编宏访问 tdx_module_args。 */
#include <asm/asm.h>                    /* 引入 x86 汇编辅助宏。 */

#include <linux/linkage.h>              /* 引入 SYM_FUNC_START/END 等符号声明宏。 */
#include <linux/errno.h>                /* 引入错误码定义，供底层 TDX 调用宏使用。 */

#include "../../virt/vmx/tdx/tdxcall.S" /* 复用 VMX/TDX 公共的 TDX_MODULE_CALL 宏实现。 */

.section .noinstr.text, "ax"            /* TDX 低级调用放入 noinstr 可执行段，避免插桩。 */

/*
 * __tdcall()  - Used by TDX guests to request services from the TDX
 * module (does not include VMM services) using TDCALL instruction.
 *
 * __tdcall() function ABI:
 *
 * @fn   (RDI)	- TDCALL Leaf ID, moved to RAX
 * @args (RSI)	- struct tdx_module_args for input
 *
 * Only RCX/RDX/R8-R11 are used as input registers.
 *
 * Return status of TDCALL via RAX.
 *
 * TDX guest 通过该入口向 TDX module 请求服务，不包含 VMM 服务。
 * 输入 leaf 从 RDI 移到 RAX，参数结构体由 RSI 指向；返回状态在 RAX。
 */
SYM_FUNC_START(__tdcall)                /* 定义 __tdcall 函数入口。 */
        TDX_MODULE_CALL host=0          /* 执行 guest 侧 TDCALL，不保存输出寄存器。 */
SYM_FUNC_END(__tdcall)                  /* 标记 __tdcall 函数结束。 */

/*
 * __tdcall_ret() - Used by TDX guests to request services from the TDX
 * module (does not include VMM services) using TDCALL instruction, with
 * saving output registers to the 'struct tdx_module_args' used as input.
 *
 * __tdcall_ret() function ABI:
 *
 * @fn   (RDI)	- TDCALL Leaf ID, moved to RAX
 * @args (RSI)	- struct tdx_module_args for input and output
 *
 * Only RCX/RDX/R8-R11 are used as input/output registers.
 *
 * Return status of TDCALL via RAX.
 *
 * 与 __tdcall() 类似，但会把 TDX module 输出寄存器写回 args。
 */
SYM_FUNC_START(__tdcall_ret)            /* 定义 __tdcall_ret 函数入口。 */
        TDX_MODULE_CALL host=0 ret=1    /* 执行 TDCALL，并保存返回寄存器到 args。 */
SYM_FUNC_END(__tdcall_ret)              /* 标记 __tdcall_ret 函数结束。 */

/*
 * __tdcall_saved_ret() - Used by TDX guests to request services from the
 * TDX module (including VMM services) using TDCALL instruction, with
 * saving output registers to the 'struct tdx_module_args' used as input.
 *
 * __tdcall_saved_ret() function ABI:
 *
 * @fn   (RDI)	- TDCALL leaf ID, moved to RAX
 * @args (RSI)	- struct tdx_module_args for input/output
 *
 * All registers in @args are used as input/output registers.
 *
 * On successful completion, return the hypercall error code.
 *
 * 该入口用于包含 VMM 服务的 TDCALL/TDVMCALL，所有 args 中的寄存器都参与保存。
 * TDCALL 机制成功时返回 hypercall 自身的错误码。
 */
SYM_FUNC_START(__tdcall_saved_ret)      /* 定义 __tdcall_saved_ret 函数入口。 */
        TDX_MODULE_CALL host=0 ret=1 saved=1 /* 执行 TDCALL，保存完整输入/输出寄存器集合。 */
SYM_FUNC_END(__tdcall_saved_ret)        /* 标记 __tdcall_saved_ret 函数结束。 */
