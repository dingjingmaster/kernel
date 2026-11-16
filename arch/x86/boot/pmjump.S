/* SPDX-License-Identifier: GPL-2.0-only */
/* ----------------------------------------------------------------------- *
 *
 *   Copyright (C) 1991, 1992 Linus Torvalds
 *   Copyright 2007 rPath, Inc. - All Rights Reserved
 *
 * ----------------------------------------------------------------------- */

/*
 * The actual transition into protected mode
 *
 * 真正完成从 16 位实模式切换到 32 位保护模式的跳转代码。
 */

#include <asm/boot.h>
#include <asm/processor-flags.h>
#include <asm/segment.h>
#include <linux/linkage.h>

	.text				/* 当前代码先放在普通文本段。 */
	.code16				/* 下面入口仍按 16 位实模式指令编码。 */

/*
 * void protected_mode_jump(u32 entrypoint, u32 bootparams);
 *
 * entrypoint 通过 EAX 传入，bootparams 通过 EDX 传入。
 */
SYM_FUNC_START_NOALIGN(protected_mode_jump)	/* 定义保护模式跳转入口函数。 */
	movl	%edx, %esi		# Pointer to boot_params table /* ESI 保存 boot_params 指针。 */

	xorl	%ebx, %ebx		/* 清零 EBX，准备计算当前代码段线性基址。 */
	movw	%cs, %bx		/* 取当前 CS 段值到 BX。 */
	shll	$4, %ebx		/* 实模式段地址左移 4 位得到线性基址。 */
	addl	%ebx, 2f		/* 将线性基址加到远跳转目标偏移上。 */
	jmp	1f			# Short jump to serialize on 386/486 /* 短跳转用于序列化流水线。 */
1:					/* 标签1: 流水线同步完成后继续。 */

	movw	$__BOOT_DS, %cx		/* CX 保存保护模式下的数据段选择子。 */
	movw	$__BOOT_TSS, %di	/* DI 保存临时 TSS 选择子。 */

	movl	%cr0, %edx		/* 读取 CR0 控制寄存器。 */
	orb	$X86_CR0_PE, %dl	# Protected mode /* 设置 CR0.PE 位，准备进入保护模式。 */
	movl	%edx, %cr0		/* 写回 CR0，正式开启保护模式。 */

	# Transition to 32-bit mode
	.byte	0x66, 0xea		# ljmpl opcode /* 手工发出 32 位操作数大小的远跳转操作码。 */
2:	.long	.Lin_pm32		# offset /* 远跳转目标偏移，前面已修正为线性地址。 */
	.word	__BOOT_CS		# segment /* 远跳转目标代码段选择子。 */
SYM_FUNC_END(protected_mode_jump)	/* 标记 protected_mode_jump 结束。 */

	.code32				/* 后续按 32 位保护模式代码编码。 */
	.section ".text32","ax"		/* 32 位入口代码放入可执行 .text32 段。 */
SYM_FUNC_START_LOCAL_NOALIGN(.Lin_pm32)	/* 定义 32 位保护模式落点。 */
	# Set up data segments for flat 32-bit mode
	movl	%ecx, %ds		/* 设置 DS 为平坦数据段。 */
	movl	%ecx, %es		/* 设置 ES 为平坦数据段。 */
	movl	%ecx, %fs		/* 设置 FS 为平坦数据段。 */
	movl	%ecx, %gs		/* 设置 GS 为平坦数据段。 */
	movl	%ecx, %ss		/* 设置 SS 为平坦数据段。 */
	# The 32-bit code sets up its own stack, but this way we do have
	# a valid stack if some debugging hack wants to use it.
	addl	%ebx, %esp		/* 将实模式栈指针调整为线性地址，保留可用调试栈。 */

	# Set up TR to make Intel VT happy
	ltr	%di			/* 加载任务寄存器，满足 Intel VT 对 TR 的要求。 */

	# Clear registers to allow for future extensions to the
	# 32-bit boot protocol
	xorl	%ecx, %ecx		/* 清零 ECX，留给未来启动协议扩展。 */
	xorl	%edx, %edx		/* 清零 EDX，避免泄漏旧参数。 */
	xorl	%ebx, %ebx		/* 清零 EBX，避免泄漏旧基址。 */
	xorl	%ebp, %ebp		/* 清零 EBP，建立干净的初始状态。 */
	xorl	%edi, %edi		/* 清零 EDI，建立干净的初始状态。 */

	# Set up LDTR to make Intel VT happy
	lldt	%cx			/* 用 0 加载 LDTR，清空本地描述符表。 */

	jmpl	*%eax			# Jump to the 32-bit entrypoint /* 跳转到传入的 32 位内核入口。 */
SYM_FUNC_END(.Lin_pm32)		/* 标记 32 位落点结束。 */
