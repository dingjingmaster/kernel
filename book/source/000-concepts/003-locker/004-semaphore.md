# 信号量

通过计数器控制资源访问数量，支持多进程同步。常用于限制并发访问资源的线程数。

信号量的结构体struct semaphore定义在`include/linux/semaphore.h`

## 定义

```c
struct semaphore
{
    raw_spinlock_t   lock;
    unsigned int     count;
    struct list_head wait_list;
};
```

## API

```c
// 初始化信号量, val表示设定的初始值
void sema_init(struct semaphore *sem, int val);

// 信号量获取(P操作), 尝试获取信号量，若值为0则阻塞进程, 不可被中断
void down(struct semaphore *sem);

// 信号量获取(P操作), 与down()类似, 但允许进程在等待时响应信号中断
int down_interruptible(struct semaphore *sem);

// 信号量获取(P操作), 尝试获取信号量, 若值为0则立即返回失败, 适用于非阻塞场景
int down_trylock(struct semaphore *sem);

// 信号量释放(V操作), 释放信号量, 将值加1并唤醒等待队列中的一个或多个进程
void up (struct semaphore* sem);
```

用户态可通过semget/semctl/semop系统调用操作System V信号量，但内核态直接使用上述API实现高效同步

典型应用场景：
- 进程间资源池管理（如数据库连接池）
- 多阶段任务同步（如生产者-消费者模型）
- 内核模块资源互斥（如设备驱动访问控制）

> 注意: 内核信号量API需在`<linux/semaphore.h>`头文件中声明，且操作需严格遵循获取-释放顺序以避免死锁
