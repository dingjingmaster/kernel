/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _ASM_KASLR_H_
#define _ASM_KASLR_H_

unsigned long kaslr_get_random_long(const char *purpose);

#ifdef CONFIG_RANDOMIZE_MEMORY
void kernel_randomize_memory(void);
void init_trampoline_kaslr(void);
#else
/**
 * 函数是 Linux 内核中用于增强内存分配空间不可预测性的核心功能:
 *  1. 内存地址空间随机化: 通过随机化直接映射区、vmalloc 区域和 vmemmap 区域的基址，使内存分配位置难以被预测
 *  2. 提升系统安全性: 防止攻击者通过固定内存地址模式（如栈溢出、缓冲区攻击）预测关键数据位置，增强内核和应用程序的安全性;
 *    虽然 kernel_randomize_memory 主要针对内存分配地址，但与内核地址空间布局随机化（KASLR）结合使用时，可进一步混淆攻击者的定位能力
 *  内存分配函数（如 vmalloc、kmalloc）返回的地址范围变得不可预测，影响依赖固定地址的驱动或模块
 */
static inline void kernel_randomize_memory(void) { }
static inline void init_trampoline_kaslr(void) {}
#endif /* CONFIG_RANDOMIZE_MEMORY */

#endif
