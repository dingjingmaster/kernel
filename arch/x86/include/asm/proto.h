/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _ASM_X86_PROTO_H
#define _ASM_X86_PROTO_H

#include <asm/ldt.h>

struct task_struct;

/* misc architecture specific prototypes */

void syscall_init(void);

#ifdef CONFIG_X86_64
void entry_SYSCALL_64(void);
void entry_SYSCALL_64_safe_stack(void);
void entry_SYSRETQ_unsafe_stack(void);
void entry_SYSRETQ_end(void);
long do_arch_prctl_64(struct task_struct *task, int option, unsigned long arg2);
#endif

#ifdef CONFIG_X86_32
void entry_INT80_32(void);
void entry_SYSENTER_32(void);
void __begin_SYSENTER_singlestep_region(void);
void __end_SYSENTER_singlestep_region(void);
#endif

#ifdef CONFIG_IA32_EMULATION
void entry_SYSENTER_compat(void);
void __end_entry_SYSENTER_compat(void);
void entry_SYSCALL_compat(void);
void entry_SYSCALL_compat_safe_stack(void);
void entry_SYSRETL_compat_unsafe_stack(void);
void entry_SYSRETL_compat_end(void);
#else /* !CONFIG_IA32_EMULATION */
#define entry_SYSCALL_compat NULL
#define entry_SYSENTER_compat NULL
#endif

/**
 * 根据 CPU 是否支持 NX 位（No-Execute），设置内核内存管理中的 _PAGE_NX 标志位。
 * 该标志位用于控制内存页的访问权限，禁止将数据页（如堆栈、堆）作为代码执行，从而防范 数据执行攻击（DEP）
 * - 若 CPU 支持 NX 位（如较新的 x86_64 处理器），则启用 _PAGE_NX，禁止数据页执行代码
 * - 若不支持 NX 位（如老旧的 x86 处理器），则回退到传统的保护机制（如页表权限控制）
 */
void x86_configure_nx(void);

extern int reboot_force;

long do_arch_prctl_common(int option, unsigned long arg2);

#endif /* _ASM_X86_PROTO_H */
