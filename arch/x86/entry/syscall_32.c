// SPDX-License-Identifier: GPL-2.0
/* System call table for i386. */
/* i386/IA32 ABI 的系统调用分发表和 trace 辅助表。 */

#include <linux/linkage.h>
#include <linux/sys.h>
#include <linux/cache.h>
#include <linux/syscalls.h>
#include <asm/syscall.h>

#ifdef CONFIG_IA32_EMULATION
/* 64 位内核启用 IA32 emulation 时，32 位 syscall 使用 compat 实现。 */
#define __SYSCALL_WITH_COMPAT(nr, native, compat)	__SYSCALL(nr, compat)
#else
/* 纯 32 位内核直接使用 native 实现。 */
#define __SYSCALL_WITH_COMPAT(nr, native, compat)	__SYSCALL(nr, native)
#endif

/* 第一次包含 syscalls_32.h 用于声明所有 __ia32_* syscall 符号。 */
#define __SYSCALL(nr, sym) extern long __ia32_##sym(const struct pt_regs *);
#define __SYSCALL_NORETURN(nr, sym) extern long __noreturn __ia32_##sym(const struct pt_regs *);
#include <asm/syscalls_32.h>
#undef  __SYSCALL

#undef  __SYSCALL_NORETURN
#define __SYSCALL_NORETURN __SYSCALL

/*
 * The sys_call_table[] is no longer used for system calls, but
 * kernel/trace/trace_syscalls.c still wants to know the system
 * call address.
 *
 * sys_call_table[] 不再用于实际系统调用入口，但 trace_syscalls 仍需要地址表。
 */
#ifdef CONFIG_X86_32
#define __SYSCALL(nr, sym) __ia32_##sym,
const sys_call_ptr_t sys_call_table[] = {
#include <asm/syscalls_32.h>
};
#undef  __SYSCALL
#endif

#define __SYSCALL(nr, sym) case nr: return __ia32_##sym(regs);
long ia32_sys_call(const struct pt_regs *regs, unsigned int nr)
{
        /* 由 syscall 号分派到对应 __ia32_* 实现，未知号返回 ni_syscall。 */
	switch (nr) {
	#include <asm/syscalls_32.h>
	default: return __ia32_sys_ni_syscall(regs);
	}
};
