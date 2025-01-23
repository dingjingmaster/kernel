/* SPDX-License-Identifier: GPL-2.0-or-later */
/* -----------------------------------------------------------------------
 *
 *   Copyright 2009-2014 Intel Corporation; author H. Peter Anvin
 *
 * ----------------------------------------------------------------------- */

/*
 * "Glove box" for BIOS calls.  Avoids the constant problems with BIOSes
 * touching registers they shouldn't be.
 */
/**
 *	struct biosregs {
 *		union {
 *			struct {
 *				u32 edi;
 *              u32 esi;
 *              u32 ebp;
 *              u32 _esp;
 *              u32 ebx;
 *              u32 edx;
 *              u32 ecx;
 *              u32 eax;
 *              u32 _fsgs;
 *              u32 _dses;
 *              u32 eflags;
 *          };
 *          struct {
 *              u16 di, hdi;
 *              u16 si, hsi;
 *              u16 bp, hbp;
 *              u16 _sp, _hsp;
 *              u16 bx, hbx;
 *              u16 dx, hdx;
 *              u16 cx, hcx;
 *              u16 ax, hax;
 *              u16 gs, fs;
 *              u16 es, ds;
 *              u16 flags, hflags;
 *          };
 *          struct {
 *              u8 dil, dih, edi2, edi3;
 *              u8 sil, sih, esi2, esi3;
 *              u8 bpl, bph, ebp2, ebp3;
 *              u8 _spl, _sph, _esp2, _esp3;
 *              u8 bl, bh, ebx2, ebx3;
 *              u8 dl, dh, edx2, edx3;
 *              u8 cl, ch, ecx2, ecx3;
 *              u8 al, ah, eax2, eax3;
 *          };
 *      };
 *  };
 */
/**
 * 实现了函数：
 *	void intcall(u8 int_no, const struct biosregs* ireg, struct biosregs* oreg);
 *  C 调用 intcall 时候 EAX、EDX和ECX分别是 int_no、ireg和oreg
 */

.code16												/* 告诉汇编器 运行在 16 位模式 */
.section ".inittext","ax"							/* .inittext 存放初始化代码。内核中的初始化代码通常在系统启动完成后就会被释放以节省内存; a: 表示这个段可执行, x: 表示这个段包含可执行指令, ax: 说明这个段是一个可加载且可执行的段 */
	.globl	intcall									/* 声明全局符号: intcall */
	.type	intcall, @function						/* 指示符号: intcall 类型位函数, @object表示符号是一个数据对象(如全局变量) */
intcall:
	/* Self-modify the INT instruction.  Ugly, but works. */
	cmpb	%al, 3f									/* 比较 寄存器A低8位 与 3标签 指向内存位置的值, 注意: 3f中的f表示标签在代码之前 */
	je	1f											/* 上条指令结果相等则跳转到 标签1 处 */
	movb	%al, 3f									/* 把 int_no 参数的值写入 */
	jmp	1f		/* Synchronize pipeline */
1:
	/* Save state */
	pushfl											/* EFLAGS 寄存器的内容压栈 */
	pushw	%fs										/* FS 段寄存器数据压栈 */
	pushw	%gs										/* GS 段寄存器数据压栈 */
	pushal											/* 将所有通用寄存器一次性压栈: EAX ECX EDX EBX ESP EBP ESI EDI */

	/* Copy input state to stack frame */
	subw	$44, %sp								/* 栈指针 SP 下移 44 个字节，分配新的栈空间, 容纳 ireg */
	movw	%dx, %si								/* dx 的值复制到 %si, 指定源地址, DX = ireg 参数 */
	movw	%sp, %di								/* %sp 的值存入 %di, 指定目标地址(栈的内存区域) */
	movw	$11, %cx								/* 循环复制 11 次, 把数据复制到 SP 区域 */
	rep; movsl										/* 每次移动4字节(双字, 32位), 正好移动44字节, 把 ireg 内容写到栈[%sp, $44(%sp)] */

	/* Pop full state from the stack */				/* 开始把栈内 ireg 内容装入到各自寄存器中 */
	popal
	popw	%gs
	popw	%fs
	popw	%es
	popw	%ds
	popfl

	/* Actual INT */
	.byte	0xcd									/* INT opcode, 对应十进制 205, 对应指令：INT imm8, 是一条中断指令，用于触发软中断。 */
3:	.byte	0										/* 上条指令 INT 的操作码, 0 一般在BIOS中断中表示执行 键盘输入检查 */

	/* Push full state to the stack */				/* intcall后的结果按照oreg的格式保存到栈中 */
	pushfl
	pushw	%ds
	pushw	%es
	pushw	%fs
	pushw	%gs
	pushal

	/* Re-establish C environment invariants */		/* intcall 调用这是C程序 */
	cld												/* 清除方向寄存器 DF, DF = 0: 内存操作的地址自动增加; DF = 1: 内存操作的地址自动减少, cld 将DF置0 */
	movzwl	%sp, %esp								/* 从内存中读取一个32位值，并将其扩展到寄存器，将sp复制到esp，并扩展为32位 */
	movw	%cs, %ax								/* 代码段基址 --> AX */
	movw	%ax, %ds								/* 代码段基址 --> DS */
	movw	%ax, %es								/* 代码段基址 --> ES */

	/* Copy output state from stack frame */
	movw	68(%esp), %di							/* Original %cx == 3rd argument */
	andw	%di, %di
	jz	4f
	movw	%sp, %si
	movw	$11, %cx
	rep; movsl										/* 栈中的oreg内容写入到caller的oreg地址 */
4:	addw	$44, %sp								/* 退出栈中的 oreg 内容 */

	/* Restore state and return */					/* 退出前复原 */
	popal
	popw	%gs
	popw	%fs
	popfl
	retl
	.size	intcall, .-intcall						/* 设置 intcall 函数的大小 */
