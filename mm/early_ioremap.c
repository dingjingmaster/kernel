// SPDX-License-Identifier: GPL-2.0
/*
 * Provide common bits of early_ioremap() support for architectures needing
 * temporary mappings during boot before ioremap() is available.
 *
 * This is mostly a direct copy of the x86 early_ioremap implementation.
 *
 * (C) Copyright 1995 1996, 2014 Linus Torvalds
 *
 */
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/vmalloc.h>
#include <asm/fixmap.h>
#include <asm/early_ioremap.h>
#include "internal.h"

#include <asm/mem_encrypt.h>

#ifdef CONFIG_MMU
static int early_ioremap_debug __initdata;

static int __init              early_ioremap_debug_setup (char* str)
{
    early_ioremap_debug = 1;

    return 0;
}
early_param ("early_ioremap_debug", early_ioremap_debug_setup);

/**
 * @brief Linux内核中用于指示分页机制是否已经完成初始化。
 */
static int after_paging_init __initdata;

/**
 * @brief 主要用于调整早期内存映射的页表权限，确保正确的访问权限。
 *  在早期启动阶段，Linux需要对物理内存进行临时映射，以访问 ACPI 表、EFI 数据、设备树等，
 *  而 此函数 允许在这些映射创建之前调整其页面保护属性(pgprot_t)。
 *  权限有：可读、可写、可执行
 * @param phys_addr
 * @param size
 * @param prot
 * @return
 */
pgprot_t __init __weak       early_memremap_pgprot_adjust (resource_size_t phys_addr,
                                                           unsigned long   size,
                                                           pgprot_t        prot)
{
    return prot;
}

void __init early_ioremap_reset (void)
{
    after_paging_init = 1;
}

/*
 * Generally, ioremap() is available after paging_init() has been called.
 * Architectures wanting to allow early_ioremap after paging_init() can
 * define __late_set_fixmap and __late_clear_fixmap to do the right thing.
 */
#    ifndef __late_set_fixmap
static inline void __init __late_set_fixmap (enum fixed_addresses idx,
                                             phys_addr_t          phys,
                                             pgprot_t             prot)
{
    BUG ();
}
#    endif

#    ifndef __late_clear_fixmap
static inline void __init __late_clear_fixmap (enum fixed_addresses idx)
{
    BUG ();
}
#    endif

/**
 * @brief prev_map：用于存储上一次映射地址，它的作用是优化 early ioremap 机制，避免重复映射。
 * 这个变量一般在 early_ioremap() 相关代码中出现，主要用于在 fixmap 机制下进行临时映射物理地址。
 * 避免 early_ioremap() 频繁分配新的fixmap位置
 */
static void __iomem* prev_map[FIX_BTMAPS_SLOTS] __initdata;
static unsigned long prev_size[FIX_BTMAPS_SLOTS] __initdata;

/**
 * @brief slot_virt 用于管理虚拟地址空间映射。
 */
static unsigned long slot_virt[FIX_BTMAPS_SLOTS] __initdata;

void __init          early_ioremap_setup (void)
{
    int i;

    for (i = 0; i < FIX_BTMAPS_SLOTS; i++) {
        WARN_ON_ONCE (prev_map[i]);
        slot_virt[i] = __fix_to_virt (FIX_BTMAP_BEGIN - NR_FIX_BTMAPS * i);
    }
}

static int __init check_early_ioremap_leak (void)
{
    int count = 0;
    int i;

    for (i = 0; i < FIX_BTMAPS_SLOTS; i++)
        if (prev_map[i])
            count++;

    if (WARN (count,
              KERN_WARNING "Debug warning: early ioremap leak of %d areas detected.\n"
                           "please boot with early_ioremap_debug and report the dmesg.\n",
              count))
        return 1;
    return 0;
}
late_initcall (check_early_ioremap_leak);

/**
 * @brief 内核在早期引导阶段使用的一个函数，用于临时映射物理地址到虚拟地址空间，
 * 以便访问硬件资源(如：EFI 表、ACPI 表、设备树等)
 * 在 ioremap 机制尚未完全初始化之前，某些关键的硬件数据需要被读取，因此 __early_ioremap 允许在内存
 * 管理子系统初始化前进行物理地址到虚拟地址的映射。
 *
 * @param phys_addr 要映射的物理地址(通常是：BIOS/EFI/ACPI)
 * @param size 要映射的内存大小
 * @param prot 页表权限(PAGE_KERNEL, PAGE_KERNEL_NOCACHE)
 * @return 返回映射的虚拟地址(__iomem* 类型)
 */
static void __init __iomem*
__early_ioremap (resource_size_t phys_addr, unsigned long size, pgprot_t prot)
{
    unsigned long        offset;
    resource_size_t      last_addr;
    unsigned int         nrpages;
    enum fixed_addresses idx;
    int                  i, slot;

    WARN_ON (system_state >= SYSTEM_RUNNING);

    slot = -1; // 要分配的 fixmap 位置
    for (i = 0; i < FIX_BTMAPS_SLOTS; i++) {
        if (!prev_map[i]) {
            slot = i;
            break;
        }
    }

    if (WARN (slot < 0, "%s(%pa, %08lx) not found slot\n", __func__, &phys_addr, size)) {
        return NULL;
    }

    /* Don't allow wraparound or zero size */
    last_addr = phys_addr + size - 1;
    if (WARN_ON (!size || last_addr < phys_addr)) {
        return NULL;
    }

    prev_size[slot] = size;
    /**
     * @brief
     * Mappings have to be page-aligned
     * 映射必须按页面对齐
     *
     * ((unsigned long)(p) & ~PAGE_MASK)
     */
    offset          = offset_in_page (phys_addr);
    phys_addr &= PAGE_MASK;
    size    = PAGE_ALIGN (last_addr + 1) - phys_addr;

    /**
     * Mappings have to fit in the FIX_BTMAP area.
     */
    nrpages = size >> PAGE_SHIFT;
    /**
     * @brief FIX 内存区域 256K
     * fixmap 区域的地址范围通常是：FIXADDR_TOP - 256KB，
     * 主要用于临时映射ACPI表、EFI数据、APIC、调试控制台等。
     */
    if (WARN_ON (nrpages > NR_FIX_BTMAPS)) {
        return NULL;
    }

    /*
     * Ok, go for it..
     */
    idx = FIX_BTMAP_BEGIN - NR_FIX_BTMAPS * slot;
    while (nrpages > 0) {
        if (after_paging_init) {
            // 分页机制初始化完成
            __late_set_fixmap (idx, phys_addr, prot);
        }
        else {
            // 未完成分页机制
            __early_set_fixmap (idx, phys_addr, prot);
        }
        phys_addr += PAGE_SIZE;
        --idx;
        --nrpages;
    }
    WARN (early_ioremap_debug, "%s(%pa, %08lx) [%d] => %08lx + %08lx\n", __func__, &phys_addr, size,
          slot, offset, slot_virt[slot]);

    prev_map[slot] = (void __iomem*)(offset + slot_virt[slot]);

    return prev_map[slot];
}

void __init early_iounmap (void __iomem* addr, unsigned long size)
{
    unsigned long        virt_addr;
    unsigned long        offset;
    unsigned int         nrpages;
    enum fixed_addresses idx;
    int                  i, slot;

    slot = -1;
    for (i = 0; i < FIX_BTMAPS_SLOTS; i++) {
        if (prev_map[i] == addr) {
            slot = i;
            break;
        }
    }

    if (WARN (slot < 0, "%s(%p, %08lx) not found slot\n", __func__, addr, size))
        return;

    if (WARN (prev_size[slot] != size, "%s(%p, %08lx) [%d] size not consistent %08lx\n", __func__,
              addr, size, slot, prev_size[slot]))
        return;

    WARN (early_ioremap_debug, "%s(%p, %08lx) [%d]\n", __func__, addr, size, slot);

    virt_addr = (unsigned long)addr;
    if (WARN_ON (virt_addr < fix_to_virt (FIX_BTMAP_BEGIN)))
        return;

    offset  = offset_in_page (virt_addr);
    nrpages = PAGE_ALIGN (offset + size) >> PAGE_SHIFT;

    idx     = FIX_BTMAP_BEGIN - NR_FIX_BTMAPS * slot;
    while (nrpages > 0) {
        if (after_paging_init) {
            __late_clear_fixmap (idx);
        }
        else {
            __early_set_fixmap (idx, 0, FIXMAP_PAGE_CLEAR);
        }
        --idx;
        --nrpages;
    }
    prev_map[slot] = NULL;
}

/* Remap an IO device */
void __init __iomem* early_ioremap (resource_size_t phys_addr, unsigned long size)
{
    return __early_ioremap (phys_addr, size, FIXMAP_PAGE_IO);
}

/* Remap memory */
void __init* early_memremap (resource_size_t phys_addr, unsigned long size)
{
    pgprot_t prot = early_memremap_pgprot_adjust (phys_addr, size, FIXMAP_PAGE_NORMAL);

    return (__force void*)__early_ioremap (phys_addr, size, prot);
}
#    ifdef FIXMAP_PAGE_RO
void __init* early_memremap_ro (resource_size_t phys_addr, unsigned long size)
{
    pgprot_t prot = early_memremap_pgprot_adjust (phys_addr, size, FIXMAP_PAGE_RO);

    return (__force void*)__early_ioremap (phys_addr, size, prot);
}
#    endif

#    ifdef CONFIG_ARCH_USE_MEMREMAP_PROT
void __init*
early_memremap_prot (resource_size_t phys_addr, unsigned long size, unsigned long prot_val)
{
    return (__force void*)__early_ioremap (phys_addr, size, __pgprot (prot_val));
}
#    endif

#    define MAX_MAP_CHUNK (NR_FIX_BTMAPS << PAGE_SHIFT)

void __init copy_from_early_mem (void* dest, phys_addr_t src, unsigned long size)
{
    unsigned long slop, clen;
    char*         p;

    while (size) {
        slop = offset_in_page (src);
        clen = size;
        if (clen > MAX_MAP_CHUNK - slop)
            clen = MAX_MAP_CHUNK - slop;
        p = early_memremap (src & PAGE_MASK, clen + slop);
        memcpy (dest, p + slop, clen);
        early_memunmap (p, clen + slop);
        dest += clen;
        src += clen;
        size -= clen;
    }
}

#else  /* CONFIG_MMU */

void __init __iomem* early_ioremap (resource_size_t phys_addr, unsigned long size)
{
    return (__force void __iomem*)phys_addr;
}

/* Remap memory */
void __init* early_memremap (resource_size_t phys_addr, unsigned long size)
{
    return (void*)phys_addr;
}
void __init* early_memremap_ro (resource_size_t phys_addr, unsigned long size)
{
    return (void*)phys_addr;
}

void __init early_iounmap (void __iomem* addr, unsigned long size)
{
}

#endif /* CONFIG_MMU */

void __init early_memunmap (void* addr, unsigned long size)
{
    early_iounmap ((__force void __iomem*)addr, size);
}
