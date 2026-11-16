/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * vsyscall_emu_64.S: Vsyscall emulation page
 *
 * 固定地址 vsyscall 模拟页，保留旧 ABI 所需的三个入口槽位。
 *
 * Copyright (c) 2011 Andy Lutomirski
 */

#include <linux/linkage.h>

#include <asm/irq_vectors.h>
#include <asm/page_types.h>
#include <asm/unistd_64.h>

__PAGE_ALIGNED_DATA                    /* 后续数据按页对齐放置。 */
        .globl __vsyscall_page         /* 导出 __vsyscall_page 符号供 C 代码映射。 */
        .balign PAGE_SIZE, 0xcc        /* 对齐到页边界，空隙用 int3 填充。 */
        .type __vsyscall_page, @object /* 标记该符号为对象而非函数。 */
__vsyscall_page:                       /* vsyscall 固定页起始地址。 */

        mov $__NR_gettimeofday, %rax   /* 第 0 槽: 准备 gettimeofday 系统调用号。 */
        syscall                        /* 进入内核执行 gettimeofday。 */
        ret                            /* 按旧 vsyscall ABI 返回调用者。 */
        int3                           /* 防止顺序落入填充区。 */

        .balign 1024, 0xcc             /* 第 1 槽对齐到 1024 字节边界。 */
        mov $__NR_time, %rax           /* 第 1 槽: 准备 time 系统调用号。 */
        syscall                        /* 进入内核执行 time。 */
        ret                            /* 按旧 vsyscall ABI 返回调用者。 */
        int3                           /* 防止顺序落入填充区。 */

        .balign 1024, 0xcc             /* 第 2 槽对齐到 1024 字节边界。 */
        mov $__NR_getcpu, %rax         /* 第 2 槽: 准备 getcpu 系统调用号。 */
        syscall                        /* 进入内核执行 getcpu。 */
        ret                            /* 按旧 vsyscall ABI 返回调用者。 */
        int3                           /* 防止顺序落入填充区。 */

        .balign 4096, 0xcc             /* 将整个模拟页填充到 4K 大小。 */

        .size __vsyscall_page, 4096    /* 声明 __vsyscall_page 对象大小为一页。 */
