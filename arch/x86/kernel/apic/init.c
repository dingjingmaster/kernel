// SPDX-License-Identifier: GPL-2.0-only
#define pr_fmt(fmt) "APIC: " fmt

#include <asm/apic.h>

#include "local.h"

/*
 * Use DEFINE_STATIC_CALL_NULL() to avoid having to provide stub functions
 * for each callback. The callbacks are setup during boot and all except
 * wait_icr_idle() must be initialized before usage. The IPI wrappers
 * use static_call() and not static_call_cond() to catch any fails.
 */
#define DEFINE_APIC_CALL(__cb)						\
	DEFINE_STATIC_CALL_NULL(apic_call_##__cb, *apic->__cb)

DEFINE_APIC_CALL (eoi);
DEFINE_APIC_CALL (native_eoi);
DEFINE_APIC_CALL (icr_read);
DEFINE_APIC_CALL (icr_write);
DEFINE_APIC_CALL (read);
DEFINE_APIC_CALL (send_IPI);
DEFINE_APIC_CALL (send_IPI_mask);
DEFINE_APIC_CALL (send_IPI_mask_allbutself);
DEFINE_APIC_CALL (send_IPI_allbutself);
DEFINE_APIC_CALL (send_IPI_all);
DEFINE_APIC_CALL (send_IPI_self);
DEFINE_APIC_CALL (wait_icr_idle);
DEFINE_APIC_CALL (wakeup_secondary_cpu);
DEFINE_APIC_CALL (wakeup_secondary_cpu_64);
DEFINE_APIC_CALL (write);

EXPORT_STATIC_CALL_TRAMP_GPL (apic_call_send_IPI_mask);
EXPORT_STATIC_CALL_TRAMP_GPL (apic_call_send_IPI_self);

/* The container for function call overrides */
struct apic_override __x86_apic_override __initdata;

#define apply_override(__cb)      \
    if (__x86_apic_override.__cb) \
    apic->__cb = __x86_apic_override.__cb

static __init void restore_override_callbacks (void)
{
    apply_override (eoi);
    apply_override (native_eoi);
    apply_override (write);
    apply_override (read);
    apply_override (send_IPI);
    apply_override (send_IPI_mask);
    apply_override (send_IPI_mask_allbutself);
    apply_override (send_IPI_allbutself);
    apply_override (send_IPI_all);
    apply_override (send_IPI_self);
    apply_override (icr_read);
    apply_override (icr_write);
    apply_override (wakeup_secondary_cpu);
    apply_override (wakeup_secondary_cpu_64);
}

#define update_call(__cb) static_call_update (apic_call_##__cb, *apic->__cb)

static __init void update_static_calls (void)
{
    // apic_call_eoi(), 项本地APIC发送EOS(终止中断)，通知APIC当前中断已处理完毕，以便处理新的中断
    update_call (eoi);
    // apic_call_native_eoi() 作用类似 eoi，但特定于原生APIC，而非虚拟化或其他模式
    update_call (native_eoi);
    // apic_call_write() 主要用于向本地APIC写入EOI(终止中断)信号
    update_call (write);
    // apic_call_read() 主要用于从本地APIC读取EOI(终止中断)信号寄存器的值，
    // 以检查当前中断是否已被处理或APIC的EOI状态
    update_call (read);
    // 先发送 EOI，再发送 IPI，从当前CPU向其他CPU发送一个中断信号，
    // 常用于多核系统中的同步或唤醒其他CPU处理任务
    update_call (send_IPI);
    // 类似 send_IPI，但是允许向一组CPU(通过掩码指定)发送IPI，而不止一个CPU
    update_call (send_IPI_mask);
    // 该函数会向除了当前CPU之外的所有CPU发送IPI，要求它们处理一个特定的任务
    update_call (send_IPI_mask_allbutself);
    // 向所有其它CPU发送IPI，通常适用于多核系统中的同步、TLB刷新或负载均衡
    update_call (send_IPI_allbutself);
    // 该函数通过APIC机制向所有CPU(包括当前CPU)广播一个IPI。
    // 这个IPI通常用于融发全局同步任务，
    // 比如全系统范围的TLB刷新、缓存同步或其它需要所有CPU系统执行的工作
    update_call (send_IPI_all);
    // 该函数会向当前 CPU 自己发送一个 IPI。这种自我中断用于触发当前CPU上的某个回调或延迟处理函数
    update_call (send_IPI_self);
    // 首先通过写入APIC的EOI寄存器，通知本地APIC当前中断已经被处理完毕
    // 随后，函数会向一个或多个目标CPU发送IPI。
    // 这通常通过写入APIC的ICR(中断命令寄存器)来实现，指示目标CPU执行特定的中断服务程序或同步操作
    // 该函数在发送 IPI 后，立即读取ICR寄存器。此读取操作的目的是：
    //  - 同步屏障：确保之前的写操作(发送IPI)已经生效，避免因为CPU或总线连续执行导致的操作不一致
    //  - 状态确认：有时读取ICR也适用于检查IPI发送的状态，比如：确认APIC是否完成了IPI命令的调用
    update_call (icr_read);
    // 主要用于在中断处理结束后，通过直接写入ICR(中断命令寄存器)来发送IPI，从而实现跨CPU中断通信和同步
    // 主要功能包括：
    //  1. 发送EOI：该函数首先向当前CPU的本地APIC发送 EOI信号(通过写入EOI寄存器)，以通知APIC当前
    //     中断已经完成。这一步用于清除中断状态、允许新的中断到来至关重要
    //  2. 通过ICR写入发送IPI：发送完EOI后，函数会直接向APIC的ICR写入一条命令，触发一个IPI。
    //     这种写操作通常包含目标CPU(或CPU集合)和中断向量信息，确保目标CPU能够响应该中断请求
    //     并执行预定的同步或回调操作。使用ICR写操作可以提供对IPI发送过程的精细控制，并确保写入操作的
    //     顺序性(避免乱序执行), 从而保证跨CPU通信的正确性
    update_call (icr_write);
    // 1. 向当前CPU的本地APIC写入EOI信号，通知APIC当前中断已处理完毕，从而清除中断状态
    // 2. 等待ICR(Interrupt Command Register)空闲。确保同步、操作有序性
    update_call (wait_icr_idle);
    // 1. 发送 EOI 信号
    // 2. 唤醒辅助 CPU，通常用于通知那些处于等待状态的辅助CPU开始执行启动代码或从低功耗状态中恢复
    update_call (wakeup_secondary_cpu);
    // 同上，适用于64位内核环境
    update_call (wakeup_secondary_cpu_64);
}

void __init apic_setup_apic_calls (void)
{
    /* Ensure that the default APIC has native_eoi populated */
    apic->native_eoi = apic->eoi;
    update_static_calls ();
    pr_info ("Static calls initialized\n");
}

void __init apic_install_driver (struct apic* driver)
{
    if (apic == driver)
        return;

    apic = driver;

    if (IS_ENABLED (CONFIG_X86_X2APIC) && apic->x2apic_set_max_apicid)
        apic->max_apic_id = x2apic_max_apicid;

    /* Copy the original eoi() callback as KVM/HyperV might overwrite it */
    if (!apic->native_eoi)
        apic->native_eoi = apic->eoi;

    /* Apply any already installed callback overrides */
    restore_override_callbacks ();
    update_static_calls ();

    pr_info ("Switched APIC routing to: %s\n", driver->name);
}
