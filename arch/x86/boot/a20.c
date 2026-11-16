// SPDX-License-Identifier: GPL-2.0-only
/* -*- linux-c -*- ------------------------------------------------------- *
 *
 *   Copyright (C) 1991, 1992 Linus Torvalds
 *   Copyright 2007-2008 rPath, Inc. - All Rights Reserved
 *   Copyright 2009 Intel Corporation; author H. Peter Anvin
 *
 * ----------------------------------------------------------------------- */

/*
 * Enable A20 gate (return -1 on failure)
 *
 * 打开 A20 地址线，成功返回 0，失败返回 -1。
 * A20 未打开时，1MiB 以上地址会回绕到低端内存，进入保护模式前必须确保它已启用。
 */

#include "boot.h"
#include "linux/types.h"

#define MAX_8042_LOOPS 100000
#define MAX_8042_FF 32

static int empty_8042 (void)
{
    u8  status;
    int loops = MAX_8042_LOOPS;
    int ffs   = MAX_8042_FF;

    /* 等待 8042 键盘控制器输入/输出缓冲区清空。 */
    while (loops--) {
        io_delay ();

        status = inb (0x64);
        if (status == 0xff) {
            /* FF is a plausible, but very unlikely status */
            /* 0xff 虽然理论上可能出现，但连续出现通常表示没有 KBC。 */
            if (!--ffs)
                return -1; /* Assume no KBC present */
        }
        if (status & 1) {
            /* Read and discard input data */
            /* 输出缓冲区有数据时读出并丢弃，避免阻塞后续命令。 */
            io_delay ();
            (void)inb (0x60);
        } else if (!(status & 2)) {
            /* Buffers empty, finished! */
            /* 输入缓冲区也为空，说明控制器可以接受新命令。 */
            return 0;
        }
    }

    return -1;
}

/* Returns nonzero if the A20 line is enabled.  The memory address
   used as a test is the int $0x80 vector, which should be safe. */
/* 检测 A20 是否已经打开。测试地址选用 int $0x80 向量位置，通常不会被这里破坏。 */

#define A20_TEST_ADDR (4 * 0x80)
#define A20_TEST_SHORT 32
#define A20_TEST_LONG 2097152 /* 2^21 */

static int a20_test (int loops)
{
    int ok = 0;
    int saved, ctr;

    /* fs 指向 0x0000，gs 指向 0xffff，用于构造 A20 回绕检测地址。 */
    set_fs (0x0000);
    set_gs (0xffff);

    /* 保存原值，测试结束后恢复。 */
    saved = ctr = rdfs32 (A20_TEST_ADDR);

    while (loops--) {
        /* 写低端地址，再从可能回绕到同一物理地址的高端别名读取。 */
        wrfs32 (++ctr, A20_TEST_ADDR);
        io_delay (); /* Serialize and make delay constant */
        ok = rdgs32 (A20_TEST_ADDR + 0x10) ^ ctr;
        /* 读出的值不同，表示高端地址没有回绕，A20 已经打开。 */
        if (ok)
            break;
    }

    wrfs32 (saved, A20_TEST_ADDR);
    return ok;
}

/* Quick test to see if A20 is already enabled */
/* 快速检测 A20 是否已经打开。 */
static int a20_test_short (void)
{
    return a20_test (A20_TEST_SHORT);
}

/* Longer test that actually waits for A20 to come on line; this
   is useful when dealing with the KBC or other slow external circuitry. */
/* 长时间检测 A20 是否打开，适用于 KBC 等响应较慢的外部电路。 */
static int a20_test_long (void)
{
    return a20_test (A20_TEST_LONG);
}

static void enable_a20_bios (void)
{
    struct biosregs ireg;

    /* 通过 BIOS INT 15h, AX=2401h 请求开启 A20。 */
    initregs (&ireg);
    ireg.ax = 0x2401;
    intcall (0x15, &ireg, NULL);
}

static void enable_a20_kbc (void)
{
    /* 通过传统 8042 键盘控制器输出端口开启 A20。 */
    empty_8042 ();

    outb (0xd1, 0x64); /* Command write */
    /* 0xd1 表示下一字节写入 8042 输出端口。 */
    empty_8042 ();

    outb (0xdf, 0x60); /* A20 on */
    /* 0xdf 置位 A20，同时保持系统不复位。 */
    empty_8042 ();

    outb (0xff, 0x64); /* Null command, but UHCI wants it */
    /* 空命令用于兼容部分 UHCI/旧硬件行为。 */
    empty_8042 ();
}

static void enable_a20_fast (void)
{
    u8 port_a;

    /* 通过系统控制端口 A(0x92) 快速开启 A20。 */
    port_a = inb (0x92); /* Configuration port A */
    port_a |= 0x02;      /* Enable A20 */
    port_a &= ~0x01;     /* Do not reset machine */
    outb (port_a, 0x92);
}

/*
 * Actual routine to enable A20; return 0 on ok, -1 on failure
 *
 * 实际执行 A20 开启流程: 先检测现状，再依次尝试 BIOS、KBC 和 fast A20 gate。
 */

#define A20_ENABLE_LOOPS 255 /* Number of times to try */

int enable_a20 (void)
{
    int loops = A20_ENABLE_LOOPS;
    int kbc_err;

    /* 多次重试，兼容 BIOS/KBC 响应慢或第一次操作未生效的机器。 */
    while (loops--) {
        /* First, check to see if A20 is already enabled
          (legacy free, etc.) */
        /* 新平台可能已经启用 A20，先快速检测。 */
        if (a20_test_short ())
            return 0;

        /* Next, try the BIOS (INT 0x15, AX=0x2401) */
        /* 优先使用 BIOS 接口，这是最标准的开启方式。 */
        enable_a20_bios ();
        if (a20_test_short ())
            return 0;

        /* Try enabling A20 through the keyboard controller */
        /* BIOS 不可靠时，退回到传统键盘控制器方式。 */
        kbc_err = empty_8042 ();

        if (a20_test_short ())
            return 0; /* BIOS worked, but with delayed reaction */

        if (!kbc_err) {
            /* 只有确认 KBC 存在并且可用时才发送 A20 命令。 */
            enable_a20_kbc ();
            if (a20_test_long ())
                return 0;
        }

        /* Finally, try enabling the "fast A20 gate" */
        /* 最后尝试 0x92 端口的快速 A20 gate。 */
        enable_a20_fast ();
        if (a20_test_long ())
            return 0;
    }

    return -1;
}
