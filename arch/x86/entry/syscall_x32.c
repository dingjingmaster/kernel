// SPDX-License-Identifier: GPL-2.0
/* System call table for x32 ABI. */
/* x32 ABI 的系统调用分发表，复用 x86-64 syscall 实现符号。 */

#include <linux/linkage.h>
#include <linux/sys.h>
#include <linux/cache.h>
#include <linux/syscalls.h>
#include <asm/syscall.h>

/* x32 表项最终映射到 __x64_* syscall 包装函数。 */
#define __SYSCALL(nr, sym) extern long __x64_##sym(const struct pt_regs *);
#define __SYSCALL_NORETURN(nr, sym) extern long __noreturn __x64_##sym(const struct pt_regs *);
#include <asm/syscalls_x32.h>
#undef  __SYSCALL

#undef  __SYSCALL_NORETURN
#define __SYSCALL_NORETURN __SYSCALL

#define __SYSCALL(nr, sym) case nr: return __x64_##sym(regs);
long x32_sys_call(const struct pt_regs *regs, unsigned int nr)
{
        /* 按 x32 syscall 号分派，未知号返回 x64 ni_syscall。 */
	switch (nr) {
	#include <asm/syscalls_x32.h>
	default: return __x64_sys_ni_syscall(regs);
	}
};
