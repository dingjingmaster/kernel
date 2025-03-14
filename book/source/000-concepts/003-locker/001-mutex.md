# 互斥锁

用于保护共享资源，确保同一时刻仅有一个线程或进程访问。通过原子操作实现，性能较高但可能引发死锁。

定义位置：`include/linux/mutex.h`、`include/linux/mutex_types.h`

## 互斥锁结构
```c
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
    void *magic; // 调试模式下使用，快速检测锁的非法使用，比如：未初始化、重复初始化
#endif
#ifdef CONFIG_DEBUG_LOCK_ALLOC
    struct lockdep_map dep_map; // 用于锁依赖关系分析，帮助检测潜在的死锁
#endif
};
```

## 初始化

```c
// 注意：不可重复初始化
#define mutex_init(mutex)                      \
    do {                                       \
        static struct lock_class_key __key;    \
                                               \
        __mutex_init((mutex), #mutex, &__key); \
    } while (0)
```

## 检测是否上锁

```c
bool mutex_is_locked(struct mutex *lock);
```

## 上锁

无法立即上锁，则将当前线程/进程加入等待队列，并睡眠。
```c
void mutex_lock(struct mutex *lock);

int mutex_trylock(struct mutex *lock);
```

## 解锁

```c
void mutex_unlock(struct mutex *lock);
```
