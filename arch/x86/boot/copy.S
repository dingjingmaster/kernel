/* SPDX-License-Identifier: GPL-2.0-only */
/* ----------------------------------------------------------------------- *
 *
 *   Copyright (C) 1991, 1992 Linus Torvalds
 *   Copyright 2007 rPath, Inc. - All Rights Reserved
 *
 * ----------------------------------------------------------------------- */

#include <linux/linkage.h>

/*
 * Memory copy routines
 *
 * 实模式启动阶段使用的内存复制/填充例程。
 * 这些函数采用 boot 代码自己的调用约定，返回时使用 retl 回到调用点。
 */

	.code16				/* 告诉汇编器按 16 位代码生成指令。 */
	.text				/* 后续代码放入普通文本段。 */

SYM_FUNC_START_NOALIGN(memcpy)		/* 定义 memcpy: AX=目标地址，DX=源地址，CX=字节数。 */
	pushw	%si			/* 保存调用者的 SI。 */
	pushw	%di			/* 保存调用者的 DI。 */
	movw	%ax, %di		/* DI 指向目标地址。 */
	movw	%dx, %si		/* SI 指向源地址。 */
	pushw	%cx			/* 保存原始字节数，用于稍后处理尾部字节。 */
	shrw	$2, %cx		/* 先按 4 字节双字计算复制次数。 */
	rep; movsl			/* 重复复制 ECX 个双字，从 DS:SI 到 ES:DI。 */
	popw	%cx			/* 恢复原始字节数。 */
	andw	$3, %cx		/* 只保留除以 4 后的余数字节数。 */
	rep; movsb			/* 复制剩余的 0 到 3 个字节。 */
	popw	%di			/* 恢复调用者的 DI。 */
	popw	%si			/* 恢复调用者的 SI。 */
	retl				/* 返回 C/启动代码调用者。 */
SYM_FUNC_END(memcpy)			/* 标记 memcpy 符号结束。 */

SYM_FUNC_START_NOALIGN(memset)		/* 定义 memset: AX=目标地址，DL=填充值，CX=字节数。 */
	pushw	%di			/* 保存调用者的 DI。 */
	movw	%ax, %di		/* DI 指向目标地址。 */
	movzbl	%dl, %eax		/* 将 8 位填充值零扩展到 EAX。 */
	imull	$0x01010101,%eax	/* 把填充值复制到 EAX 的每个字节中。 */
	pushw	%cx			/* 保存原始字节数。 */
	shrw	$2, %cx		/* 先按 4 字节双字计算填充次数。 */
	rep; stosl			/* 重复写入 EAX 到 ES:DI。 */
	popw	%cx			/* 恢复原始字节数。 */
	andw	$3, %cx		/* 只保留尾部剩余字节数。 */
	rep; stosb			/* 写入剩余的 0 到 3 个字节。 */
	popw	%di			/* 恢复调用者的 DI。 */
	retl				/* 返回 C/启动代码调用者。 */
SYM_FUNC_END(memset)			/* 标记 memset 符号结束。 */

SYM_FUNC_START_NOALIGN(copy_from_fs)	/* 定义 copy_from_fs: 从 FS 段复制到当前 ES 段。 */
	pushw	%ds			/* 保存原 DS。 */
	pushw	%fs			/* 将 FS 压栈，准备作为新的 DS。 */
	popw	%ds			/* DS=FS，使 memcpy 从 FS:SI 读取。 */
	calll	memcpy			/* 复用 memcpy 完成实际复制。 */
	popw	%ds			/* 恢复原 DS。 */
	retl				/* 返回调用者。 */
SYM_FUNC_END(copy_from_fs)		/* 标记 copy_from_fs 符号结束。 */

SYM_FUNC_START_NOALIGN(copy_to_fs)	/* 定义 copy_to_fs: 从当前 DS 段复制到 FS 段。 */
	pushw	%es			/* 保存原 ES。 */
	pushw	%fs			/* 将 FS 压栈，准备作为新的 ES。 */
	popw	%es			/* ES=FS，使 memcpy 写入 FS:DI。 */
	calll	memcpy			/* 复用 memcpy 完成实际复制。 */
	popw	%es			/* 恢复原 ES。 */
	retl				/* 返回调用者。 */
SYM_FUNC_END(copy_to_fs)		/* 标记 copy_to_fs 符号结束。 */
