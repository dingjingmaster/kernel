// SPDX-License-Identifier: GPL-2.0-only
/*
 *  Copyright (C) 1995  Linus Torvalds
 *
 * This file contains the setup_arch() code, which handles the architecture-dependent
 * parts of early kernel initialization.
 */
#include <linux/acpi.h>
#include <linux/console.h>
#include <linux/cpu.h>
#include <linux/crash_dump.h>
#include <linux/dma-map-ops.h>
#include <linux/efi.h>
#include <linux/ima.h>
#include <linux/init_ohci1394_dma.h>
#include <linux/initrd.h>
#include <linux/iscsi_ibft.h>
#include <linux/memblock.h>
#include <linux/panic_notifier.h>
#include <linux/pci.h>
#include <linux/root_dev.h>
#include <linux/hugetlb.h>
#include <linux/tboot.h>
#include <linux/usb/xhci-dbgp.h>
#include <linux/static_call.h>
#include <linux/swiotlb.h>
#include <linux/random.h>

#include <uapi/linux/mount.h>

#include <xen/xen.h>

#include <asm/apic.h>
#include <asm/efi.h>
#include <asm/numa.h>
#include <asm/bios_ebda.h>
#include <asm/bugs.h>
#include <asm/cacheinfo.h>
#include <asm/coco.h>
#include <asm/cpu.h>
#include <asm/efi.h>
#include <asm/gart.h>
#include <asm/hypervisor.h>
#include <asm/io_apic.h>
#include <asm/kasan.h>
#include <asm/kaslr.h>
#include <asm/mce.h>
#include <asm/memtype.h>
#include <asm/mtrr.h>
#include <asm/realmode.h>
#include <asm/olpc_ofw.h>
#include <asm/pci-direct.h>
#include <asm/prom.h>
#include <asm/proto.h>
#include <asm/thermal.h>
#include <asm/unwind.h>
#include <asm/vsyscall.h>
#include <linux/vmalloc.h>
#include <asm/sparsemem.h>

/*
 * max_low_pfn_mapped: highest directly mapped pfn < 4 GB
 * max_pfn_mapped:     highest directly mapped pfn > 4 GB
 *
 * The direct mapping only covers E820_TYPE_RAM regions, so the ranges and gaps are
 * represented by pfn_mapped[].
 */
unsigned long max_low_pfn_mapped;
unsigned long max_pfn_mapped;

#ifdef CONFIG_DMI
RESERVE_BRK (dmi_alloc, 65536);
#endif

unsigned long          _brk_start = (unsigned long)__brk_base;
unsigned long          _brk_end   = (unsigned long)__brk_base;

struct boot_params     boot_params;

/*
 * These are the four main kernel memory regions, we put them into
 * the resource tree so that kdump tools and other debugging tools
 * recover it:
 *
 * 这是四个主要的内核内存区域，我们把它们放到资源树中，以便kdump工具和其他调试工具恢复它：
 *
 * 四区：
 * .text    代码(可执行)    内核函数
 * .rodata  只读数据        常量、字符串、配置
 * .data    可读写数据      全局变量、堆栈
 * .bss     未初始化数据    动态分配缓存区
 */

// 内核只读数据段(常量、字符串、配置)
static struct resource rodata_resource = {.name  = "Kernel rodata",
                                          .start = 0,
                                          .end   = 0,
                                          .flags = IORESOURCE_BUSY | IORESOURCE_SYSTEM_RAM};

// 数据区
static struct resource data_resource   = {.name  = "Kernel data",
                                          .start = 0,
                                          .end   = 0,
                                          .flags = IORESOURCE_BUSY | IORESOURCE_SYSTEM_RAM};

// 代码区
static struct resource code_resource   = {.name  = "Kernel code",
                                          .start = 0,
                                          .end   = 0,
                                          .flags = IORESOURCE_BUSY | IORESOURCE_SYSTEM_RAM};

// 未初始化数据区
static struct resource bss_resource    = {.name  = "Kernel bss",
                                          .start = 0,
                                          .end   = 0,
                                          .flags = IORESOURCE_BUSY | IORESOURCE_SYSTEM_RAM};

#ifdef CONFIG_X86_32
/* CPU data as detected by the assembly code in head_32.S */
struct cpuinfo_x86 new_cpu_data;

struct apm_info    apm_info;
EXPORT_SYMBOL (apm_info);

#    if defined(CONFIG_X86_SPEEDSTEP_SMI) || defined(CONFIG_X86_SPEEDSTEP_SMI_MODULE)
struct ist_info ist_info;
EXPORT_SYMBOL (ist_info);
#    else
struct ist_info ist_info;
#    endif

#endif

struct cpuinfo_x86 boot_cpu_data __read_mostly;
EXPORT_SYMBOL (boot_cpu_data);

#if !defined(CONFIG_X86_PAE) || defined(CONFIG_X86_64)
__visible unsigned long mmu_cr4_features __ro_after_init;
#else
__visible unsigned long mmu_cr4_features __ro_after_init = X86_CR4_PAE;
#endif

#ifdef CONFIG_IMA
static phys_addr_t ima_kexec_buffer_phys;
static size_t      ima_kexec_buffer_size;
#endif

/* Boot loader ID and version as integers, for the benefit of proc_dointvec */
int                bootloader_type, bootloader_version;

/*
 * Setup options
 */
struct screen_info screen_info;
EXPORT_SYMBOL (screen_info);
struct edid_info edid_info;
EXPORT_SYMBOL_GPL (edid_info);

extern int    root_mountflags;

unsigned long saved_video_mode;

#define RAMDISK_IMAGE_START_MASK 0x07FF
#define RAMDISK_PROMPT_FLAG 0x8000
#define RAMDISK_LOAD_FLAG 0x4000

static char __initdata command_line[COMMAND_LINE_SIZE];
#ifdef CONFIG_CMDLINE_BOOL
char                       builtin_cmdline[COMMAND_LINE_SIZE] = CONFIG_CMDLINE;
bool builtin_cmdline_added __ro_after_init;
#endif

#if defined(CONFIG_EDD) || defined(CONFIG_EDD_MODULE)
struct edd edd;
#    ifdef CONFIG_EDD_MODULE
EXPORT_SYMBOL (edd);
#    endif
/**
 * copy_edd() - Copy the BIOS EDD information
 *              from boot_params into a safe place.
 *
 */
static inline void __init copy_edd (void)
{
    memcpy (edd.mbr_signature, boot_params.edd_mbr_sig_buffer, sizeof (edd.mbr_signature));
    memcpy (edd.edd_info, boot_params.eddbuf, sizeof (edd.edd_info));
    edd.mbr_signature_nr = boot_params.edd_mbr_sig_buf_entries;
    edd.edd_info_nr      = boot_params.eddbuf_entries;
}
#else
/**
 * copy_edd 函数是 Linux 内核初始化阶段的重要函数之一，负责从 BIOS 的 EDD(Enhanced Disk Drive)
 * 数据结构中提取并复制磁盘设备信息到内核的 boot_params 结构中。
 * 以下是其核心作用和功能详解：
 */
static inline void __init copy_edd (void)
{
}
#endif

void* __init extend_brk (size_t size, size_t align)
{
    size_t mask = align - 1;
    void*  ret;

    BUG_ON (_brk_start == 0);
    BUG_ON (align & mask);

    _brk_end = (_brk_end + mask) & ~mask;
    BUG_ON ((char*)(_brk_end + size) > __brk_limit);

    ret = (void*)_brk_end;
    _brk_end += size;

    memset (ret, 0, size);

    return ret;
}

#ifdef CONFIG_X86_32
static void __init cleanup_highmap (void)
{
}
#endif

static void __init reserve_brk (void)
{
    if (_brk_end > _brk_start)
        memblock_reserve (__pa_symbol (_brk_start), _brk_end - _brk_start);

    /* Mark brk area as locked down and no longer taking any
       new allocations */
    _brk_start = 0;
}

#ifdef CONFIG_BLK_DEV_INITRD

static u64 __init get_ramdisk_image (void)
{
    u64 ramdisk_image = boot_params.hdr.ramdisk_image;

    ramdisk_image |= (u64)boot_params.ext_ramdisk_image << 32;

    if (ramdisk_image == 0)
        ramdisk_image = phys_initrd_start;

    return ramdisk_image;
}
static u64 __init get_ramdisk_size (void)
{
    u64 ramdisk_size = boot_params.hdr.ramdisk_size;

    ramdisk_size |= (u64)boot_params.ext_ramdisk_size << 32;

    if (ramdisk_size == 0)
        ramdisk_size = phys_initrd_size;

    return ramdisk_size;
}

static void __init relocate_initrd (void)
{
    /* Assume only end is not page aligned */
    u64 ramdisk_image = get_ramdisk_image ();
    u64 ramdisk_size  = get_ramdisk_size ();
    u64 area_size     = PAGE_ALIGN (ramdisk_size);

    /* We need to move the initrd down into directly mapped mem */
    u64 relocated_ramdisk =
            memblock_phys_alloc_range (area_size, PAGE_SIZE, 0, PFN_PHYS (max_pfn_mapped));
    if (!relocated_ramdisk)
        panic ("Cannot find place for new RAMDISK of size %lld\n", ramdisk_size);

    initrd_start = relocated_ramdisk + PAGE_OFFSET;
    initrd_end   = initrd_start + ramdisk_size;
    printk (KERN_INFO "Allocated new RAMDISK: [mem %#010llx-%#010llx]\n", relocated_ramdisk,
            relocated_ramdisk + ramdisk_size - 1);

    copy_from_early_mem ((void*)initrd_start, ramdisk_image, ramdisk_size);

    printk (KERN_INFO "Move RAMDISK from [mem %#010llx-%#010llx] to"
                      " [mem %#010llx-%#010llx]\n",
            ramdisk_image, ramdisk_image + ramdisk_size - 1, relocated_ramdisk,
            relocated_ramdisk + ramdisk_size - 1);
}

static void __init early_reserve_initrd (void)
{
    /* Assume only end is not page aligned */
    u64 ramdisk_image = get_ramdisk_image ();
    u64 ramdisk_size  = get_ramdisk_size ();
    u64 ramdisk_end   = PAGE_ALIGN (ramdisk_image + ramdisk_size);

    if (!boot_params.hdr.type_of_loader || !ramdisk_image || !ramdisk_size)
        return; /* No initrd provided by bootloader */

    memblock_reserve (ramdisk_image, ramdisk_end - ramdisk_image);
}

static void __init reserve_initrd (void)
{
    /* Assume only end is not page aligned */
    u64 ramdisk_image = get_ramdisk_image ();
    u64 ramdisk_size  = get_ramdisk_size ();
    u64 ramdisk_end   = PAGE_ALIGN (ramdisk_image + ramdisk_size);

    if (!boot_params.hdr.type_of_loader || !ramdisk_image || !ramdisk_size)
        return; /* No initrd provided by bootloader */

    initrd_start = 0;

    printk (KERN_INFO "RAMDISK: [mem %#010llx-%#010llx]\n", ramdisk_image, ramdisk_end - 1);

    if (pfn_range_is_mapped (PFN_DOWN (ramdisk_image), PFN_DOWN (ramdisk_end))) {
        /* All are mapped, easy case */
        initrd_start = ramdisk_image + PAGE_OFFSET;
        initrd_end   = initrd_start + ramdisk_size;
        return;
    }

    relocate_initrd ();

    memblock_phys_free (ramdisk_image, ramdisk_end - ramdisk_image);
}

#else
static void __init early_reserve_initrd (void)
{
}
static void __init reserve_initrd (void)
{
}
#endif /* CONFIG_BLK_DEV_INITRD */

static void __init add_early_ima_buffer (u64 phys_addr)
{
#ifdef CONFIG_IMA
    struct ima_setup_data* data;

    data = early_memremap (phys_addr + sizeof (struct setup_data), sizeof (*data));
    if (!data) {
        pr_warn ("setup: failed to memremap ima_setup_data entry\n");
        return;
    }

    if (data->size) {
        memblock_reserve (data->addr, data->size);
        ima_kexec_buffer_phys = data->addr;
        ima_kexec_buffer_size = data->size;
    }

    early_memunmap (data, sizeof (*data));
#else
    pr_warn ("Passed IMA kexec data, but CONFIG_IMA not set. Ignoring.\n");
#endif
}

#if defined(CONFIG_HAVE_IMA_KEXEC) && !defined(CONFIG_OF_FLATTREE)
int __init ima_free_kexec_buffer (void)
{
    if (!ima_kexec_buffer_size)
        return -ENOENT;

    memblock_free_late (ima_kexec_buffer_phys, ima_kexec_buffer_size);

    ima_kexec_buffer_phys = 0;
    ima_kexec_buffer_size = 0;

    return 0;
}

int __init ima_get_kexec_buffer (void** addr, size_t* size)
{
    if (!ima_kexec_buffer_size)
        return -ENOENT;

    *addr = __va (ima_kexec_buffer_phys);
    *size = ima_kexec_buffer_size;

    return 0;
}
#endif

/**
 * 负责解析和初始化 BIOS 传递的启动参数（Setup Data），为内核后续的硬件探测和资源分配提供基础信息。
 * 1. 桥梁功能：作为内核与 BIOS 之间的中间层，
 *    将 BIOS 存储在特定内存区域（如 0x9FC00 或 0xFFFF0000）的启动参数结构（setup_header）
 *    解析到内核可用的数据结构（boot_params）中
 * 2. 关键初始化：完成内核启动参数的早期解析，
 *    为后续的硬件探测（如 CPU、内存、PCI 设备）、命令行参数处理等步骤提供必要信息。
 */
static void __init parse_setup_data (void)
{
    struct setup_data* data;
    u64                pa_data, pa_next;

    /**
     * setup_data 字段属于 boot_params.hdr 结构体的一部分，用于存储引导程序传递给内核的关键参数，例如：
     *  - 内存布局信息（如物理内存大小、可用内存区域）
     *  - 显示模式设置（如分辨率、颜色深度）
     *  - 命令行参数（如 root=/dev/sda1）
     *  - 其他与硬件初始化相关的配置
     * 数据来源与格式
     *  - 在UEFI启动场景中，setup_data 从引导程序加载的 bzImage 的 setup_header 拷贝而来，
     *    包含与引导程序交互的协议数据
     *  - 在传统BIOS启动中，该字段由引导程序（如GRUB）填充，内核通过解析 setup_header 获取参数
     */
    pa_data = boot_params.hdr.setup_data;
    while (pa_data) {
        u32 data_len, data_type;
        data      = early_memremap (pa_data, sizeof (*data));
        data_len  = data->len + sizeof (struct setup_data);
        data_type = data->type;
        pa_next   = data->next;
        early_memunmap (data, sizeof (*data));

        switch (data_type) {
        case SETUP_E820_EXT: e820__memory_setup_extended (pa_data, data_len); break;
        case SETUP_DTB: add_dtb (pa_data); break;
        case SETUP_EFI: parse_efi_setup (pa_data, data_len); break;
        case SETUP_IMA: add_early_ima_buffer (pa_data); break;
        case SETUP_RNG_SEED:
            data = early_memremap (pa_data, data_len);
            add_bootloader_randomness (data->data, data->len);
            /* Zero seed for forward secrecy. */
            memzero_explicit (data->data, data->len);
            /* Zero length in case we find ourselves back here by accident. */
            memzero_explicit (&data->len, sizeof (data->len));
            early_memunmap (data, data_len);
            break;
        default: break;
        }
        pa_data = pa_next;
    }
}

static void __init memblock_x86_reserve_range_setup_data (void)
{
    struct setup_indirect* indirect;
    struct setup_data*     data;
    u64                    pa_data, pa_next;
    u32                    len;

    pa_data = boot_params.hdr.setup_data;
    while (pa_data) {
        data = early_memremap (pa_data, sizeof (*data));
        if (!data) {
            pr_warn ("setup: failed to memremap setup_data entry\n");
            return;
        }

        len     = sizeof (*data);
        pa_next = data->next;

        memblock_reserve (pa_data, sizeof (*data) + data->len);

        if (data->type == SETUP_INDIRECT) {
            len += data->len;
            early_memunmap (data, sizeof (*data));
            data = early_memremap (pa_data, len);
            if (!data) {
                pr_warn ("setup: failed to memremap indirect setup_data\n");
                return;
            }

            indirect = (struct setup_indirect*)data->data;

            if (indirect->type != SETUP_INDIRECT)
                memblock_reserve (indirect->addr, indirect->len);
        }

        pa_data = pa_next;
        early_memunmap (data, len);
    }
}

static void __init arch_reserve_crashkernel (void)
{
    unsigned long long crash_base, crash_size, low_size = 0;
    char*              cmdline = boot_command_line;
    bool               high    = false;
    int                ret;

    if (!IS_ENABLED (CONFIG_CRASH_RESERVE))
        return;

    ret = parse_crashkernel (cmdline, memblock_phys_mem_size (), &crash_size, &crash_base,
                             &low_size, &high);
    if (ret)
        return;

    if (xen_pv_domain ()) {
        pr_info ("Ignoring crashkernel for a Xen PV domain\n");
        return;
    }

    reserve_crashkernel_generic (cmdline, crash_size, crash_base, low_size, high);
}

static struct resource standard_io_resources[] = {
        {.name = "dma1", .start = 0x00, .end = 0x1f, .flags = IORESOURCE_BUSY | IORESOURCE_IO},
        {.name = "pic1", .start = 0x20, .end = 0x21, .flags = IORESOURCE_BUSY | IORESOURCE_IO},
        {.name = "timer0", .start = 0x40, .end = 0x43, .flags = IORESOURCE_BUSY | IORESOURCE_IO},
        {.name = "timer1", .start = 0x50, .end = 0x53, .flags = IORESOURCE_BUSY | IORESOURCE_IO},
        {.name = "keyboard", .start = 0x60, .end = 0x60, .flags = IORESOURCE_BUSY | IORESOURCE_IO},
        {.name = "keyboard", .start = 0x64, .end = 0x64, .flags = IORESOURCE_BUSY | IORESOURCE_IO},
        {.name  = "dma page reg",
         .start = 0x80,
         .end   = 0x8f,
         .flags = IORESOURCE_BUSY | IORESOURCE_IO},
        {.name = "pic2", .start = 0xa0, .end = 0xa1, .flags = IORESOURCE_BUSY | IORESOURCE_IO},
        {.name = "dma2", .start = 0xc0, .end = 0xdf, .flags = IORESOURCE_BUSY | IORESOURCE_IO},
        {.name = "fpu", .start = 0xf0, .end = 0xff, .flags = IORESOURCE_BUSY | IORESOURCE_IO}};

void __init reserve_standard_io_resources (void)
{
    int i;

    /* request I/O space for devices used on all i[345]86 PCs */
    for (i = 0; i < ARRAY_SIZE (standard_io_resources); i++)
        request_resource (&ioport_resource, &standard_io_resources[i]);
}

static bool __init snb_gfx_workaround_needed (void)
{
#ifdef CONFIG_PCI
    int                          i;
    u16                          vendor, devid;
    static const __initconst u16 snb_ids[] = {
            0x0102, 0x0112, 0x0122, 0x0106, 0x0116, 0x0126, 0x010a,
    };

    /* Assume no if something weird is going on with PCI */
    if (!early_pci_allowed ())
        return false;

    vendor = read_pci_config_16 (0, 2, 0, PCI_VENDOR_ID);
    if (vendor != 0x8086)
        return false;

    devid = read_pci_config_16 (0, 2, 0, PCI_DEVICE_ID);
    for (i = 0; i < ARRAY_SIZE (snb_ids); i++)
        if (devid == snb_ids[i])
            return true;
#endif

    return false;
}

/*
 * Sandy Bridge graphics has trouble with certain ranges, exclude
 * them from allocation.
 */
static void __init trim_snb_memory (void)
{
    static const __initconst unsigned long bad_pages[] = {
            0x20050000, 0x20110000, 0x20130000, 0x20138000, 0x40004000,
    };
    int i;

    if (!snb_gfx_workaround_needed ())
        return;

    printk (KERN_DEBUG "reserving inaccessible SNB gfx pages\n");

    /*
     * SandyBridge integrated graphics devices have a bug that prevents
     * them from accessing certain memory ranges, namely anything below
     * 1M and in the pages listed in bad_pages[] above.
     *
     * To avoid these pages being ever accessed by SNB gfx devices reserve
     * bad_pages that have not already been reserved at boot time.
     * All memory below the 1 MB mark is anyway reserved later during
     * setup_arch(), so there is no need to reserve it here.
     */

    for (i = 0; i < ARRAY_SIZE (bad_pages); i++) {
        if (memblock_reserve (bad_pages[i], PAGE_SIZE))
            printk (KERN_WARNING "failed to reserve 0x%08lx\n", bad_pages[i]);
    }
}

static void __init trim_bios_range (void)
{
    /*
     * A special case is the first 4Kb of memory;
     * This is a BIOS owned area, not kernel ram, but generally
     * not listed as such in the E820 table.
     *
     * This typically reserves additional memory (64KiB by default)
     * since some BIOSes are known to corrupt low memory.  See the
     * Kconfig help text for X86_RESERVE_LOW.
     */
    e820__range_update (0, PAGE_SIZE, E820_TYPE_RAM, E820_TYPE_RESERVED);

    /*
     * special case: Some BIOSes report the PC BIOS
     * area (640Kb -> 1Mb) as RAM even though it is not.
     * take them out.
     */
    e820__range_remove (BIOS_BEGIN, BIOS_END - BIOS_BEGIN, E820_TYPE_RAM, 1);

    e820__update_table (e820_table);
}

/**
 * called before trim_bios_range() to spare extra sanitize
 * Linux 内核中用于整合 BIOS 通过 E820 接口探测到的物理内存布局的核心组件，其作用可概括为以下方面：
 *  1. 内存区域添加与标记: 将 BIOS 通过 E820 接口提供的物理内存区域（如可用 RAM、保留区域）添加到内核的 e820_table 中，并标记其类型（如 E820_RAM、E820_RESERVED）
 *  2. 全局内存参数初始化: 根据 E820 探测结果设置关键全局变量，如 max_pfn（最大物理帧号）和 max_low_pfn（低内存最大帧号），用于确定内存分区的范围（如 DMA、NORMAL、HIGHMEM）
 *  3. 内存预分配准备: 通过 memblock_x86_fill() 初始化内存预分配系统，将可用内存标记为可分配（memory 类型），保留区域标记为不可分配（reserved 类型）
 */
static void __init e820_add_kernel_range (void)
{
    u64 start = __pa_symbol (_text);
    u64 size  = __pa_symbol (_end) - start;

    /*
     * Complain if .text .data and .bss are not marked as E820_TYPE_RAM and
     * attempt to fix it by adding the range. We may have a confused BIOS,
     * or the user may have used memmap=exactmap or memmap=xxM$yyM to
     * exclude kernel range. If we really are running on top non-RAM,
     * we will crash later anyways.
     */
    if (e820__mapped_all (start, start + size, E820_TYPE_RAM)) {
        return;
    }

    pr_warn (".text .data .bss are not marked as E820_TYPE_RAM!\n");
    e820__range_remove (start, size, E820_TYPE_RAM, 0);
    e820__range_add (start, size, E820_TYPE_RAM);
}

/**
 * 用于在内核启动的早期阶段预留（reserve）特定的内存区域。
 * 这一过程确保了这些内存区域不会被内核或其他系统组件错误地使用，从而保证关键硬件设备、固件或引导加载程序所需的内存区域得以保留和正常工作。
 */
static void __init early_reserve_memory (void)
{
    /**
     * Reserve the memory occupied by the kernel between _text and
     * __end_of_kernel_reserve symbols. Any kernel sections after the
     * __end_of_kernel_reserve symbol must be explicitly reserved with a
     * separate memblock_reserve() or they will be discarded.
     *
     * 保留 _text 和 __end_of_kernel_reserve 符号之间内核占用的内存。
     * 在__end_of_kernel_reserve符号之后的内核部分必须通过单独的memblock_reserve()明确保留，否则将被丢弃。
     *
     * _text:内核代码段的起始地址，包含：
     *  1. 内核的入口点，是CPU复位后执行的第一条指令
     *  2. 整个内核的代码段(.text)。编译后的内核函数和指令
     * _text：之后的区域主要包含：
     *  1. .text：内核代码本体，包含所有内核函数
     *  2. .rodata：只读数据，比如：常量、内核符号表、syscall表等
     *  3. .init.text/.init.data：初始化期间使用的数据和代码，例如：start_kernel()及其调用的初始化函数(这些段在内核启动完成后会被释放)
     * __end_of_kernel_reserve：
     *  1. 该符号通常表示内核保留区域的结束，其具体位置依赖于内核的链接脚本和体系架构
     *  2. 在vmlinux.lds.S中，它一般用于标记内核静态影响的结尾，后续的物理内存可用于动态分配(如：memblock管理的区域)
     */
    memblock_reserve (__pa_symbol (_text),
                      (unsigned long)__end_of_kernel_reserve - (unsigned long)_text);

    /*
     * The first 4Kb of memory is a BIOS owned area, but generally it is
     * not listed as such in the E820 table.
     *
     * Reserve the first 64K of memory since some BIOSes are known to
     * corrupt low memory. After the real mode trampoline is allocated the
     * rest of the memory below 640k is reserved.
     *
     * In addition, make sure page 0 is always reserved because on
     * systems with L1TF its contents can be leaked to user processes.
     *
     * 内存的前 4KB 是 BIOS 拥有的区域，但通常不会在 E820 表中列出。
     *
     * 保留前 64K 内存，因为某些 BIOS 会损坏低内存。实际模式分配后，640K 以下的其余内存将被保留。
     *
     * 此外，确保第 0 页始终被保留，因为在使用 L1TF 的系统中，其内容可能会泄露给用户进程。
     */
    memblock_reserve (0, SZ_64K);

    // 保留 ramdisk 展开内存
    early_reserve_initrd ();

    // 内核启动早期阶段与系统初始化相关的特定内存区域标记为保留区域，确保这些区域内存中保存的setup数据不会被后续内存分配所覆盖或使用。
    // - 保留关键的启动数据区域：BIOS或引导加载器将一些重要配置信息、参数以及其他setup数据存放在特定内存区域，被memblock设置位保留，避免被通用内存分配器误用
    // - 适配x86架构的特殊需求：x86平台某些特殊内存布局要求，比如低内存(0 ~ 1MB)中某些区域必须保持原样。该函数会根据平台要求，保留相应内存范围，以保证与旧有BIOS数据区、EBDA(扩展BIOS数据区)等相关数据不会被破坏
    // - 为后续内核初始化提供保障：在内核进一步建立完善内存管理机制（比如：页描述符数组和buddy分配器）之前，确保这些setup数据保持不变。
    memblock_x86_reserve_range_setup_data ();

    // 保留BIOS和固件使用的内存
    reserve_bios_regions ();

    /**
     * 在Intel Sandy Bridge(SNB)平台上调整和保留特定的物理内存区域，以防止内核错误使用某些可能导致系统不稳定的内存范围。
     * 其主要功能包括：
     *  1. 检测并保留问题内存区域
     *  2. 防止GPU或硬件冲突
     *  3. 调用 memblock_remove进行调整
     *
     * SNB 是Intel 2011年推出的第二代Core处理器微架构，属于x86-64指令集架构的CPU平台。它是Nehaiem(第一代 Core i系列)的继任者，主要用于桌面、笔记本和服务器市场。
     *  1. 32nm制程工艺
     *  2. 集成GPU(核显)
     *  3. 改进微架构
     *  4. 新增内存控制器
     *  5. 引入PCIe 2.0控制器：处理器内部集成了 PCIe 2.0控制器，减少了对外部芯片组的依赖，提高了I/O速度
     */
    trim_snb_memory ();
}

/*
 * Dump out kernel offset information on panic.
 */
static int dump_kernel_offset (struct notifier_block* self, unsigned long v, void* p)
{
    if (kaslr_enabled ()) {
        pr_emerg ("Kernel Offset: 0x%lx from 0x%lx (relocation range: 0x%lx-0x%lx)\n",
                  kaslr_offset (), __START_KERNEL, __START_KERNEL_map, MODULES_VADDR - 1);
    } else {
        pr_emerg ("Kernel Offset: disabled\n");
    }

    return 0;
}

void x86_configure_nx (void)
{
    if (boot_cpu_has (X86_FEATURE_NX))
        __supported_pte_mask |= _PAGE_NX;
    else
        __supported_pte_mask &= ~_PAGE_NX;
}

static void __init x86_report_nx (void)
{
    if (!boot_cpu_has (X86_FEATURE_NX)) {
        printk (KERN_NOTICE "Notice: NX (Execute Disable) protection "
                            "missing in CPU!\n");
    } else {
#if defined(CONFIG_X86_64) || defined(CONFIG_X86_PAE)
        printk (KERN_INFO "NX (Execute Disable) protection: active\n");
#else
        /* 32bit non-PAE kernel, NX cannot be used */
        printk (KERN_NOTICE "Notice: NX (Execute Disable) protection "
                            "cannot be enabled: non-PAE kernel!\n");
#endif
    }
}

/*
 * Determine if we were loaded by an EFI loader.  If so, then we have also been
 * passed the efi memmap, systab, etc., so we should use these data structures
 * for initialization.  Note, the efi init code path is determined by the
 * global efi_enabled. This allows the same kernel image to be used on existing
 * systems (with a traditional BIOS) as well as on EFI systems.
 */
/*
 * setup_arch - architecture-specific boot-time initializations
 *
 * Note: On x86_64, fixmaps are ready for use even before this is called.
 */

void __init setup_arch (char** cmdline_p)
{
#ifdef CONFIG_X86_32
    memcpy (&boot_cpu_data, &new_cpu_data, sizeof (new_cpu_data));

    /*
     * copy kernel address range established so far and switch
     * to the proper swapper page table
     */
    clone_pgd_range (swapper_pg_dir + KERNEL_PGD_BOUNDARY, initial_page_table + KERNEL_PGD_BOUNDARY,
                     KERNEL_PGD_PTRS);

    load_cr3 (swapper_pg_dir);
    /*
     * Note: Quark X1000 CPUs advertise PGE incorrectly and require
     * a cr3 based tlb flush, so the following __flush_tlb_all()
     * will not flush anything because the CPU quirk which clears
     * X86_FEATURE_PGE has not been invoked yet. Though due to the
     * load_cr3() above the TLB has been flushed already. The
     * quirk is invoked before subsequent calls to __flush_tlb_all()
     * so proper operation is guaranteed.
     */
    __flush_tlb_all ();
#else
    printk (KERN_INFO "Command line: %s\n", boot_command_line);
    boot_cpu_data.x86_phys_bits = MAX_PHYSMEM_BITS;
#endif

#ifdef CONFIG_CMDLINE_BOOL
#    ifdef CONFIG_CMDLINE_OVERRIDE
    strscpy (boot_command_line, builtin_cmdline, COMMAND_LINE_SIZE);
#    else
    if (builtin_cmdline[0]) {
        /* append boot loader cmdline to builtin */
        strlcat (builtin_cmdline, " ", COMMAND_LINE_SIZE);
        strlcat (builtin_cmdline, boot_command_line, COMMAND_LINE_SIZE);
        strscpy (boot_command_line, builtin_cmdline, COMMAND_LINE_SIZE);
    }
#    endif
    builtin_cmdline_added = true;
#endif

    strscpy (command_line, boot_command_line, COMMAND_LINE_SIZE);
    *cmdline_p = command_line;

    /**
     * If we have OLPC OFW, we might end up relocating the fixmap due to
     * reserve_top(), so do this before touching the ioremap area.
     *
     * OLPC 是特殊硬件，通用计算机不需要关注其初始化过程
     */
    olpc_ofw_detect ();

    idt_setup_early_traps ();
    early_cpu_init ();
    jump_label_init ();
    static_call_init ();
    early_ioremap_init ();

    setup_olpc_ofw_pgd ();

    ROOT_DEV    = old_decode_dev (boot_params.hdr.root_dev);
    screen_info = boot_params.screen_info;
    edid_info   = boot_params.edid_info;
#ifdef CONFIG_X86_32
    apm_info.bios = boot_params.apm_bios_info;
    ist_info      = boot_params.ist_info;
#endif
    saved_video_mode = boot_params.hdr.vid_mode;
    bootloader_type  = boot_params.hdr.type_of_loader;
    if ((bootloader_type >> 4) == 0xe) {
        bootloader_type &= 0xf;
        bootloader_type |= (boot_params.hdr.ext_loader_type + 0x10) << 4;
    }
    bootloader_version = bootloader_type & 0xf;
    bootloader_version |= boot_params.hdr.ext_loader_ver << 4;

#ifdef CONFIG_BLK_DEV_RAM
    rd_image_start = boot_params.hdr.ram_size & RAMDISK_IMAGE_START_MASK;
#endif
#ifdef CONFIG_EFI
    if (!strncmp ((char*)&boot_params.efi_info.efi_loader_signature, EFI32_LOADER_SIGNATURE, 4)) {
        set_bit (EFI_BOOT, &efi.flags);
    } else if (!strncmp ((char*)&boot_params.efi_info.efi_loader_signature, EFI64_LOADER_SIGNATURE,
                         4)) {
        set_bit (EFI_BOOT, &efi.flags);
        set_bit (EFI_64BIT, &efi.flags);
    }
#endif

    x86_init.oem.arch_setup (); // x86_64 空实现

    /**
     * Do some memory reservations *before* memory is added to memblock, so
     * memblock allocations won't overwrite it.
     *
     * After this point, everything still needed from the boot loader or
     * firmware or kernel text should be early reserved or marked not RAM in
     * e820. All other memory is free game.
     *
     * This call needs to happen before e820__memory_setup() which calls the
     * xen_memory_setup() on Xen dom0 which relies on the fact that those
     * early reservations have happened already.
     *
     * 在内存被添加到memblock之前做一些内存预留，这样memblock分配就不会覆盖它。
     * 在此之后，引导加载程序或固件或内核文本中仍然需要的所有内容都应该在e820中提前保留或标记为非RAM。
     * 所有其他内存都是空闲的。
     * 这个调用需要发生在e820__memory_setup()之前，
     * e820__memory_setup()调用Xen dom0上的xen_memory_setup()，这依赖于这些早期的保留已经发生的事实。
     */
    early_reserve_memory ();

    // boot_cpu_data.x86_phys_bits = arch/x86/kernel/setup.c
    // boot_cpu_data.x86_phys_bits = MAX_PHYSMEM_BITS;
    // MAX_PHYSMEM_BITS 2^n: max size of physical address space
    // arch/x86/include/asm/sparsemem.h => #define MAX_PHYSMEM_BITS (pgtable_l5_enabled() ? 52 : 46)
    // 传统4级页表：PGD、PUD、PMD、PTE
    iomem_resource.end = (1ULL << boot_cpu_data.x86_phys_bits) - 1;

    e820__memory_setup ();
    parse_setup_data ();

    // 桥梁功能：作为内核与 BIOS 之间的接口，将 BIOS 存储的 EDD 数据（包含硬盘、光驱等存储设备的详细信息）
    // 解析到内核可用的数据结构中。
    copy_edd (); // 空实现

    if (!boot_params.hdr.root_flags) {
        root_mountflags &= ~MS_RDONLY;
    }

    /**
     * Linux 内核初始化过程中用于设置初始内存管理结构（Initial Memory Management, init_mm）的核心函数，
     * 其作用主要包括以下方面:
     *  1. 初始化初始内存描述符
     *      创建内存描述符：该函数负责创建并初始化全局内存描述符 init_mm，
     *      该结构体记录了内核早期阶段使用的内存布局信息，包括:
     *        - 虚拟地址空间：定义内核初始的虚拟地址范围（如 0xFFFF800000000000 到 0xFFFFFFFFFFFFFFFF）
     *        - 页表配置：设置初始页表（如临时页表），确保内核能够访问物理内存和设备I/O空间
     *        - 内存区域注册：将 BIOS/E820 探测到的可用内存区域（如 E820_RAM）注册到 init_mm 中，
     *          为后续内存管理模块提供基础数据
     *  2. 页表初始化
     *      - 临时页表构建：在 MMU 启用前，setup_initial_init_mm 会构建一个临时的页表，
     *        将物理内存映射到内核的虚拟地址空间，确保内核代码和数据可访问。
     *      - 地址空间隔离：通过页表设置，区分内核空间和用户空间的地址范围，防止早期阶段的内存越界访问
     *   3. 内存管理子系统准备
     *      - 初始化内存管理数据结构：如页缓存（page cache）、内存分配器（如 vmalloc）等，
     *        为后续内存分配和页面管理奠定基础
     *      - 中断处理支持：配置内存管理相关的中断处理程序，确保在内存访问异常时能够正确响应
     *   4. 在内核启动流程中的位置
     */
    setup_initial_init_mm (_text, _etext, _edata, (void*)_brk_end);

    // 初始化代码段、数据段、只读数据段、未初始化数据段 范围
    code_resource.start   = __pa_symbol (_text);
    code_resource.end     = __pa_symbol (_etext) - 1;
    rodata_resource.start = __pa_symbol (__start_rodata);
    rodata_resource.end   = __pa_symbol (__end_rodata) - 1;
    data_resource.start   = __pa_symbol (_sdata);
    data_resource.end     = __pa_symbol (_edata) - 1;
    bss_resource.start    = __pa_symbol (__bss_start);
    bss_resource.end      = __pa_symbol (__bss_stop) - 1;

    x86_configure_nx (); // CPU 是否包含NX功能(禁止数据段执行)
    parse_early_param ();

    if (efi_enabled (EFI_BOOT)) {
        efi_memblock_x86_reserve_range ();
    }

#ifdef CONFIG_MEMORY_HOTPLUG
    /*
     * Memory used by the kernel cannot be hot-removed because Linux
     * cannot migrate the kernel pages. When memory hotplug is
     * enabled, we should prevent memblock from allocating memory
     * for the kernel.
     *
     * ACPI SRAT records all hotpluggable memory ranges. But before
     * SRAT is parsed, we don't know about it.
     *
     * The kernel image is loaded into memory at very early time. We
     * cannot prevent this anyway. So on NUMA system, we set any
     * node the kernel resides in as un-hotpluggable.
     *
     * Since on modern servers, one node could have double-digit
     * gigabytes memory, we can assume the memory around the kernel
     * image is also un-hotpluggable. So before SRAT is parsed, just
     * allocate memory near the kernel image to try the best to keep
     * the kernel away from hotpluggable memory.
     */
    if (movable_node_is_enabled ())
        memblock_set_bottom_up (true);
#endif

    x86_report_nx ();         // 打印输出 nx 支持情况
    apic_setup_apic_calls (); // APIC

    if (acpi_mps_check ()) {
#ifdef CONFIG_X86_LOCAL_APIC
        apic_is_disabled = true;
#endif
        setup_clear_cpu_cap (X86_FEATURE_APIC);
    }

    e820__reserve_setup_data ();
    e820__finish_early_params ();

    if (efi_enabled (EFI_BOOT)) {
        efi_init ();
    }

    reserve_ibft_region ();
    x86_init.resources.dmi_setup ();

    /**
     * VMware detection requires dmi to be available, so this
     * needs to be done after dmi_setup(), for the boot CPU.
     * For some guest types (Xen PV, SEV-SNP, TDX) it is required to be
     * called before cache_bp_init() for setting up MTRR state.
     *
     * VMware检测需要dmi可用，所以这需要在dmi_setup（）之后完成，用于引导CPU。
     * 对于某些客户机类型（Xen PV、SEV-SNP、TDX），需要在cache_bp_init（）之前调用它来设置MTRR状态。
     */
    init_hypervisor_platform ();

    tsc_early_init ();
    x86_init.resources.probe_roms ();

    /* after parse_early_param, so could debug it */
    insert_resource (&iomem_resource, &code_resource);
    insert_resource (&iomem_resource, &rodata_resource);
    insert_resource (&iomem_resource, &data_resource);
    insert_resource (&iomem_resource, &bss_resource);

    e820_add_kernel_range ();
    trim_bios_range ();
#ifdef CONFIG_X86_32
    if (ppro_with_ram_bug ()) {
        e820__range_update (0x70000000ULL, 0x40000ULL, E820_TYPE_RAM, E820_TYPE_RESERVED);
        e820__update_table (e820_table);
        printk (KERN_INFO "fixed physical RAM map:\n");
        e820__print_table ("bad_ppro");
    }
#else
    early_gart_iommu_check ();
#endif

    /*
     * partially used pages are not usable - thus
     * we are rounding upwards:
     */
    max_pfn = e820__end_of_ram_pfn ();

    /* update e820 for memory not covered by WB MTRRs */
    cache_bp_init ();
    if (mtrr_trim_uncached_memory (max_pfn)) {
        max_pfn = e820__end_of_ram_pfn ();
    }

    max_possible_pfn = max_pfn;

    /*
     * Define random base addresses for memory sections after max_pfn is
     * defined and before each memory section base is used.
     */
    kernel_randomize_memory ();

#ifdef CONFIG_X86_32
    /* max_low_pfn get updated here */
    find_low_pfn_range ();
#else
    check_x2apic ();

    /* How many end-of-memory variables you have, grandma! */
    /* need this before calling reserve_initrd */
    if (max_pfn > (1UL << (32 - PAGE_SHIFT))) {
        max_low_pfn = e820__end_of_low_ram_pfn ();
    } else {
        max_low_pfn = max_pfn;
    }

    high_memory = (void*)__va (max_pfn * PAGE_SIZE - 1) + 1;
#endif

    /* Find and reserve MPTABLE area */
    x86_init.mpparse.find_mptable ();

    early_alloc_pgt_buf ();

    /*
     * Need to conclude brk, before e820__memblock_setup()
     * it could use memblock_find_in_range, could overlap with
     * brk area.
     */
    reserve_brk ();

    cleanup_highmap ();

    memblock_set_current_limit (ISA_END_ADDRESS);
    e820__memblock_setup ();

    /*
     * Needs to run after memblock setup because it needs the physical
     * memory size.
     */
    mem_encrypt_setup_arch ();
    cc_random_init ();

    efi_find_mirror ();
    efi_esrt_init ();
    efi_mokvar_table_init ();

    /*
     * The EFI specification says that boot service code won't be
     * called after ExitBootServices(). This is, in fact, a lie.
     */
    efi_reserve_boot_services ();

    /* preallocate 4k for mptable mpc */
    e820__memblock_alloc_reserved_mpc_new ();

#ifdef CONFIG_X86_CHECK_BIOS_CORRUPTION
    setup_bios_corruption_check ();
#endif

#ifdef CONFIG_X86_32
    printk (KERN_DEBUG "initial memory mapped: [mem 0x00000000-%#010lx]\n",
            (max_pfn_mapped << PAGE_SHIFT) - 1);
#endif

    /*
     * Find free memory for the real mode trampoline and place it there. If
     * there is not enough free memory under 1M, on EFI-enabled systems
     * there will be additional attempt to reclaim the memory for the real
     * mode trampoline at efi_free_boot_services().
     *
     * Unconditionally reserve the entire first 1M of RAM because BIOSes
     * are known to corrupt low memory and several hundred kilobytes are not
     * worth complex detection what memory gets clobbered. Windows does the
     * same thing for very similar reasons.
     *
     * Moreover, on machines with SandyBridge graphics or in setups that use
     * crashkernel the entire 1M is reserved anyway.
     *
     * Note the host kernel TDX also requires the first 1MB being reserved.
     */
    x86_platform.realmode_reserve ();

    init_mem_mapping ();

    /*
     * init_mem_mapping() relies on the early IDT page fault handling.
     * Now either enable FRED or install the real page fault handler
     * for 64-bit in the IDT.
     */
    cpu_init_replace_early_idt ();

    /*
     * Update mmu_cr4_features (and, indirectly, trampoline_cr4_features)
     * with the current CR4 value.  This may not be necessary, but
     * auditing all the early-boot CR4 manipulation would be needed to
     * rule it out.
     *
     * Mask off features that don't work outside long mode (just
     * PCIDE for now).
     */
    mmu_cr4_features = __read_cr4 () & ~X86_CR4_PCIDE;

    memblock_set_current_limit (get_max_mapped ());

    /*
     * NOTE: On x86-32, only from this point on, fixmaps are ready for use.
     */

#ifdef CONFIG_PROVIDE_OHCI1394_DMA_INIT
    if (init_ohci1394_dma_early)
        init_ohci1394_dma_on_all_controllers ();
#endif
    /* Allocate bigger log buffer */
    setup_log_buf (1);

    if (efi_enabled (EFI_BOOT)) {
        switch (boot_params.secure_boot) {
        case efi_secureboot_mode_disabled: pr_info ("Secure boot disabled\n"); break;
        case efi_secureboot_mode_enabled: pr_info ("Secure boot enabled\n"); break;
        default: pr_info ("Secure boot could not be determined\n"); break;
        }
    }

    reserve_initrd ();

    acpi_table_upgrade ();
    /* Look for ACPI tables and reserve memory occupied by them. */
    acpi_boot_table_init ();

    vsmp_init ();

    io_delay_init ();

    early_platform_quirks ();

    /* Some platforms need the APIC registered for NUMA configuration */
    early_acpi_boot_init ();
    x86_init.mpparse.early_parse_smp_cfg ();

    x86_flattree_get_config ();

    initmem_init ();
    dma_contiguous_reserve (max_pfn_mapped << PAGE_SHIFT);

    if (boot_cpu_has (X86_FEATURE_GBPAGES))
        hugetlb_cma_reserve (PUD_SHIFT - PAGE_SHIFT);

    /*
     * Reserve memory for crash kernel after SRAT is parsed so that it
     * won't consume hotpluggable memory.
     */
    arch_reserve_crashkernel ();

    if (!early_xdbc_setup_hardware ())
        early_xdbc_register_console ();

    x86_init.paging.pagetable_init ();

    kasan_init ();

    /*
     * Sync back kernel address range.
     *
     * FIXME: Can the later sync in setup_cpu_entry_areas() replace
     * this call?
     */
    sync_initial_page_table ();

    tboot_probe ();

    map_vsyscall ();

    x86_32_probe_apic ();

    early_quirks ();

    topology_apply_cmdline_limits_early ();

    /*
     * Parse SMP configuration. Try ACPI first and then the platform
     * specific parser.
     */
    acpi_boot_init ();
    x86_init.mpparse.parse_smp_cfg ();

    /* Last opportunity to detect and map the local APIC */
    init_apic_mappings ();

    topology_init_possible_cpus ();

    init_cpu_to_node ();
    init_gi_nodes ();

    io_apic_init_mappings ();

    x86_init.hyper.guest_late_init ();

    e820__reserve_resources ();
    e820__register_nosave_regions (max_pfn);

    x86_init.resources.reserve_resources ();

    e820__setup_pci_gap ();

#ifdef CONFIG_VT
#    if defined(CONFIG_VGA_CONSOLE)
    if (!efi_enabled (EFI_BOOT) || (efi_mem_type (0xa0000) != EFI_CONVENTIONAL_MEMORY))
        vgacon_register_screen (&screen_info);
#    endif
#endif
    x86_init.oem.banner ();

    x86_init.timers.wallclock_init ();

    /*
     * This needs to run before setup_local_APIC() which soft-disables the
     * local APIC temporarily and that masks the thermal LVT interrupt,
     * leading to softlockups on machines which have configured SMI
     * interrupt delivery.
     */
    therm_lvt_init ();

    mcheck_init ();

    register_refined_jiffies (CLOCK_TICK_RATE);

#ifdef CONFIG_EFI
    if (efi_enabled (EFI_BOOT))
        efi_apply_memmap_quirks ();
#endif

    unwind_init ();
}

#ifdef CONFIG_X86_32

static struct resource video_ram_resource = {.name  = "Video RAM area",
                                             .start = 0xa0000,
                                             .end   = 0xbffff,
                                             .flags = IORESOURCE_BUSY | IORESOURCE_MEM};

void __init            i386_reserve_resources (void)
{
    request_resource (&iomem_resource, &video_ram_resource);
    reserve_standard_io_resources ();
}

#endif /* CONFIG_X86_32 */

static struct notifier_block kernel_offset_notifier = {.notifier_call = dump_kernel_offset};

static int __init            register_kernel_offset_dumper (void)
{
    atomic_notifier_chain_register (&panic_notifier_list, &kernel_offset_notifier);
    return 0;
}
__initcall (register_kernel_offset_dumper);

#ifdef CONFIG_HOTPLUG_CPU
bool arch_cpu_is_hotpluggable (int cpu)
{
    return cpu > 0;
}
#endif /* CONFIG_HOTPLUG_CPU */
