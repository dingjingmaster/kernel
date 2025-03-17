/*
 * Common hypervisor code
 *
 * Copyright (C) 2008, VMware, Inc.
 * Author : Alok N Kataria <akataria@vmware.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE, GOOD TITLE or
 * NON INFRINGEMENT.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */

#include <linux/init.h>
#include <linux/export.h>
#include <asm/processor.h>
#include <asm/hypervisor.h>

static const __initconst struct hypervisor_x86* const hypervisors[] = {
#ifdef CONFIG_XEN_PV
        &x86_hyper_xen_pv,
#endif
#ifdef CONFIG_XEN_PVHVM
        &x86_hyper_xen_hvm,
#endif
        &x86_hyper_vmware,    &x86_hyper_ms_hyperv,
#ifdef CONFIG_KVM_GUEST
        &x86_hyper_kvm,
#endif
#ifdef CONFIG_JAILHOUSE_GUEST
        &x86_hyper_jailhouse,
#endif
#ifdef CONFIG_ACRN_GUEST
        &x86_hyper_acrn,
#endif
};

enum x86_hypervisor_type x86_hyper_type;
EXPORT_SYMBOL (x86_hyper_type);

bool __initdata   nopv;
static __init int parse_nopv (char* arg)
{
    nopv = true;
    return 0;
}
early_param ("nopv", parse_nopv);

static inline const struct hypervisor_x86* __init detect_hypervisor_vendor (void)
{
    const struct hypervisor_x86 *h = NULL, *const * p;
    uint32_t                     pri, max_pri = 0;

    for (p = hypervisors; p < hypervisors + ARRAY_SIZE (hypervisors); p++) {
        if (unlikely (nopv) && !(*p)->ignore_nopv)
            continue;

        pri = (*p)->detect ();
        if (pri > max_pri) {
            max_pri = pri;
            h       = *p;
        }
    }

    if (h)
        pr_info ("Hypervisor detected: %s\n", h->name);

    return h;
}

static void __init copy_array (const void* src, void* target, unsigned int size)
{
    unsigned int       i, n = size / sizeof (void*);
    const void* const* from = (const void* const*)src;
    const void**       to   = (const void**)target;

    for (i = 0; i < n; i++)
        if (from[i])
            to[i] = from[i];
}

/**
 * init_hypervisor_platform 函数是 Linux 内核中用于初始化和配置虚拟化平台（如 KVM、Xen、Hyper-V）的核心函数，其功能可概括为以下方面:
 *  - 检测 Hypervisor 存在(通过 CPU 特征标志（如 x86 的 VMX 或 ARM 的 HVC 寄存器）或 ACPI 表（如 hypervisor_platform）判断系统是否运行在虚拟化环境中)
 *  - 根据检测到的 Hypervisor 类型，初始化对应的数据结构（如 KVM 的 vcpu 数组、Xen 的 xen_domain_info）
 *  - 设置 CPU 控制寄存器（如 x86 的 CR4.VMX、ARM 的 SCTL.HVEN）以启用 VMX 或 HVC 支持
 *  - 初始化嵌套页表（NPT）或硬件虚拟化支持的页表结构（如 EPT、VHPT）
 *  - 注册中断处理函数（如 KVM 的 kvm_vm_exit）处理 VM exit、VM entry 等事件
 *  - 配置 HPET 以提供精确的虚拟化时钟事件
 *  - 标记 Hypervisor 使用的内存区域（如 Xen 的 shared_info 结构）为保留区，防止内核误分配
 *  - 设置用户态与 Hypervisor 通信的 upcall 页（如 KVM 的 (unsigned long)hypervisor_entry）
 */
void __init init_hypervisor_platform (void)
{
    const struct hypervisor_x86* h;

    h = detect_hypervisor_vendor ();

    if (!h)
        return;

    copy_array (&h->init, &x86_init.hyper, sizeof (h->init));
    copy_array (&h->runtime, &x86_platform.hyper, sizeof (h->runtime));

    x86_hyper_type = h->type;
    x86_init.hyper.init_platform ();
}
