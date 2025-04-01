/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
#ifndef _UAPI_ASM_X86_SETUP_DATA_H
#define _UAPI_ASM_X86_SETUP_DATA_H

/* setup_data/setup_indirect types */
#define SETUP_NONE 0
#define SETUP_E820_EXT 1
#define SETUP_DTB 2
#define SETUP_PCI 3
#define SETUP_EFI 4
#define SETUP_APPLE_PROPERTIES 5
#define SETUP_JAILHOUSE 6
#define SETUP_CC_BLOB 7
#define SETUP_IMA 8
#define SETUP_RNG_SEED 9
#define SETUP_ENUM_MAX SETUP_RNG_SEED

#define SETUP_INDIRECT (1 << 31)
#define SETUP_TYPE_MAX (SETUP_ENUM_MAX | SETUP_INDIRECT)

#ifndef __ASSEMBLY__

#    include <linux/types.h>

/* extensible setup data list node */
struct setup_data
{
    __u64 next;   // 下一个 setup_data 结构, 形成链表
    __u32 type;   /**
                   * 数据类型
                   *  0x1: EFI内存映射;
                   *  0x2: EFI系统表;
                   *  0x3: ACPI RSDP 表;
                   *  0x4: SMBIOS表
                   *  0x5: TSM 相关数据
                   */
    __u32 len;    // 数据长度
    __u8  data[]; // 实际数据
};

/* extensible setup indirect data node */
struct setup_indirect
{
    __u32 type;
    __u32 reserved; /* Reserved, must be set to zero. */
    __u64 len;
    __u64 addr;
};

/**
 * The E820 memory region entry of the boot protocol ABI:
 */
struct boot_e820_entry
{
    __u64 addr;
    __u64 size;
    __u32 type;
} __attribute__ ((packed));

/*
 * The boot loader is passing platform information via this Jailhouse-specific
 * setup data structure.
 */
struct jailhouse_setup_data
{
    struct
    {
        __u16 version;
        __u16 compatible_version;
    } __attribute__ ((packed)) hdr;
    struct
    {
        __u16 pm_timer_address;
        __u16 num_cpus;
        __u64 pci_mmconfig_base;
        __u32 tsc_khz;
        __u32 apic_khz;
        __u8  standard_ioapic;
        __u8  cpu_ids[255];
    } __attribute__ ((packed)) v1;
    struct
    {
        __u32 flags;
    } __attribute__ ((packed)) v2;
} __attribute__ ((packed));

/*
 * IMA buffer setup data information from the previous kernel during kexec
 */
struct ima_setup_data
{
    __u64 addr;
    __u64 size;
} __attribute__ ((packed));

#endif /* __ASSEMBLY__ */

#endif /* _UAPI_ASM_X86_SETUP_DATA_H */
