/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _ASM_X86_SHARED_IO_H
#define _ASM_X86_SHARED_IO_H

#include <linux/types.h>

#define BUILDIO(bwl, bw, type)						\
static __always_inline void __out##bwl(type value, u16 port)		\
{									\
	asm volatile("out" #bwl " %" #bw "0, %w1"			\
		     : : "a"(value), "Nd"(port));			\
}									\
									\
static __always_inline type __in##bwl(u16 port)				\
{									\
	type value;							\
	asm volatile("in" #bwl " %w1, %" #bw "0"			\
		     : "=a"(value) : "Nd"(port));			\
	return value;							\
}

/**
 * __outb 等价于：
 * outb %b0, %w1 : : "a"(value), "Nd"(port)
 *  movb $value, %al   ; AL = value (byte)
 *  movw $port, %dx    ; DX = port (16-bit)
 *  outb %al, %dx      ; 发送 AL 到 DX 端口
 *
 * inb %w1, %b0 : "=a"(value) : "Nd"(port)
 *  movw $port, %dx    ; DX = port (16-bit)
 *  inb %dx, %al       ; AL = 从端口读取的字节
 *  movb %al, value    ; value = AL
 */
BUILDIO (b, b, u8)
BUILDIO (w, w, u16)
BUILDIO (l, , u32)
#undef BUILDIO

#define inb __inb
#define inw __inw
#define inl __inl
#define outb __outb
#define outw __outw
#define outl __outl

#endif
