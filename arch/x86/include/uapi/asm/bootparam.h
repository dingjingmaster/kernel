/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
#ifndef _ASM_X86_BOOTPARAM_H
#define _ASM_X86_BOOTPARAM_H

#include <asm/setup_data.h>

/* ram_size flags */
#define RAMDISK_IMAGE_START_MASK 0x07FF
#define RAMDISK_PROMPT_FLAG 0x8000
#define RAMDISK_LOAD_FLAG 0x4000

/* loadflags */
#define LOADED_HIGH (1 << 0)
#define KASLR_FLAG (1 << 1)
#define QUIET_FLAG (1 << 5)
#define KEEP_SEGMENTS (1 << 6)
#define CAN_USE_HEAP (1 << 7)

/* xloadflags */
#define XLF_KERNEL_64 (1 << 0)
#define XLF_CAN_BE_LOADED_ABOVE_4G (1 << 1)
#define XLF_EFI_HANDOVER_32 (1 << 2)
#define XLF_EFI_HANDOVER_64 (1 << 3)
#define XLF_EFI_KEXEC (1 << 4)
#define XLF_5LEVEL (1 << 5)
#define XLF_5LEVEL_ENABLED (1 << 6)
#define XLF_MEM_ENCRYPTION (1 << 7)

#ifndef __ASSEMBLY__

#    include <linux/types.h>
#    include <linux/screen_info.h>
#    include <linux/apm_bios.h>
#    include <linux/edd.h>
#    include <asm/ist.h>
#    include <video/edid.h>

/**
 * @brief 由引导程序填充，用于向内核提供必要启动信息。
 * 它主要描述内核的加载方式，包括内存布局、命令行参数位置、启动协议版本等。
 */
struct setup_header
{
    __u8  setup_sects;           // 0x1F1 内核 setup 代码的扇区数
    __u16 root_flags;            // 0x1F2 根设备标志
    __u32 syssize;               // 0x1F4 内核镜像大小(以 16 字节为单位)
    __u16 ram_size;              // 0x1F8 过时字段，已不使用
    __u16 vid_mode;              // 0x1FA 设定的显示模式
    __u16 root_dev;              // 0x1FC 根文件系统设备号
    __u16 boot_flag;             // 0x1FE 必须为 0xAA55，表示有效的引导块
    __u16 jump;                  // 0x200 跳转指令(引导加载程序检查)
    __u32 header;                // 0x202 魔数，必须是：0x53726448("HdrS")
    __u16 version;               // 0x206 启动协议版本
    __u32 realmode_swtch;        // 0x208 过时字段，已不使用
    __u16 start_sys_seg;         // 0x20C 过时字段
    __u16 kernel_version;        // 0x20E 指向内核版本字符串的偏移地址
    __u8  type_of_loader;        // 0x210 加载器类型(GRUB、SYSLINUX 等)
    __u8  loadflags;             // 0x211 启动标志，如是否为高内存加载
    __u16 setup_move_size;       // 0x212 setup 代码可以移动的字节数
    __u32 code32_start;          // 0x214 保护模式 (32-bit) 内核的入口点
    __u32 ramdisk_image;         // 0x218 initrd 的物理地址
    __u32 ramdisk_size;          // 0x21C initrd 的大小
    __u32 bootsect_kludge;       // 0x220 过时字段
    __u16 heap_end_ptr;          // 0x224 内存堆结束指针
    __u8  ext_loader_ver;        // 0x226 额外的加载器版本
    __u8  ext_loader_type;       // 0x227 额外的加载器类型
    __u32 cmd_line_ptr;          // 0x228 内核命令行字符串的地址
    __u32 initrd_addr_max;       // 0x22C initrd 允许的最大加载地址
    __u32 kernel_alignment;      // 0x230 内核对齐要求
    __u8  relocatable_kernel;    // 0x234 内核是否可重定位
    __u8  min_alignment;         // 0x235 内核最小的对齐单位
    __u16 xloadflags;            // 0x236 额外的加载标志
    __u32 cmdline_size;          // 0x238 内核命令行最大长度
    __u32 hardware_subarch;      // 0x23C 硬件子架构
    __u64 hardware_subarch_data; // 0x240 额外的硬件子架构数据
    __u32 payload_offset;        // 0x248 PE 格式内核的偏移
    __u32 payload_length;        // 0x24C PE 格式内核的大小
    __u64 setup_data;            // 0x250 指向 `struct setup_data` 指针
    __u64 pref_address;
    __u32 init_size;
    __u32 handover_offset;
    __u32 kernel_info_offset;
} __attribute__ ((packed));

struct sys_desc_table
{
    __u16 length;
    __u8  table[14];
};

/* Gleaned from OFW's set-parameters in cpu/x86/pc/linux.fth */
struct olpc_ofw_header
{
    __u32 ofw_magic;   /* OFW signature */
    __u32 ofw_version;
    __u32 cif_handler; /* callback into OFW */
    __u32 irq_desc_table;
} __attribute__ ((packed));

struct efi_info
{
    __u32 efi_loader_signature; // "EL64" 表示是EFI启动
    __u32 efi_systab;           // EFI system table 物理地址
    __u32 efi_memdesc_size;     // EFI memory descriptor 大小
    __u32 efi_memdesc_version;  // EFI memory descriptor 版本
    __u32 efi_memmap;           // EFI memory map 物理地址
    __u32 efi_memmap_size;      // EFI memory map 大小
    __u32 efi_systab_hi;        // EFI system table 高 32 位(64位系统使用)
    __u32 efi_memmap_hi;        // EFI memory map 高 32 位
};

/*
 * This is the maximum number of entries in struct boot_params::e820_table
 * (the zeropage), which is part of the x86 boot protocol ABI:
 */
#    define E820_MAX_ENTRIES_ZEROPAGE 128

/*
 * Smallest compatible version of jailhouse_setup_data required by this kernel.
 */
#    define JAILHOUSE_SETUP_REQUIRED_VERSION 1

/* The so-called "zeropage" */
/**
 * @brief boot_params 结构是内核引导过程中用于存储和传递系统启动信息的重要数据结构。它主要由引导加载程序
 * (Bootloader，如：GRUB、syslinux、EFI stub)填充，并传递给内核，以便内核在启动时候能够正确配置系统。
 * 主要作用：
 *  - 内存布局
 *  - 硬盘信息
 *  - 显示模式(如：VESA/VGA)
 *  - 其他BIOS/UEFI相关信息
 * 提供引导参数：
 *  - 内核命令行参数
 *  - 传递 setup_data 结构，用于扩展额外的引导数据
 * 兼容性支持：
 *  - 该结构的设计兼容早期的x86保护模式启动方式
 *  - 适用于BIOS和UEFI启动方式
 */
struct boot_params
{
    struct screen_info     screen_info;                    /* 0x000 */
    struct apm_bios_info   apm_bios_info;                  /* 0x040 */
    __u8                   _pad2[4];                       /* 0x054 */
    __u64                  tboot_addr;                     /* 0x058 */
    struct ist_info        ist_info;                       /* 0x060 */
    __u64                  acpi_rsdp_addr;                 /* 0x070 */
    __u8                   _pad3[8];                       /* 0x078 */
    __u8                   hd0_info[16]; /* obsolete! */   /* 0x080 */
    __u8                   hd1_info[16]; /* obsolete! */   /* 0x090 */
    struct sys_desc_table  sys_desc_table; /* obsolete! */ /* 0x0a0 */
    struct olpc_ofw_header olpc_ofw_header;                /* 0x0b0 */
    __u32                  ext_ramdisk_image;              /* 0x0c0 */
    __u32                  ext_ramdisk_size;               /* 0x0c4 */
    __u32                  ext_cmd_line_ptr;               /* 0x0c8 */
    __u8                   _pad4[112];                     /* 0x0cc */
    __u32                  cc_blob_address;                /* 0x13c */
    struct edid_info       edid_info;                      /* 0x140 */
    /**
     * @brief 存储EFI相关信息
     */
    struct efi_info        efi_info;                     /* 0x1c0 */
    __u32                  alt_mem_k;                    /* 0x1e0 */
    __u32                  scratch; /* Scratch field! */ /* 0x1e4 */
    /**
     * @brief 存储系统内存映射信息
     */
    __u8                   e820_entries;            /* 0x1e8 */
    __u8                   eddbuf_entries;          /* 0x1e9 */
    __u8                   edd_mbr_sig_buf_entries; /* 0x1ea */
    __u8                   kbd_status;              /* 0x1eb */
    __u8                   secure_boot;             /* 0x1ec */
    __u8                   _pad5[2];                /* 0x1ed */
    /*
     * The sentinel is set to a nonzero value (0xff) in header.S.
     *
     * A bootloader is supposed to only take setup_header and put
     * it into a clean boot_params buffer. If it turns out that
     * it is clumsy or too generous with the buffer, it most
     * probably will pick up the sentinel variable too. The fact
     * that this variable then is still 0xff will let kernel
     * know that some variables in boot_params are invalid and
     * kernel should zero out certain portions of boot_params.
     */
    __u8                   sentinel; /* 0x1ef */
    __u8                   _pad6[1]; /* 0x1f0 */
    /**
     * @brief struct setup_header：包含与引导相关的关键信息，如：setup_data的指针等。
     */
    struct setup_header    hdr; /* setup header */              /* 0x1f1 */
    __u8                   _pad7[0x290 - 0x1f1 - sizeof (struct setup_header)];
    __u32                  edd_mbr_sig_buffer[EDD_MBR_SIG_MAX]; /* 0x290 */
    /**
     * @brief 存储系统内存映射信息
     */
    struct boot_e820_entry e820_table[E820_MAX_ENTRIES_ZEROPAGE]; /* 0x2d0 */
    __u8                   _pad8[48];                             /* 0xcd0 */
    struct edd_info        eddbuf[EDDMAXNR];                      /* 0xd00 */
    __u8                   _pad9[276];                            /* 0xeec */
} __attribute__ ((packed));

/**
 * enum x86_hardware_subarch - x86 hardware subarchitecture
 *
 * The x86 hardware_subarch and hardware_subarch_data were added as of the x86
 * boot protocol 2.07 to help distinguish and support custom x86 boot
 * sequences. This enum represents accepted values for the x86
 * hardware_subarch.  Custom x86 boot sequences (not X86_SUBARCH_PC) do not
 * have or simply *cannot* make use of natural stubs like BIOS or EFI, the
 * hardware_subarch can be used on the Linux entry path to revector to a
 * subarchitecture stub when needed. This subarchitecture stub can be used to
 * set up Linux boot parameters or for special care to account for nonstandard
 * handling of page tables.
 *
 * These enums should only ever be used by x86 code, and the code that uses
 * it should be well contained and compartmentalized.
 *
 * KVM and Xen HVM do not have a subarch as these are expected to follow
 * standard x86 boot entries. If there is a genuine need for "hypervisor" type
 * that should be considered separately in the future. Future guest types
 * should seriously consider working with standard x86 boot stubs such as
 * the BIOS or EFI boot stubs.
 *
 * WARNING: this enum is only used for legacy hacks, for platform features that
 *        are not easily enumerated or discoverable. You should not ever use
 *        this for new features.
 *
 * @X86_SUBARCH_PC: Should be used if the hardware is enumerable using standard
 *    PC mechanisms (PCI, ACPI) and doesn't need a special boot flow.
 * @X86_SUBARCH_LGUEST: Used for x86 hypervisor demo, lguest, deprecated
 * @X86_SUBARCH_XEN: Used for Xen guest types which follow the PV boot path,
 *     which start at asm startup_xen() entry point and later jump to the C
 *     xen_start_kernel() entry point. Both domU and dom0 type of guests are
 *     currently supported through this PV boot path.
 * @X86_SUBARCH_INTEL_MID: Used for Intel MID (Mobile Internet Device) platform
 *    systems which do not have the PCI legacy interfaces.
 * @X86_SUBARCH_CE4100: Used for Intel CE media processor (CE4100) SoC
 *     for settop boxes and media devices, the use of a subarch for CE4100
 *     is more of a hack...
 */
enum x86_hardware_subarch
{
    X86_SUBARCH_PC = 0,
    X86_SUBARCH_LGUEST,
    X86_SUBARCH_XEN,
    X86_SUBARCH_INTEL_MID,
    X86_SUBARCH_CE4100,
    X86_NR_SUBARCHS,
};

#endif /* __ASSEMBLY__ */

#endif /* _ASM_X86_BOOTPARAM_H */
