/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __LINUX_MUTEX_TYPES_H
#define __LINUX_MUTEX_TYPES_H

#include <linux/atomic.h>
#include <linux/lockdep_types.h>
#include <linux/osq_lock.h>
#include <linux/spinlock_types.h>
#include <linux/types.h>

#ifndef CONFIG_PREEMPT_RT

/**
 * Simple, straightforward mutexes with strict semantics (简单直接的互斥锁，具有严格的语义规则):
 *
 * - only one task can hold the mutex at a time (同一时刻只能有一个任务持有互斥锁)
 * - only the owner can unlock the mutex (只有锁持有者才能解锁)
 * - multiple unlocks are not permitted (不允许多次解锁)
 * - recursive locking is not permitted (不允许递归加锁)
 * - a mutex object must be initialized via the API (必须通过官方API初始化)
 * - a mutex object must not be initialized via memset or copying (不允许使用memset或复制初始化)
 * - task may not exit with mutex held (持有锁的任务不能退出)
 * - memory areas where held locks reside must not be freed (锁相关内存区域不可释放)
 * - held mutexes must not be reinitialized (锁不可重新初始化)
 * - mutexes may not be used in hardware or software interrupt contexts such as tasklets and timers (不可用于中断上下文（如tasklet、定时器等）)
 *
 * These semantics are fully enforced when DEBUG_MUTEXES is
 * enabled. Furthermore, besides enforcing the above rules, the mutex
 * debugging code also implements a number of additional features
 * that make lock debugging easier and faster (当启用 DEBUG_MUTEXES 时，
 * 这些语义将得到全面执行。此外，除了强制执行上述规则外，
 * 互斥锁调试代码还实现了一些额外的功能，使锁调试更轻松、更快捷):
 *
 * - uses symbolic names of mutexes, whenever they are printed in debug output (在调试输出中打印互斥锁时，始终使用其符号名称)
 * - point-of-acquire tracking, symbolic lookup of function names (获取点跟踪，函数名称的符号查找)
 * - list of all locks held in the system, printout of them (打印输出系统中所有持有的锁)
 * - owner tracking (锁的持有者)
 * - detects self-recursing locks and prints out all relevant info (检测自递归锁并打印出所有相关信息)
 * - detects multi-task circular deadlocks and prints out all affected locks and tasks (and only those tasks) (检测多任务循环死锁并打印出所有受影响的锁和任务（并且仅打印出这些任务）)
 */
struct mutex
{
    atomic_long_t owner;      // 锁持有者
    raw_spinlock_t wait_lock; // 自旋锁，用于保护等待队列的并发访问。
                              // 当线程尝试获取锁失败时，会被加入等待队列，此时需通过wait_lock保证队列操作的原子性
#ifdef CONFIG_MUTEX_SPIN_ON_OWNER
    struct optimistic_spin_queue osq; /* Spinner MCS lock */
#endif
    struct list_head wait_list; // 双向链表，用于维护等待获取锁的进程/线程队列
#ifdef CONFIG_DEBUG_MUTEXES
    void* magic; // 调试模式下使用，快速检测锁的非法使用，比如：未初始化、重复初始化
#endif
#ifdef CONFIG_DEBUG_LOCK_ALLOC
    struct lockdep_map dep_map; // 用于锁依赖关系分析，帮助检测潜在的死锁
#endif
};

#else /* !CONFIG_PREEMPT_RT */
/*
 * Preempt-RT variant based on rtmutexes.
 */
#include <linux/rtmutex.h>

struct mutex
{
    struct rt_mutex_base rtmutex;
#ifdef CONFIG_DEBUG_LOCK_ALLOC
    struct lockdep_map dep_map;
#endif
};

#endif /* CONFIG_PREEMPT_RT */

#endif /* __LINUX_MUTEX_TYPES_H */
