// SPDX-License-Identifier: GPL-2.0
#include <linux/mm_types.h>
#include <linux/maple_tree.h>
#include <linux/rwsem.h>
#include <linux/spinlock.h>
#include <linux/list.h>
#include <linux/cpumask.h>
#include <linux/mman.h>
#include <linux/pgtable.h>

#include <linux/atomic.h>
#include <linux/user_namespace.h>
#include <linux/iommu.h>
#include <asm/mmu.h>

#ifndef INIT_MM_CONTEXT
#    define INIT_MM_CONTEXT(name)
#endif

const struct vm_operations_struct vma_dummy_vm_ops;

/**
 * @brief
 * For dynamically allocated mm_structs, there is a dynamically sized cpumask
 * at the end of the structure, the size of which depends on the maximum CPU
 * number the system can see. That way we allocate only as much memory for
 * mm_cpumask() as needed for the hundreds, or thousands of processes that
 * a system typically runs.
 *
 * Since there is only one init_mm in the entire system, keep it simple
 * and size this cpu_bitmask to NR_CPUS.
 *
 * init_mm 是Linux内核中的一个全局变量，表示内核的内存管理结构。它的主要作用是为内核自身的地址空间提供一个默认的 mm_struct
 * 结构体，通常在以下情况下使用：
 *  1. 内核线程的内存管理：内核线程(如：kthread_create()创建的线程)没有独立的用户态内存空间，因此它们会使用 init_mm 作为它们的 mm 结构
 *  2. 早期引导(Boot)阶段：系统启动的早期阶段，还没有用户进程，所有代码都运行在内核态，此时 init_mm 作为内核的地址空间初始化（例如：分页机制初始化会使用它）
 *  3. 进程上下文切换：当内核线程调度时，如果新的线程没有自己的 mm_struct，那么active_mm仍然会保持 init_mm, 以保证CPU的MMU(内存管理单元)不会访问无效的页表
 *
 * init_mm
 * - pgd(页全局目录)：指向内核页表的pgd，用于地址转换
 * - mmap(内存映射链表)：对于 init_mm, 通常为空，因其不涉及用户态进程的虚拟内存管理
 * - mm_count 和 mm_users：计数器，控制 mm_struct 何时释放，但 init_mm 一般不会释放
 * - start_code, end_code, start_data, end_data：标记内核代码段和数据段的范围
 */
struct mm_struct init_mm = {
    .mm_mt    = MTREE_INIT_EXT (mm_mt, MM_MT_FLAGS, init_mm.mmap_lock),
    .pgd      = swapper_pg_dir,
    .mm_users = ATOMIC_INIT (2),
    .mm_count = ATOMIC_INIT (1),
    .write_protect_seq = SEQCNT_ZERO (init_mm.write_protect_seq),
    MMAP_LOCK_INITIALIZER (init_mm).page_table_lock =
        __SPIN_LOCK_UNLOCKED (init_mm.page_table_lock),
    .arg_lock = __SPIN_LOCK_UNLOCKED (init_mm.arg_lock),
    .mmlist   = LIST_HEAD_INIT (init_mm.mmlist),
#ifdef CONFIG_PER_VMA_LOCK
    .mm_lock_seq = 0,
#endif
    .user_ns    = &init_user_ns,
    .cpu_bitmap = CPU_BITS_NONE,
    INIT_MM_CONTEXT (init_mm)
};

void setup_initial_init_mm (void* start_code, void* end_code, void* end_data, void* brk)
{
    init_mm.start_code = (unsigned long)start_code;
    init_mm.end_code   = (unsigned long)end_code;
    init_mm.end_data   = (unsigned long)end_data;
    init_mm.brk        = (unsigned long)brk; // 堆内存空间
}
