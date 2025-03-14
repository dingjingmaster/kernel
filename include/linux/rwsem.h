/* SPDX-License-Identifier: GPL-2.0 */
/* rwsem.h: R/W semaphores, public interface
 *
 * Written by David Howells (dhowells@redhat.com).
 * Derived from asm-i386/semaphore.h
 */

#ifndef _LINUX_RWSEM_H
#define _LINUX_RWSEM_H

#include <linux/linkage.h>

#include <linux/types.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/atomic.h>
#include <linux/err.h>
#include <linux/cleanup.h>

#ifdef CONFIG_DEBUG_LOCK_ALLOC
#define __RWSEM_DEP_MAP_INIT(lockname)        \
    .dep_map = {                              \
            .name            = #lockname,     \
            .wait_type_inner = LD_WAIT_SLEEP, \
    },
#else
#define __RWSEM_DEP_MAP_INIT(lockname)
#endif

#ifndef CONFIG_PREEMPT_RT

#ifdef CONFIG_RWSEM_SPIN_ON_OWNER
#include <linux/osq_lock.h>
#endif

/**
 * For an uncontended rwsem, count and owner are the only fields a task
 * needs to touch when acquiring the rwsem. So they are put next to each
 * other to increase the chance that they will share the same cacheline.
 *
 * In a contended rwsem, the owner is likely the most frequently accessed
 * field in the structure as the optimistic waiter that holds the osq lock
 * will spin on owner. For an embedded rwsem, other hot fields in the
 * containing structure should be moved further away from the rwsem to
 * reduce the chance that they will share the same cacheline causing
 * cacheline bouncing problem.
 */
struct rw_semaphore
{
    /**
     * 位0: 写锁占用(1表示写锁被持有, 0表示无写锁)
     * 位1: 等待队列存在标志(1表示有进程/线程在等待, 0表示无等待者)
     * 位2: 锁传递标志(用于优化锁获取逻辑)
     * 低62位: 读者计数(最大支持2^62-1个并发读)
     */
    atomic_long_t count;
    /*
     * Write owner or one of the read owners as well flags regarding
     * the current state of the rwsem. Can be used as a speculative
     * check to see if the write owner is running on the cpu.
     */
    atomic_long_t owner;              // 指向当前写锁/读锁持有者进程(读锁持有者中任何一个进程)
#ifdef CONFIG_RWSEM_SPIN_ON_OWNER
    struct optimistic_spin_queue osq; // 自旋锁队列，用于优化锁竞争场景：通过自旋等待减少线程切换开销，适用于锁持有时间短的场景
#endif
    raw_spinlock_t   wait_lock;       // 自旋锁，保护等待队列的并发访问
    struct list_head wait_list;       // 维护等待获取锁的进程/线程队列，新加入的插入尾部，唤醒从头部取出
#ifdef CONFIG_DEBUG_RWSEMS
    void* magic;                      // 用于检测锁结构破坏, 在锁初始化时设置，释放时清除, 正常值为锁的地址，若被修改则触发调试警告
#endif
#ifdef CONFIG_DEBUG_LOCK_ALLOC
    struct lockdep_map dep_map;       // 死锁检测依赖映射，用于锁依赖关系分析, 记录锁的持有者和等待者关系, 辅助检测潜在的死锁链
#endif
};

#define RWSEM_UNLOCKED_VALUE 0UL
#define RWSEM_WRITER_LOCKED (1UL << 0)
#define __RWSEM_COUNT_INIT(name) .count = ATOMIC_LONG_INIT (RWSEM_UNLOCKED_VALUE)

static inline int rwsem_is_locked (struct rw_semaphore* sem)
{
    return atomic_long_read (&sem->count) != RWSEM_UNLOCKED_VALUE;
}

static inline void rwsem_assert_held_nolockdep (const struct rw_semaphore* sem)
{
    WARN_ON (atomic_long_read (&sem->count) == RWSEM_UNLOCKED_VALUE);
}

static inline void rwsem_assert_held_write_nolockdep (const struct rw_semaphore* sem)
{
    WARN_ON (!(atomic_long_read (&sem->count) & RWSEM_WRITER_LOCKED));
}

/* Common initializer macros and functions */

#ifdef CONFIG_DEBUG_RWSEMS
#define __RWSEM_DEBUG_INIT(lockname) .magic = &lockname,
#else
#define __RWSEM_DEBUG_INIT(lockname)
#endif

#ifdef CONFIG_RWSEM_SPIN_ON_OWNER
#define __RWSEM_OPT_INIT(lockname) .osq = OSQ_LOCK_UNLOCKED,
#else
#define __RWSEM_OPT_INIT(lockname)
#endif

#define __RWSEM_INITIALIZER(name)                                                                                                             \
    {__RWSEM_COUNT_INIT (name), .owner = ATOMIC_LONG_INIT (0), __RWSEM_OPT_INIT (name).wait_lock = __RAW_SPIN_LOCK_UNLOCKED (name.wait_lock), \
     .wait_list = LIST_HEAD_INIT ((name).wait_list), __RWSEM_DEBUG_INIT (name) __RWSEM_DEP_MAP_INIT (name)}

#define DECLARE_RWSEM(name) struct rw_semaphore name = __RWSEM_INITIALIZER (name)

extern void __init_rwsem (struct rw_semaphore* sem, const char* name, struct lock_class_key* key);

#define init_rwsem(sem)                     \
    do {                                    \
        static struct lock_class_key __key; \
                                            \
        __init_rwsem ((sem), #sem, &__key); \
    } while (0)

/*
 * This is the same regardless of which rwsem implementation that is being used.
 * It is just a heuristic meant to be called by somebody already holding the
 * rwsem to see if somebody from an incompatible type is wanting access to the
 * lock.
 */
static inline int rwsem_is_contended (struct rw_semaphore* sem)
{
    return !list_empty (&sem->wait_list);
}

#else /* !CONFIG_PREEMPT_RT */

#include <linux/rwbase_rt.h>

struct rw_semaphore
{
    struct rwbase_rt rwbase;
#ifdef CONFIG_DEBUG_LOCK_ALLOC
    struct lockdep_map dep_map;
#endif
};

#define __RWSEM_INITIALIZER(name) {.rwbase = __RWBASE_INITIALIZER (name), __RWSEM_DEP_MAP_INIT (name)}

#define DECLARE_RWSEM(lockname) struct rw_semaphore lockname = __RWSEM_INITIALIZER (lockname)

extern void                __init_rwsem (struct rw_semaphore* rwsem, const char* name, struct lock_class_key* key);

#define init_rwsem(sem)                     \
    do {                                    \
        static struct lock_class_key __key; \
                                            \
        __init_rwsem ((sem), #sem, &__key); \
    } while (0)

static __always_inline int rwsem_is_locked (const struct rw_semaphore* sem)
{
    return rw_base_is_locked (&sem->rwbase);
}

static __always_inline void rwsem_assert_held_nolockdep (const struct rw_semaphore* sem)
{
    WARN_ON (!rwsem_is_locked (sem));
}

static __always_inline void rwsem_assert_held_write_nolockdep (const struct rw_semaphore* sem)
{
    WARN_ON (!rw_base_is_write_locked (&sem->rwbase));
}

static __always_inline int rwsem_is_contended (struct rw_semaphore* sem)
{
    return rw_base_is_contended (&sem->rwbase);
}

#endif /* CONFIG_PREEMPT_RT */

/*
 * The functions below are the same for all rwsem implementations including
 * the RT specific variant.
 */

static inline void rwsem_assert_held (const struct rw_semaphore* sem)
{
    if (IS_ENABLED (CONFIG_LOCKDEP))
        lockdep_assert_held (sem);
    else
        rwsem_assert_held_nolockdep (sem);
}

static inline void rwsem_assert_held_write (const struct rw_semaphore* sem)
{
    if (IS_ENABLED (CONFIG_LOCKDEP))
        lockdep_assert_held_write (sem);
    else
        rwsem_assert_held_write_nolockdep (sem);
}

/*
 * lock for reading
 */
extern void             down_read (struct rw_semaphore* sem);
extern int __must_check down_read_interruptible (struct rw_semaphore* sem);
extern int __must_check down_read_killable (struct rw_semaphore* sem);

/*
 * trylock for reading -- returns 1 if successful, 0 if contention
 */
extern int              down_read_trylock (struct rw_semaphore* sem);

/*
 * lock for writing
 */
extern void             down_write (struct rw_semaphore* sem);
extern int __must_check down_write_killable (struct rw_semaphore* sem);

/*
 * trylock for writing -- returns 1 if successful, 0 if contention
 */
extern int              down_write_trylock (struct rw_semaphore* sem);

/*
 * release a read lock
 */
extern void             up_read (struct rw_semaphore* sem);

/*
 * release a write lock
 */
extern void             up_write (struct rw_semaphore* sem);

DEFINE_GUARD (rwsem_read, struct rw_semaphore*, down_read (_T), up_read (_T))
DEFINE_GUARD_COND (rwsem_read, _try, down_read_trylock (_T))
DEFINE_GUARD_COND (rwsem_read, _intr, down_read_interruptible (_T) == 0)

DEFINE_GUARD (rwsem_write, struct rw_semaphore*, down_write (_T), up_write (_T))
DEFINE_GUARD_COND (rwsem_write, _try, down_write_trylock (_T))

/*
 * downgrade write lock to read lock
 */
extern void downgrade_write (struct rw_semaphore* sem);

#ifdef CONFIG_DEBUG_LOCK_ALLOC
/*
 * nested locking. NOTE: rwsems are not allowed to recurse
 * (which occurs if the same task tries to acquire the same
 * lock instance multiple times), but multiple locks of the
 * same lock class might be taken, if the order of the locks
 * is always the same. This ordering rule can be expressed
 * to lockdep via the _nested() APIs, but enumerating the
 * subclasses that are used. (If the nesting relationship is
 * static then another method for expressing nested locking is
 * the explicit definition of lock class keys and the use of
 * lockdep_set_class() at lock initialization time.
 * See Documentation/locking/lockdep-design.rst for more details.)
 */
extern void             down_read_nested (struct rw_semaphore* sem, int subclass);
extern int __must_check down_read_killable_nested (struct rw_semaphore* sem, int subclass);
extern void             down_write_nested (struct rw_semaphore* sem, int subclass);
extern int              down_write_killable_nested (struct rw_semaphore* sem, int subclass);
extern void             _down_write_nest_lock (struct rw_semaphore* sem, struct lockdep_map* nest_lock);

#define down_write_nest_lock(sem, nest_lock)                    \
    do {                                                        \
        typecheck (struct lockdep_map*, &(nest_lock)->dep_map); \
        _down_write_nest_lock (sem, &(nest_lock)->dep_map);     \
    } while (0)

/*
 * Take/release a lock when not the owner will release it.
 *
 * [ This API should be avoided as much as possible - the
 *   proper abstraction for this case is completions. ]
 */
extern void down_read_non_owner (struct rw_semaphore* sem);
extern void up_read_non_owner (struct rw_semaphore* sem);
#else
#define down_read_nested(sem, subclass) down_read (sem)
#define down_read_killable_nested(sem, subclass) down_read_killable (sem)
#define down_write_nest_lock(sem, nest_lock) down_write (sem)
#define down_write_nested(sem, subclass) down_write (sem)
#define down_write_killable_nested(sem, subclass) down_write_killable (sem)
#define down_read_non_owner(sem) down_read (sem)
#define up_read_non_owner(sem) up_read (sem)
#endif

#endif /* _LINUX_RWSEM_H */
