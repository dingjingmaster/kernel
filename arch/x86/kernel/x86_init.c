/*
 * Copyright (C) 2009 Thomas Gleixner <tglx@linutronix.de>
 *
 *  For licencing details see kernel-base/COPYING
 */
#include <linux/dmi.h>
#include <linux/init.h>
#include <linux/ioport.h>
#include <linux/export.h>
#include <linux/pci.h>
#include <linux/acpi.h>

#include <asm/acpi.h>
#include <asm/bios_ebda.h>
#include <asm/paravirt.h>
#include <asm/pci_x86.h>
#include <asm/mpspec.h>
#include <asm/setup.h>
#include <asm/apic.h>
#include <asm/e820/api.h>
#include <asm/time.h>
#include <asm/irq.h>
#include <asm/io_apic.h>
#include <asm/hpet.h>
#include <asm/memtype.h>
#include <asm/tsc.h>
#include <asm/iommu.h>
#include <asm/mach_traps.h>
#include <asm/irqdomain.h>
#include <asm/realmode.h>

void x86_init_noop (void)
{
}
void __init x86_init_uint_noop (unsigned int unused)
{
}
static int __init iommu_init_noop (void)
{
    return 0;
}
static void iommu_shutdown_noop (void)
{
}
bool __init bool_x86_init_noop (void)
{
    return false;
}
void x86_op_int_noop (int cpu)
{
}
int set_rtc_noop (const struct timespec64* now)
{
    return -EINVAL;
}
void get_rtc_noop (struct timespec64* now)
{
}

static __initconst const struct of_device_id of_cmos_match[] = {{.compatible = "motorola,mc146818"}, {}};

/*
 * Allow devicetree configured systems to disable the RTC by setting the
 * corresponding DT node's status property to disabled. Code is optimized
 * out for CONFIG_OF=n builds.
 */
static __init void                           x86_wallclock_init (void)
{
    struct device_node* node = of_find_matching_node (NULL, of_cmos_match);

    if (node && !of_device_is_available (node)) {
        x86_platform.get_wallclock = get_rtc_noop;
        x86_platform.set_wallclock = set_rtc_noop;
    }
}

/*
 * The platform setup functions are preset with the default functions
 * for standard PC hardware.
 */
struct x86_init_ops x86_init __initdata = {

    .resources = {
        .probe_roms         = probe_roms,
        .reserve_resources  = reserve_standard_io_resources,
        .memory_setup       = e820__memory_setup_default,
        .dmi_setup          = dmi_setup,
    },

    .mpparse = {
        .setup_ioapic_ids    = x86_init_noop,
        .find_mptable        = mpparse_find_mptable,
        .early_parse_smp_cfg    = mpparse_parse_early_smp_config,
        .parse_smp_cfg        = mpparse_parse_smp_config,
    },

    .irqs = {
        .pre_vector_init    = init_ISA_irqs,
        .intr_init        = native_init_IRQ,
        .intr_mode_select    = apic_intr_mode_select,
        .intr_mode_init        = apic_intr_mode_init,
        .create_pci_msi_domain    = native_create_pci_msi_domain,
    },

    .oem = {
        .arch_setup        = x86_init_noop,
        .banner            = default_banner,
    },

    .paging = {
        .pagetable_init        = native_pagetable_init,
    },

    .timers = {
        .setup_percpu_clockev    = setup_boot_APIC_clock,
        .timer_init        = hpet_time_init,
        .wallclock_init        = x86_wallclock_init,
    },

    .iommu = {
        .iommu_init        = iommu_init_noop,
    },

    .pci = {
        .init            = x86_default_pci_init,
        .init_irq        = x86_default_pci_init_irq,
        .fixup_irqs        = x86_default_pci_fixup_irqs,
    },

    .hyper = {
        .init_platform        = x86_init_noop,
        .guest_late_init    = x86_init_noop,
        .x2apic_available    = bool_x86_init_noop,
        .msi_ext_dest_id    = bool_x86_init_noop,
        .init_mem_mapping    = x86_init_noop,
        .init_after_bootmem    = x86_init_noop,
    },

    .acpi = {
        .set_root_pointer    = x86_default_set_root_pointer,
        .get_root_pointer    = x86_default_get_root_pointer,
        .reduced_hw_early_init    = acpi_generic_reduced_hw_init,
    },
};

struct x86_cpuinit_ops x86_cpuinit = {
        .early_percpu_clock_init = x86_init_noop,
        .setup_percpu_clockev    = setup_secondary_APIC_clock,
        .parallel_bringup        = true,
};

static void default_nmi_init (void) {};

static int  enc_status_change_prepare_noop (unsigned long vaddr, int npages, bool enc)
{
    return 0;
}
static int enc_status_change_finish_noop (unsigned long vaddr, int npages, bool enc)
{
    return 0;
}
static bool enc_tlb_flush_required_noop (bool enc)
{
    return false;
}
static bool enc_cache_flush_required_noop (void)
{
    return false;
}
static void enc_kexec_begin_noop (void)
{
}
static void enc_kexec_finish_noop (void)
{
}
static bool is_private_mmio_noop (u64 addr)
{
    return false;
}

struct x86_platform_ops x86_platform __ro_after_init = {
    .calibrate_cpu            = native_calibrate_cpu_early,
    .calibrate_tsc            = native_calibrate_tsc,
    .get_wallclock            = mach_get_cmos_time,
    .set_wallclock            = mach_set_cmos_time,
    .iommu_shutdown            = iommu_shutdown_noop,
    .is_untracked_pat_range        = is_ISA_range,
    .nmi_init            = default_nmi_init,
    .get_nmi_reason            = default_get_nmi_reason,
    .save_sched_clock_state        = tsc_save_sched_clock_state,
    .restore_sched_clock_state    = tsc_restore_sched_clock_state,
    .realmode_reserve        = reserve_real_mode,
    .realmode_init            = init_real_mode,
    .hyper.pin_vcpu            = x86_op_int_noop,
    .hyper.is_private_mmio        = is_private_mmio_noop,

    .guest = {
        .enc_status_change_prepare = enc_status_change_prepare_noop,
        .enc_status_change_finish  = enc_status_change_finish_noop,
        .enc_tlb_flush_required       = enc_tlb_flush_required_noop,
        .enc_cache_flush_required  = enc_cache_flush_required_noop,
        .enc_kexec_begin       = enc_kexec_begin_noop,
        .enc_kexec_finish       = enc_kexec_finish_noop,
    },
};

EXPORT_SYMBOL_GPL (x86_platform);

struct x86_apic_ops x86_apic_ops __ro_after_init = {
        .io_apic_read = native_io_apic_read,
        .restore      = native_restore_boot_irq_mode,
};
