# 读写锁

允许多个读操作并发执行，但写操作独占资源。适用于读多写少的场景，通过分离读/写锁提升并发性能。

定义位置：`include/linux/rwsem.h`

## 结构

```c
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
```

## API

```c
// 初始化
init_rwsem(sem)

// 检测是否上锁
int rwsem_is_locked (struct rw_semaphore* sem);

// 读锁
extern void             down_read (struct rw_semaphore* sem);
extern int __must_check down_read_interruptible (struct rw_semaphore* sem);
extern int __must_check down_read_killable (struct rw_semaphore* sem);

// 尝试读锁
extern int              down_read_trylock (struct rw_semaphore* sem);

// 写锁
extern void             down_write (struct rw_semaphore* sem);
extern int __must_check down_write_killable (struct rw_semaphore* sem);

// 尝试写
extern int              down_write_trylock (struct rw_semaphore* sem);

// 释放读
extern void             up_read (struct rw_semaphore* sem);

// 释放写
extern void             up_write (struct rw_semaphore* sem);

// 释放写，上读锁
extern void downgrade_write (struct rw_semaphore* sem);
```

