// SPDX-License-Identifier: GPL-2.0
/* System call table for x86-64. */
/* x86-64 native ABI 的系统调用分发表和 trace 辅助表。 */

#include <linux/linkage.h>
#include <linux/sys.h>
#include <linux/cache.h>
#include <linux/syscalls.h>
#include <asm/syscall.h>

/* 第一次包含 syscalls_64.h 用于声明所有 __x64_* syscall 符号。 */
#define __SYSCALL(nr, sym) extern long __x64_##sym(const struct pt_regs *);
#define __SYSCALL_NORETURN(nr, sym) extern long __noreturn __x64_##sym(const struct pt_regs *);
#include <asm/syscalls_64.h>
#undef  __SYSCALL

#undef  __SYSCALL_NORETURN
#define __SYSCALL_NORETURN __SYSCALL

/*
 * The sys_call_table[] is no longer used for system calls, but
 * kernel/trace/trace_syscalls.c still wants to know the system
 * call address.
 *
 * sys_call_table[] 不再参与快速系统调用分派，但 tracing 仍通过它定位 syscall 地址。
 */
#define __SYSCALL(nr, sym) __x64_##sym,
const sys_call_ptr_t sys_call_table[] = {
#include <asm/syscalls_64.h>
};
#undef  __SYSCALL

#define __SYSCALL(nr, sym) case nr: return __x64_##sym(regs);
long x64_sys_call(const struct pt_regs *regs, unsigned int nr)
{
        /* 由 syscall 号分派到对应 __x64_* 实现，未知号返回 ni_syscall。 */
	switch (nr) {
	#include <asm/syscalls_64.h>
	default: return __x64_sys_ni_syscall(regs);
	}
};
