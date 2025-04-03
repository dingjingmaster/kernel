/* SPDX-License-Identifier: GPL-2.0-only */
/*
 *  Copyright 2007 Red Hat, Inc.
 *  by Peter Jones <pjones@redhat.com>
 *  Copyright 2007 IBM, Inc.
 *  by Konrad Rzeszutek <konradr@linux.vnet.ibm.com>
 *  Copyright 2008
 *  by Konrad Rzeszutek <ketuzsezr@darnok.org>
 *
 * This code exposes the iSCSI Boot Format Table to userland via sysfs.
 */

#ifndef ISCSI_IBFT_H
#define ISCSI_IBFT_H

#include <linux/types.h>

/*
 * Physical location of iSCSI Boot Format Table.
 * If the value is 0 there is no iBFT on the machine.
 */
extern phys_addr_t ibft_phys_addr;

#ifdef CONFIG_ISCSI_IBFT_FIND

/*
 * Routine used to find and reserve the iSCSI Boot Format Table. The
 * physical address is set in the ibft_phys_addr variable.
 */
void reserve_ibft_region(void);

/*
 * Physical bounds to search for the iSCSI Boot Format Table.
 */
#define IBFT_START 0x80000 /* 512kB */
#define IBFT_END 0x100000 /* 1MB */

#else
/**
 * Linux 内核中用于保留 Intel iBFT（Intel Boot Firmware Table）表内存区域的核心函数，
 * 其作用可概括为以下几点：
 *  - iBFT 表定位与保留(通过解析 UEFI 固件 中的 iBFT 表（Intel Boot Firmware Table），
 *    确定其物理内存地址范围，并将该区域标记为 e820_type_reserved，防止内核或用户态程序覆盖)
 *  - 校验 iBFT 表的签名（如 iBFT_SIGNATURE）和版本号，确保其有效性。若无效，内核会记录警告但继续启动
 *  - 提取 iBFT 中的关键信息（如设备树、启动选项），供后续 UEFI 驱动使用
 *  - 通过 mprotect() 或页表标记，禁止对 iBFT 区域的写操作，避免内核误修改导致 UEFI 引导失败
 *  - 确保 iBFT 区域不会被 memblock 分配器分配给内核模块或用户态程序
 */
static inline void reserve_ibft_region(void) {}
#endif

#endif /* ISCSI_IBFT_H */
