/* SPDX-License-Identifier: GPL-2.0 */
/*
 * NOTE:
 *
 * This header has combined a lot of unrelated to each other stuff.
 * The process of splitting its content is in progress while keeping
 * backward compatibility. That's why it's highly recommended NOT to
 * include this header inside another header file, especially under
 * generic or architectural include/ directory.
 *
 * 注意:
 *
 * 这个头文件历史上聚合了大量彼此并不相关的内容。当前仍在逐步拆分，
 * 同时保持向后兼容。因此强烈建议不要在其它头文件中包含它，尤其不要
 * 在 generic 或架构相关的 include/ 目录下这样做。
 */
#ifndef _LINUX_KERNEL_H
#define _LINUX_KERNEL_H

#include <linux/stdarg.h>
#include <linux/align.h>
#include <linux/array_size.h>
#include <linux/limits.h>
#include <linux/linkage.h>
#include <linux/stddef.h>
#include <linux/types.h>
#include <linux/compiler.h>
#include <linux/container_of.h>
#include <linux/bitops.h>
#include <linux/hex.h>
#include <linux/kstrtox.h>
#include <linux/log2.h>
#include <linux/math.h>
#include <linux/minmax.h>
#include <linux/typecheck.h>
#include <linux/panic.h>
#include <linux/printk.h>
#include <linux/build_bug.h>
#include <linux/sprintf.h>
#include <linux/static_call_types.h>
#include <linux/instruction_pointer.h>
#include <linux/wordpart.h>

#include <asm/byteorder.h>

#include <uapi/linux/kernel.h>

#define STACK_MAGIC 0xdeadbeef

/* generic data direction definitions */
/* 通用数据方向定义。 */
#define READ 0
#define WRITE 1

#define PTR_IF(cond, ptr) ((cond) ? (ptr) : NULL)

#define u64_to_user_ptr(x)            \
    ({                                \
        typecheck (u64, (x));         \
        (void __user*)(uintptr_t)(x); \
    })

struct completion;
struct user;

#ifdef CONFIG_PREEMPT_VOLUNTARY_BUILD

extern int __cond_resched (void);
#    define might_resched() __cond_resched ()

#elif defined(CONFIG_PREEMPT_DYNAMIC) && defined(CONFIG_HAVE_PREEMPT_DYNAMIC_CALL)

extern int __cond_resched (void);

DECLARE_STATIC_CALL (might_resched, __cond_resched);

static __always_inline void might_resched (void)
{
    static_call_mod (might_resched) ();
}

#elif defined(CONFIG_PREEMPT_DYNAMIC) && defined(CONFIG_HAVE_PREEMPT_DYNAMIC_KEY)

extern int dynamic_might_resched (void);
#    define might_resched() dynamic_might_resched ()

#else

#    define might_resched() \
        do {                \
        } while (0)

#endif /* CONFIG_PREEMPT_* */
/* CONFIG_PREEMPT_* 相关的 might_resched() 定义结束。 */

#ifdef CONFIG_DEBUG_ATOMIC_SLEEP
extern void __might_resched (const char* file, int line, unsigned int offsets);
extern void __might_sleep (const char* file, int line);
extern void __cant_sleep (const char* file, int line, int preempt_offset);
extern void __cant_migrate (const char* file, int line);

/**
 * might_sleep - annotation for functions that can sleep
 *
 * this macro will print a stack trace if it is executed in an atomic
 * context (spinlock, irq-handler, ...). Additional sections where blocking is
 * not allowed can be annotated with non_block_start() and non_block_end()
 * pairs.
 *
 * This is a useful debugging help to be able to catch problems early and not
 * be bitten later when the calling function happens to sleep when it is not
 * supposed to.
 *
 * might_sleep - 标注当前函数允许睡眠
 *
 * 如果该宏在原子上下文中执行，例如持有自旋锁或处于中断处理程序中，
 * 会打印调用栈。其它禁止阻塞的代码区间可以用 non_block_start() 和
 * non_block_end() 成对标注。
 *
 * 这是一个很有用的调试辅助，能够尽早发现“不该睡眠却发生睡眠”的路径，
 * 避免问题在更晚阶段才暴露。
 */
#    define might_sleep()                       \
        do {                                    \
            __might_sleep (__FILE__, __LINE__); \
            might_resched ();                   \
        } while (0)
/**
 * cant_sleep - annotation for functions that cannot sleep
 *
 * this macro will print a stack trace if it is executed with preemption enabled
 *
 * cant_sleep - 标注当前函数不能睡眠
 *
 * 如果该宏在抢占已启用的情况下执行，会打印调用栈。
 */
#    define cant_sleep()                          \
        do {                                      \
            __cant_sleep (__FILE__, __LINE__, 0); \
        } while (0)
#    define sched_annotate_sleep() (current->task_state_change = 0)

/**
 * cant_migrate - annotation for functions that cannot migrate
 *
 * Will print a stack trace if executed in code which is migratable
 *
 * cant_migrate - 标注当前函数不能迁移到其它 CPU
 *
 * 如果在允许迁移的代码路径中执行该宏，会打印调用栈。
 */
#    define cant_migrate()                           \
        do {                                         \
            if (IS_ENABLED (CONFIG_SMP))             \
                __cant_migrate (__FILE__, __LINE__); \
        } while (0)

/**
 * non_block_start - annotate the start of section where sleeping is prohibited
 *
 * This is on behalf of the oom reaper, specifically when it is calling the mmu
 * notifiers. The problem is that if the notifier were to block on, for example,
 * mutex_lock() and if the process which holds that mutex were to perform a
 * sleeping memory allocation, the oom reaper is now blocked on completion of
 * that memory allocation. Other blocking calls like wait_event() pose similar
 * issues.
 *
 * non_block_start - 标注禁止睡眠代码区间的开始
 *
 * 该机制主要服务于 oom reaper，尤其是它调用 mmu notifier 的场景。
 * 如果 notifier 在 mutex_lock() 等阻塞调用上等待，而持有该互斥锁的
 * 进程又正在执行可睡眠的内存分配，oom reaper 就会被该分配完成过程阻塞。
 * wait_event() 等其它阻塞调用也有类似问题。
 */
#    define non_block_start() (current->non_block_count++)
/**
 * non_block_end - annotate the end of section where sleeping is prohibited
 *
 * Closes a section opened by non_block_start().
 *
 * non_block_end - 标注禁止睡眠代码区间的结束
 *
 * 用于关闭由 non_block_start() 打开的区间。
 */
#    define non_block_end() WARN_ON (current->non_block_count-- == 0)
#else
static inline void __might_resched (const char* file, int line, unsigned int offsets)
{
}
static inline void __might_sleep (const char* file, int line){}
#    define might_sleep()     \
        do {                  \
            might_resched (); \
        } while (0)
#    define cant_sleep() \
        do {             \
        } while (0)
#    define cant_migrate() \
        do {               \
        } while (0)
#    define sched_annotate_sleep() \
        do {                       \
        } while (0)
#    define non_block_start() \
        do {                  \
        } while (0)
#    define non_block_end() \
        do {                \
        } while (0)
#endif

#define might_sleep_if(cond) \
    do {                     \
        if (cond)            \
            might_sleep ();  \
    } while (0)

#if defined(CONFIG_MMU) && (defined(CONFIG_PROVE_LOCKING) || defined(CONFIG_DEBUG_ATOMIC_SLEEP))
#    define might_fault() __might_fault (__FILE__, __LINE__)
void __might_fault (const char* file, int line);
#else
static inline void might_fault (void)
{
}
#endif

void        do_exit (long error_code) __noreturn;

extern int  core_kernel_text (unsigned long addr);
extern int  __kernel_text_address (unsigned long addr);
extern int  kernel_text_address (unsigned long addr);
extern int  func_ptr_is_kernel_text (void* ptr);

extern void bust_spinlocks (int yes);

extern int  root_mountflags;

extern bool early_boot_irqs_disabled;

/*
 * Values used for system_state. Ordering of the states must not be changed
 * as code checks for <, <=, >, >= STATE.
 *
 * system_state 使用的取值。状态顺序不能改变，因为已有代码会使用
 * <、<=、>、>= 等关系判断当前系统阶段。
 */
extern enum system_states {
    SYSTEM_BOOTING,
    SYSTEM_SCHEDULING,
    SYSTEM_FREEING_INITMEM,
    SYSTEM_RUNNING,
    SYSTEM_HALT,
    SYSTEM_POWER_OFF,
    SYSTEM_RESTART,
    SYSTEM_SUSPEND,
} system_state;

/*
 * General tracing related utility functions - trace_printk(),
 * tracing_on/tracing_off and tracing_start()/tracing_stop
 *
 * Use tracing_on/tracing_off when you want to quickly turn on or off
 * tracing. It simply enables or disables the recording of the trace events.
 * This also corresponds to the user space /sys/kernel/tracing/tracing_on
 * file, which gives a means for the kernel and userspace to interact.
 * Place a tracing_off() in the kernel where you want tracing to end.
 * From user space, examine the trace, and then echo 1 > tracing_on
 * to continue tracing.
 *
 * tracing_stop/tracing_start has slightly more overhead. It is used
 * by things like suspend to ram where disabling the recording of the
 * trace is not enough, but tracing must actually stop because things
 * like calling smp_processor_id() may crash the system.
 *
 * Most likely, you want to use tracing_on/tracing_off.
 *
 * 通用 tracing 辅助函数，包括 trace_printk()、tracing_on/tracing_off
 * 以及 tracing_start()/tracing_stop。
 *
 * 如果只是希望快速打开或关闭 tracing，通常应使用 tracing_on/tracing_off。
 * 它们只控制 trace event 的记录开关，也对应用户态的
 * /sys/kernel/tracing/tracing_on 文件，便于内核和用户态交互。可以在希望
 * 停止记录的位置调用 tracing_off()，再从用户态查看 trace，并通过
 * echo 1 > tracing_on 继续记录。
 *
 * tracing_stop/tracing_start 的开销稍高，适用于 suspend to ram 等场景。
 * 在这些路径中，仅停止记录 trace 还不够，必须真正停止 tracing，否则
 * 调用 smp_processor_id() 等操作可能导致系统崩溃。
 *
 * 大多数情况下，应优先使用 tracing_on/tracing_off。
 */

enum ftrace_dump_mode
{
    DUMP_NONE,
    DUMP_ALL,
    DUMP_ORIG,
    DUMP_PARAM,
};

#ifdef CONFIG_TRACING
void        tracing_on (void);
void        tracing_off (void);
int         tracing_is_on (void);
void        tracing_snapshot (void);
void        tracing_snapshot_alloc (void);

extern void tracing_start (void);
extern void tracing_stop (void);

static inline __printf (1, 2) void ____trace_printk_check_format (const char* fmt, ...)
{
}
#    define __trace_printk_check_format(fmt, args...)        \
        do {                                                 \
            if (0)                                           \
                ____trace_printk_check_format (fmt, ##args); \
        } while (0)

/**
 * trace_printk - printf formatting in the ftrace buffer
 * @fmt: the printf format for printing
 *
 * Note: __trace_printk is an internal function for trace_printk() and
 *       the @ip is passed in via the trace_printk() macro.
 *
 * This function allows a kernel developer to debug fast path sections
 * that printk is not appropriate for. By scattering in various
 * printk like tracing in the code, a developer can quickly see
 * where problems are occurring.
 *
 * This is intended as a debugging tool for the developer only.
 * Please refrain from leaving trace_printks scattered around in
 * your code. (Extra memory is used for special buffers that are
 * allocated when trace_printk() is used.)
 *
 * A little optimization trick is done here. If there's only one
 * argument, there's no need to scan the string for printf formats.
 * The trace_puts() will suffice. But how can we take advantage of
 * using trace_puts() when trace_printk() has only one argument?
 * By stringifying the args and checking the size we can tell
 * whether or not there are args. __stringify((__VA_ARGS__)) will
 * turn into "()\0" with a size of 3 when there are no args, anything
 * else will be bigger. All we need to do is define a string to this,
 * and then take its size and compare to 3. If it's bigger, use
 * do_trace_printk() otherwise, optimize it to trace_puts(). Then just
 * let gcc optimize the rest.
 *
 * trace_printk - 按 printf 格式向 ftrace 缓冲区写入内容
 * @fmt: 用于打印的 printf 格式字符串
 *
 * 注意: __trace_printk 是 trace_printk() 的内部实现函数，@ip 参数由
 * trace_printk() 宏传入。
 *
 * 该函数允许内核开发者调试不适合使用 printk 的快速路径。通过在代码中
 * 插入类似 printk 的 trace 记录，可以快速观察问题发生的位置。
 *
 * 该接口只应作为开发者调试工具使用。请避免把 trace_printk 长期散落在
 * 代码中，因为使用它时会为专用缓冲区额外分配内存。
 *
 * 这里使用了一个小优化: 如果只有一个参数，就不需要扫描 printf 格式，
 * 直接使用 trace_puts() 即可。通过把可变参数字符串化并检查大小，可以
 * 判断是否存在额外参数。__stringify((__VA_ARGS__)) 在没有参数时会变成
 * "()\0"，大小为 3；其它情况都会更大。因此大于 3 时使用
 * do_trace_printk()，否则优化为 trace_puts()，剩余优化交给 gcc 完成。
 */

#    define trace_printk(fmt, ...)                           \
        do {                                                 \
            char _______STR[] = __stringify ((__VA_ARGS__)); \
            if (sizeof (_______STR) > 3)                     \
                do_trace_printk (fmt, ##__VA_ARGS__);        \
            else                                             \
                trace_puts (fmt);                            \
        } while (0)

#    define do_trace_printk(fmt, args...)                                                 \
        do {                                                                              \
            static const char* trace_printk_fmt __used __section ("__trace_printk_fmt") = \
                    __builtin_constant_p (fmt) ? fmt : NULL;                              \
                                                                                          \
            __trace_printk_check_format (fmt, ##args);                                    \
                                                                                          \
            if (__builtin_constant_p (fmt))                                               \
                __trace_bprintk (_THIS_IP_, trace_printk_fmt, ##args);                    \
            else                                                                          \
                __trace_printk (_THIS_IP_, fmt, ##args);                                  \
        } while (0)

extern __printf (2, 3) int __trace_bprintk (unsigned long ip, const char* fmt, ...);

extern __printf (2, 3) int __trace_printk (unsigned long ip, const char* fmt, ...);

/**
 * trace_puts - write a string into the ftrace buffer
 * @str: the string to record
 *
 * Note: __trace_bputs is an internal function for trace_puts and
 *       the @ip is passed in via the trace_puts macro.
 *
 * This is similar to trace_printk() but is made for those really fast
 * paths that a developer wants the least amount of "Heisenbug" effects,
 * where the processing of the print format is still too much.
 *
 * This function allows a kernel developer to debug fast path sections
 * that printk is not appropriate for. By scattering in various
 * printk like tracing in the code, a developer can quickly see
 * where problems are occurring.
 *
 * This is intended as a debugging tool for the developer only.
 * Please refrain from leaving trace_puts scattered around in
 * your code. (Extra memory is used for special buffers that are
 * allocated when trace_puts() is used.)
 *
 * Returns: 0 if nothing was written, positive # if string was.
 *  (1 when __trace_bputs is used, strlen(str) when __trace_puts is used)
 *
 * trace_puts - 向 ftrace 缓冲区写入字符串
 * @str: 要记录的字符串
 *
 * 注意: __trace_bputs 是 trace_puts 的内部实现函数，@ip 参数由
 * trace_puts 宏传入。
 *
 * 它类似 trace_printk()，但面向更极致的快速路径调试场景，用于尽量降低
 * “调试代码改变问题表现”的影响，因为此时连格式化字符串处理的开销都嫌大。
 *
 * 该函数允许内核开发者调试不适合使用 printk 的快速路径。通过在代码中
 * 插入类似 printk 的 trace 记录，可以快速观察问题发生的位置。
 *
 * 该接口只应作为开发者调试工具使用。请避免把 trace_puts 长期散落在
 * 代码中，因为使用它时会为专用缓冲区额外分配内存。
 *
 * 返回值: 未写入内容时返回 0；写入字符串时返回正数。
 * 使用 __trace_bputs 时返回 1，使用 __trace_puts 时返回 strlen(str)。
 */

#    define trace_puts(str)                                                               \
        ({                                                                                \
            static const char* trace_printk_fmt __used __section ("__trace_printk_fmt") = \
                    __builtin_constant_p (str) ? str : NULL;                              \
                                                                                          \
            if (__builtin_constant_p (str))                                               \
                __trace_bputs (_THIS_IP_, trace_printk_fmt);                              \
            else                                                                          \
                __trace_puts (_THIS_IP_, str, strlen (str));                              \
        })
extern int  __trace_bputs (unsigned long ip, const char* str);
extern int  __trace_puts (unsigned long ip, const char* str, int size);

extern void trace_dump_stack (int skip);

/*
 * The double __builtin_constant_p is because gcc will give us an error
 * if we try to allocate the static variable to fmt if it is not a
 * constant. Even with the outer if statement.
 *
 * 这里使用两层 __builtin_constant_p，是因为即使外层已经有 if 判断，
 * 如果 fmt 不是常量，gcc 仍会在为静态变量赋值时直接报错。
 */
#    define ftrace_vprintk(fmt, vargs)                                                        \
        do {                                                                                  \
            if (__builtin_constant_p (fmt)) {                                                 \
                static const char* trace_printk_fmt __used __section ("__trace_printk_fmt") = \
                        __builtin_constant_p (fmt) ? fmt : NULL;                              \
                                                                                              \
                __ftrace_vbprintk (_THIS_IP_, trace_printk_fmt, vargs);                       \
            } else                                                                            \
                __ftrace_vprintk (_THIS_IP_, fmt, vargs);                                     \
        } while (0)

extern __printf (2, 0) int __ftrace_vbprintk (unsigned long ip, const char* fmt, va_list ap);

extern __printf (2, 0) int __ftrace_vprintk (unsigned long ip, const char* fmt, va_list ap);

extern void ftrace_dump (enum ftrace_dump_mode oops_dump_mode);
#else
static inline void tracing_start (void)
{
}
static inline void tracing_stop (void)
{
}
static inline void trace_dump_stack (int skip)
{
}

static inline void tracing_on (void)
{
}
static inline void tracing_off (void)
{
}
static inline int tracing_is_on (void)
{
    return 0;
}
static inline void tracing_snapshot (void)
{
}
static inline void tracing_snapshot_alloc (void)
{
}

static inline __printf (1, 2) int trace_printk (const char* fmt, ...)
{
    return 0;
}
static __printf (1, 0) inline int ftrace_vprintk (const char* fmt, va_list ap)
{
    return 0;
}
static inline void ftrace_dump (enum ftrace_dump_mode oops_dump_mode){}
#endif /* CONFIG_TRACING */
/* CONFIG_TRACING 相关 tracing 接口定义结束。 */

/* Rebuild everything on CONFIG_FTRACE_MCOUNT_RECORD */
/* CONFIG_FTRACE_MCOUNT_RECORD 变化时需要重建所有相关目标。 */
#ifdef CONFIG_FTRACE_MCOUNT_RECORD
#    define REBUILD_DUE_TO_FTRACE_MCOUNT_RECORD
#endif

/* Permissions on a sysfs file: you didn't miss the 0 prefix did you? */
/* sysfs 文件权限必须使用八进制写法；确认没有漏掉前导 0。 */
#define VERIFY_OCTAL_PERMISSIONS(perms)                                                           \
    (BUILD_BUG_ON_ZERO ((perms) < 0) +                                                            \
     BUILD_BUG_ON_ZERO ((perms) > 0777) + /* USER_READABLE >= GROUP_READABLE >= OTHER_READABLE */ \
                                           /* 用户可读权限应不低于组和其它用户。 */              \
     BUILD_BUG_ON_ZERO ((((perms) >> 6) & 4) < (((perms) >> 3) & 4)) +                            \
     BUILD_BUG_ON_ZERO ((((perms) >> 3) & 4) <                                                    \
                        ((perms) & 4)) +  /* USER_WRITABLE >= GROUP_WRITABLE */                   \
                                           /* 用户可写权限应不低于组。 */                         \
     BUILD_BUG_ON_ZERO (                                                                          \
             (((perms) >> 6) & 2) <                                                               \
             (((perms) >> 3) & 2)) +      /* OTHER_WRITABLE?  Generally considered a bad idea. */ \
                                           /* 其它用户可写通常被认为是不安全的。 */               \
     BUILD_BUG_ON_ZERO ((perms) & 2) +                                                            \
     (perms))
#endif
