// SPDX-License-Identifier: GPL-2.0-only
/* -*- linux-c -*- ------------------------------------------------------- *
 *
 *   Copyright (C) 1991, 1992 Linus Torvalds
 *   Copyright 2007 rPath, Inc. - All Rights Reserved
 *   Copyright 2009 Intel Corporation; author H. Peter Anvin
 *
 * ----------------------------------------------------------------------- */

/*
 * Main module for the real-mode kernel code
 */
#include <linux/build_bug.h>

#include "boot.h"
#include "string.h"

struct boot_params boot_params __attribute__((aligned(16)));

struct port_io_ops pio_ops;

char *HEAP = _end;
char *heap_end = _end;		/* Default end of heap = no heap */

/**
 * Copy the header into the boot parameter block.  Since this
 * screws up the old-style command line protocol, adjust by
 * filling in the new-style command line pointer instead.
 *
 * 复制引导头和命令行参数：
 *	旧版命令行参数使用特定的结构体(如: old_cmdline)来存储参数
 *	新版命令行参数直接通过boot_params.hdr.cmd_line_ptr传递，以字符串形式存储
 *
 *	boot_params.hdr
 *	boot_params.hdr.cmd_line_ptr
 */
static void copy_boot_params(void)
{
	struct old_cmdline {
		u16 cl_magic;
		u16 cl_offset;
	};
	const struct old_cmdline * const oldcmd = absolute_pointer(OLD_CL_ADDRESS);

	/**
	 * <root>/include/linux/build_bug.h
	 * 条件不满足输出编译错误: compiletime_assert(cond, msg)
	 * 在 <root>/tools/include/linux/compoler.h 中定义: compiletime_assert()
	 */
	BUILD_BUG_ON(sizeof(boot_params) != 4096);
	memcpy(&boot_params.hdr, &hdr, sizeof(hdr));

	/**
	 * 检查和处理引导程序(如:GRUB)传递过来的命令行参数
	 *
	 * boot_params 结构体，用于存储从引导加载程序传递过来的启动参数
	 * hdr是boot_params中的一个子结构体，通常表示引导头(boot header)
	 * cmd_line_ptr 是指向内核命令行参数的指针。它是一个物理地址，指向引导加载程序传递给内核的命令行字符串
	 *
	 * oldcmd 是一个指向旧版命令行参数结构的指针
	 * cl_magic 是该结构中的一个字段，通常用于标识该结构的类型或版本
	 * OLD_CL_MAGIC 是一个预定义的常量，用于验证oldcmd是否是合法的旧版本命令行参数结构
	 */
	if (!boot_params.hdr.cmd_line_ptr && oldcmd->cl_magic == OLD_CL_MAGIC) {
		/* Old-style command line protocol */
		u16 cmdline_seg;

		/*
		 * Figure out if the command line falls in the region
		 * of memory that an old kernel would have copied up
		 * to 0x90000...
		 */
		if (oldcmd->cl_offset < boot_params.hdr.setup_move_size) {
			cmdline_seg = ds();
		}
		else {
			cmdline_seg = 0x9000;
		}

		boot_params.hdr.cmd_line_ptr = (cmdline_seg << 4) + oldcmd->cl_offset;
	}
}

/*
 * Query the keyboard lock status as given by the BIOS, and
 * set the keyboard repeat rate to maximum.  Unclear why the latter
 * is done here; this might be possible to kill off as stale code.
 */
static void keyboard_init(void)
{
	struct biosregs ireg, oreg;

	initregs(&ireg);

	ireg.ah = 0x02;		/* Get keyboard status */
	intcall(0x16, &ireg, &oreg);
	boot_params.kbd_status = oreg.al;

	ireg.ax = 0x0305;	/* Set keyboard repeat rate */
	intcall(0x16, &ireg, NULL);
}

/*
 * Get Intel SpeedStep (IST) information.
 */
static void query_ist(void)
{
	struct biosregs ireg, oreg;

	/*
	 * Some older BIOSes apparently crash on this call, so filter
	 * it from machines too old to have SpeedStep at all.
	 */
	if (cpu.level < 6)
		return;

	initregs(&ireg);
	ireg.ax  = 0xe980;	 /* IST Support */
	ireg.edx = 0x47534943;	 /* Request value */
	intcall(0x15, &ireg, &oreg);

	boot_params.ist_info.signature  = oreg.eax;
	boot_params.ist_info.command    = oreg.ebx;
	boot_params.ist_info.event      = oreg.ecx;
	boot_params.ist_info.perf_level = oreg.edx;
}

/*
 * Tell the BIOS what CPU mode we intend to run in.
 */
static void set_bios_mode(void)
{
#ifdef CONFIG_X86_64
	struct biosregs ireg;

	initregs(&ireg);
	ireg.ax = 0xec00;
	ireg.bx = 2;
	intcall(0x15, &ireg, NULL);
#endif
}

static void init_heap(void)
{
	char *stack_end;

	if (boot_params.hdr.loadflags & CAN_USE_HEAP) {
		stack_end = (char *) (current_stack_pointer - STACK_SIZE);
		heap_end = (char *) ((size_t)boot_params.hdr.heap_end_ptr + 0x200);
		if (heap_end > stack_end)
			heap_end = stack_end;
	} else {
		/* Boot protocol 2.00 only, no heap available */
		puts("WARNING: Ancient bootloader, some functionality may be limited!\n");
	}
}

void main(void)
{
	init_default_io_ops();			// io.h --> asm/shared/io.h 中, 定义 inb、outb、outw 三个IO操作, 封装了 in out outw 汇编指令

	/* First, copy the boot header into the "zeropage" */
	copy_boot_params();				// 复制GRUB传入的命令行参数

	/* Initialize the early-boot console */
	console_init();					// <src>/arch/x86/boot/early_serial_console.c, 根据命令行 earlyprintk, serial, ttyS 或 console, uart8250,uart,io 配置输出日志
	if (cmdline_find_option_bool("debug"))
		puts("early console in setup code\n");

	/* End of heap check */
	init_heap();					//

	/* Make sure we have all the proper CPU support */
	if (validate_cpu()) {			// <src>/arch/x86/boot/cpu.c
		puts("Unable to boot - please use a kernel appropriate for your CPU.\n");
		die();
	}

	/* Tell the BIOS what CPU mode we intend to run in */
	set_bios_mode();

	/* Detect memory layout */
	detect_memory();

	/* Set keyboard repeat rate (why?) and query the lock flags */
	keyboard_init();

	/* Query Intel SpeedStep (IST) information */
	query_ist();

	/* Query APM information */
#if defined(CONFIG_APM) || defined(CONFIG_APM_MODULE)
	query_apm_bios();
#endif

	/* Query EDD information */
#if defined(CONFIG_EDD) || defined(CONFIG_EDD_MODULE)
	query_edd();
#endif

	/* Set the video mode */
	set_video();

	/* Do the last things and invoke protected mode */
	go_to_protected_mode();
}
