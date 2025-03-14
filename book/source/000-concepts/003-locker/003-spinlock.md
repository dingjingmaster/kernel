# 自旋锁

线程在等待锁时保持忙等待（不睡眠），适用于临界区代码短且持有时间短的场景。无法用于可睡眠的代码。

自旋锁在单CPU和多CPU系统中的不同实现，分别位于`include/linux/spinlock_api_up.h`和`include/linux/spinlock_api_smp.h`

定义在`include/linux/spinlock_types.h`中

## 定义

```c
// 单处理器
typedef struct spinlock
{
    union
    {
        struct raw_spinlock rlock; // 空实现

#ifdef CONFIG_DEBUG_LOCK_ALLOC
#define LOCK_PADSIZE (offsetof (struct raw_spinlock, dep_map))
        struct
        {
            u8                 __padding[LOCK_PADSIZE];
            struct lockdep_map dep_map;
        };
#endif
    };
} spinlock_t;

// 多处理器使用汇编指令实现
```

## API

```c
// 初始化
spin_lock_init(spinlock_t *lock);

// 加锁, 基础加锁，适用于进程上下文且无中断干扰的场景
spin_lock(spinlock_t *lock);

// 加锁, 加锁时禁用本地硬件中断，防止中断上下文竞争
spin_lock_irq(spinlock_t *lock);

// 加锁, 保存当前中断状态并加锁，适用于需要保留中断状态的场景
spin_lock_bh(spinlock_t *lock);

// 解锁, 释放锁，唤醒等待队列中的线程
spin_unlock(spinlock_t *lock);

// 解锁, 恢复之前保存的中断状态并释放锁
spin_unlock_irqrestore(spinlock_t *lock, unsigned long flags);

// 解锁, 释放锁并重新启用本地中断
spin_unlock_irq(spinlock_t *lock);

// 释放锁并重新启用本地软中断
spin_unlock_bh(spinlock_t *lock);

// 判断锁是否被持有，返回布尔值
spin_is_locked(spinlock_t *lock);
```

主要操作机制：
- 忙等待：当锁被占用时，线程会通过循环不断检查锁状态（如while (test_and_set(&lock))），直至获取锁。此机制适用于临界区执行时间极短的场景
- 原子操作实现
	- x86架构：通过LOCK前缀指令（如LOCK CMPXCHG）实现原子性
    - ARM架构：使用LDREX和STREX指令实现原子读-修改-写操作
- 中断与抢占控制
	- 加锁时可能禁用中断（spin_lock_irq）或软中断（spin_lock_bh），避免上下文切换导致的竞争
    - 在单核UP（非抢占）场景下，spin_lock退化为仅禁用内核抢占（preempt_disable）
- 锁状态管理
	- raw_spinlock结构体通过原子计数器（如x86的head_tail票务锁）编码锁状态，区分读写锁与自旋锁
    - break_lock字段用于检测锁饥饿（长时间等待）

使用场景：
- SMP多核系统中保护短临界区（如数据结构更新）
- 高性能场景（如网络协议栈）

注意事项：
- 不可重入，多次获取会导致死锁
- 锁持有期间禁止阻塞或睡眠，否则可能引发优先级反转
- 需配合local_irq_disable()等函数使用，避免中断干扰
