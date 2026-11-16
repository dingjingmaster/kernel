// SPDX-License-Identifier: GPL-2.0
#include <linux/init_task.h>
#include <linux/export.h>
#include <linux/mqueue.h>
#include <linux/sched.h>
#include <linux/sched/sysctl.h>
#include <linux/sched/rt.h>
#include <linux/sched/task.h>
#include <linux/sched/ext.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/audit.h>
#include <linux/numa.h>
#include <linux/scs.h>
#include <linux/plist.h>

#include <linux/uaccess.h>

/*
 * init_signals 是 init_task 使用的初始 signal_struct。
 * 这里建立线程组、等待队列、资源限制、进程 ID 关系以及 CPU 计时器等
 * 最基础的信号/进程组状态，后续第一个任务会直接引用它。
 */
static struct signal_struct init_signals = {
    .nr_threads       = 1,
                                            .thread_head      = LIST_HEAD_INIT (init_task.thread_node),
                                            .wait_chldexit    = __WAIT_QUEUE_HEAD_INITIALIZER (init_signals.wait_chldexit),
                                            .shared_pending   = {.list = LIST_HEAD_INIT (init_signals.shared_pending.list), .signal = {{0}}},
                                            .multiprocess     = HLIST_HEAD_INIT,
                                            .rlim             = INIT_RLIMITS,
                                            .cred_guard_mutex = __MUTEX_INITIALIZER (init_signals.cred_guard_mutex),
                                            .exec_update_lock = __RWSEM_INITIALIZER (init_signals.exec_update_lock),
#ifdef CONFIG_POSIX_TIMERS
                                            .posix_timers = HLIST_HEAD_INIT,
                                            .cputimer =
                                                    {
                                                            .cputime_atomic = INIT_CPUTIME_ATOMIC,
                                                    },
#endif
                                            INIT_CPU_TIMERS (init_signals).pids =
                                                    {
                                                            [PIDTYPE_PID]  = &init_struct_pid,
                                                            [PIDTYPE_TGID] = &init_struct_pid,
                                                            [PIDTYPE_PGID] = &init_struct_pid,
                                                            [PIDTYPE_SID]  = &init_struct_pid,
                                                    },
                                            INIT_PREV_CPUTIME (init_signals)};

/*
 * init_sighand 是 init_task 使用的初始 sighand_struct。
 * 默认所有信号处理动作为 SIG_DFL，并初始化信号锁和 signalfd 等待队列。
 */
static struct sighand_struct init_sighand = {
        .count = REFCOUNT_INIT (1),
        .action =
                {
                        {{
                                .sa_handler = SIG_DFL,
                        }},
                },
        .siglock      = __SPIN_LOCK_UNLOCKED (init_sighand.siglock),
        .signalfd_wqh = __WAIT_QUEUE_HEAD_INITIALIZER (init_sighand.signalfd_wqh),
};

#ifdef CONFIG_SHADOW_CALL_STACK
/*
 * 启用 Shadow Call Stack 时，为 init_task 准备初始影子调用栈。
 * 最后一个槽位写入 SCS_END_MAGIC，用于标记影子栈边界。
 */
unsigned long init_shadow_call_stack[SCS_SIZE / sizeof (long)] = {[(SCS_SIZE / sizeof (long)) - 1] = SCS_END_MAGIC};
#endif

/**
 * @brief
 * Set up the first task table, touch at your own risk!. Base=0,
 * limit=0x1fffff (=2MB)
 * 64 字节对齐
 *
 * 设置第一个任务表。这里是启动阶段最基础的 task_struct 静态定义，
 * 改动风险很高；历史说明中的 Base=0、limit=0x1fffff 表示早期任务表
 * 覆盖的基础地址和 2MB 限制。
 *
 * init_task: 第一个进程，也是启动阶段所有任务关系和默认上下文的根。
 * 它最初作为内核线程运行，后续会派生 kthreadd 和 PID 1 的 init。
 */
struct task_struct init_task __aligned (L1_CACHE_BYTES) = {
#ifdef CONFIG_THREAD_INFO_IN_TASK
        .thread_info    = INIT_THREAD_INFO (init_task),
        .stack_refcount = REFCOUNT_INIT (1),
#endif
        .__state                 = 0,
        .stack                   = init_stack,
        .usage                   = REFCOUNT_INIT (2),
        .flags                   = PF_KTHREAD,  // 指示是内核线程
        .prio                    = MAX_PRIO - 20,   // 120
        .static_prio             = MAX_PRIO - 20,   // 120
        .normal_prio             = MAX_PRIO - 20,   // 120
        .policy                  = SCHED_NORMAL,    // 调度策略
        .cpus_ptr                = &init_task.cpus_mask,
        .user_cpus_ptr           = NULL,
        .cpus_mask               = CPU_MASK_ALL,
        .max_allowed_capacity    = SCHED_CAPACITY_SCALE,
        .nr_cpus_allowed         = NR_CPUS,         // 默认 1 cpu, 最大64
        .mm                      = NULL,            // 内核线程没有用户地址空间
        .active_mm               = &init_mm,        // 借用初始内存描述符作为活动 mm
        .faults_disabled_mapping = NULL,
        .restart_block =
                {
                        // 默认没有需要重启的系统调用
                        .fn = do_no_restart_syscall,
                },
        .se =
                {
                        // CFS 调度实体链表节点初始化
                        .group_node = LIST_HEAD_INIT (init_task.se.group_node),
                },
        .rt =
                {
                        // RT 调度实体的运行队列节点和时间片初始化
                        .run_list   = LIST_HEAD_INIT (init_task.rt.run_list),
                        .time_slice = RR_TIMESLICE,
                },
        .tasks = LIST_HEAD_INIT (init_task.tasks),  // 全局任务链表节点
#ifdef CONFIG_SMP
        // SMP 下用于 RT 推送迁移的优先级链表节点
        .pushable_tasks = PLIST_NODE_INIT (init_task.pushable_tasks, MAX_PRIO),
#endif
#ifdef CONFIG_CGROUP_SCHED
        // 初始任务属于根 task_group
        .sched_task_group = &root_task_group,
#endif
#ifdef CONFIG_SCHED_CLASS_EXT
        .scx =
                {
                        .dsq_list.node = LIST_HEAD_INIT (init_task.scx.dsq_list.node),
                        .sticky_cpu    = -1,
                        .holding_cpu   = -1,
                        .runnable_node = LIST_HEAD_INIT (init_task.scx.runnable_node),
                        .runnable_at   = INITIAL_JIFFIES,
                        .ddsp_dsq_id   = SCX_DSQ_INVALID,
                        .slice         = SCX_SLICE_DFL,
                },
#endif
        .ptraced      = LIST_HEAD_INIT (init_task.ptraced),       // 被 ptrace 跟踪的子任务链表
        .ptrace_entry = LIST_HEAD_INIT (init_task.ptrace_entry),  // 挂入父任务 ptrace 链表的节点
        .real_parent  = &init_task,                               // 初始任务的真实父任务指向自身
        .parent       = &init_task,                               // 调试视角父任务同样指向自身
        .children     = LIST_HEAD_INIT (init_task.children),      // 子任务链表
        .sibling      = LIST_HEAD_INIT (init_task.sibling),       // 兄弟任务链表节点
        .group_leader = &init_task,                               // 线程组 leader 为自身
        RCU_POINTER_INITIALIZER (real_cred, &init_cred),
        RCU_POINTER_INITIALIZER (cred, &init_cred),
        .comm   = INIT_TASK_COMM,                                 // 初始任务名，通常为 "swapper"
        .thread = INIT_THREAD,                                    // 架构相关线程上下文初值
        .fs     = &init_fs,                                       // 初始文件系统上下文
        .files  = &init_files,                                    // 初始文件描述符表
#ifdef CONFIG_IO_URING
        .io_uring = NULL,
#endif
        .signal                             = &init_signals,       // 共享信号状态
        .sighand                            = &init_sighand,       // 信号处理函数表
        .nsproxy                            = &init_nsproxy,       // 初始命名空间集合
        .pending                            = {.list = LIST_HEAD_INIT (init_task.pending.list), .signal = {{0}}},
        .blocked                            = {{0}},
        .alloc_lock                         = __SPIN_LOCK_UNLOCKED (init_task.alloc_lock),
        .journal_info                       = NULL,
        INIT_CPU_TIMERS (init_task).pi_lock = __RAW_SPIN_LOCK_UNLOCKED (init_task.pi_lock),
        .timer_slack_ns                     = 50000, /* 50 usec default slack */
        /* 定时器默认 slack 为 50 微秒，用于允许内核合并相近定时器以节能。 */
        .thread_pid                         = &init_struct_pid,
        .thread_node                        = LIST_HEAD_INIT (init_signals.thread_head),
#ifdef CONFIG_AUDIT
        // 初始任务还没有有效登录 UID 和审计会话 ID
        .loginuid  = INVALID_UID,
        .sessionid = AUDIT_SID_UNSET,
#endif
#ifdef CONFIG_PERF_EVENTS
        // perf 事件列表和互斥锁初始化
        .perf_event_mutex = __MUTEX_INITIALIZER (init_task.perf_event_mutex),
        .perf_event_list  = LIST_HEAD_INIT (init_task.perf_event_list),
#endif
#ifdef CONFIG_PREEMPT_RCU
        // 可抢占 RCU 的读侧嵌套、特殊状态和阻塞节点初始值
        .rcu_read_lock_nesting     = 0,
        .rcu_read_unlock_special.s = 0,
        .rcu_node_entry            = LIST_HEAD_INIT (init_task.rcu_node_entry),
        .rcu_blocked_node          = NULL,
#endif
#ifdef CONFIG_TASKS_RCU
        // Tasks RCU 用于跟踪任务级宽限期的初始状态
        .rcu_tasks_holdout      = false,
        .rcu_tasks_holdout_list = LIST_HEAD_INIT (init_task.rcu_tasks_holdout_list),
        .rcu_tasks_idle_cpu     = -1,
        .rcu_tasks_exit_list    = LIST_HEAD_INIT (init_task.rcu_tasks_exit_list),
#endif
#ifdef CONFIG_TASKS_TRACE_RCU
        // Tasks Trace RCU 读侧嵌套和阻塞链表初始化
        .trc_reader_nesting   = 0,
        .trc_reader_special.s = 0,
        .trc_holdout_list     = LIST_HEAD_INIT (init_task.trc_holdout_list),
        .trc_blkd_node        = LIST_HEAD_INIT (init_task.trc_blkd_node),
#endif
#ifdef CONFIG_CPUSETS
        // cpuset 内存节点允许集合的 seqcount
        .mems_allowed_seq = SEQCNT_SPINLOCK_ZERO (init_task.mems_allowed_seq, &init_task.alloc_lock),
#endif
#ifdef CONFIG_RT_MUTEXES
        // PI futex/rtmutex 使用的优先级继承等待者结构
        .pi_waiters  = RB_ROOT_CACHED,
        .pi_top_task = NULL,
#endif
        INIT_PREV_CPUTIME (init_task)
#ifdef CONFIG_VIRT_CPU_ACCOUNTING_GEN
                // 虚拟 CPU 记账从系统态开始
                .vtime.seqcount = SEQCNT_ZERO (init_task.vtime_seqcount),
        .vtime.starttime        = 0,
        .vtime.state            = VTIME_SYS,
#endif
#ifdef CONFIG_NUMA_BALANCING
        // NUMA 自动均衡初始没有首选节点和 fault 统计
        .numa_preferred_nid = NUMA_NO_NODE,
        .numa_group         = NULL,
        .numa_faults        = NULL,
#endif
#if defined(CONFIG_KASAN_GENERIC) || defined(CONFIG_KASAN_SW_TAGS)
        // 启动早期避免 KASAN 在自身初始化前递归进入检查逻辑
        .kasan_depth = 1,
#endif
#ifdef CONFIG_KCSAN
        // KCSAN 作用域访问链表初始为空/毒化状态
        .kcsan_ctx =
                {
                        .scoped_accesses = {LIST_POISON1, NULL},
                },
#endif
#ifdef CONFIG_TRACE_IRQFLAGS
        // 初始认为软中断处于启用状态
        .softirqs_enabled = 1,
#endif
#ifdef CONFIG_LOCKDEP
        .lockdep_depth     = 0, /* no locks held yet */
        /* 初始任务尚未持有任何锁。 */
        .curr_chain_key    = INITIAL_CHAIN_KEY,
        .lockdep_recursion = 0,
#endif
#ifdef CONFIG_FUNCTION_GRAPH_TRACER
        // 函数图跟踪器的返回栈初始为空
        .ret_stack           = NULL,
        .tracing_graph_pause = ATOMIC_INIT (0),
#endif
#if defined(CONFIG_TRACING) && defined(CONFIG_PREEMPTION)
        // tracing 递归计数初始为 0
        .trace_recursion = 0,
#endif
#ifdef CONFIG_LIVEPATCH
        // livepatch 初始处于无迁移事务状态
        .patch_state = KLP_TRANSITION_IDLE,
#endif
#ifdef CONFIG_SECURITY
        // LSM 安全 blob 后续由安全子系统分配/初始化
        .security = NULL,
#endif
#ifdef CONFIG_SECCOMP_FILTER
        // seccomp 过滤器引用计数初始为 0
        .seccomp = {.filter_count = ATOMIC_INIT (0)},
#endif
};
EXPORT_SYMBOL (init_task);

/*
 * Initial thread structure. Alignment of this is handled by a special
 * linker map entry.
 *
 * 初始 thread_info 结构。其对齐方式由专用链接脚本条目保证。
 */
#ifndef CONFIG_THREAD_INFO_IN_TASK
struct thread_info init_thread_info __init_thread_info = INIT_THREAD_INFO (init_task);
#endif
