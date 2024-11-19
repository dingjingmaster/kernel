/*
 * 'sched.c' is the main kernel file. It contains scheduling primitives
 * (sleep_on, wakeup, schedule etc) as well as a number of simple system
 * call functions (type getpid(), which just extracts a field from
 * current-task
 */
#include <linux/sched.h>
#include <linux/kernel.h>
#include <signal.h>
#include <linux/sys.h>
#include <asm/system.h>
#include <asm/io.h>
#include <asm/segment.h>

#define LATCH (1193180/HZ)

extern void mem_use(void);

extern int timer_interrupt(void);
extern int system_call(void);

union task_union {
    struct task_struct task;
    char stack[PAGE_SIZE];
};

static union task_union init_task = {INIT_TASK,};

long volatile jiffies=0;
long startup_time=0;
struct task_struct *current = &(init_task.task), *last_task_used_math = NULL;

struct task_struct * task[NR_TASKS] = {&(init_task.task), };

long user_stack [ PAGE_SIZE>>2 ] ;

struct {
    long * a;
    short b;
    } stack_start = { & user_stack [PAGE_SIZE>>2] , 0x10 };
/*
 *  'math_state_restore()' saves the current math information in the
 * old math state array, and gets the new ones from the current task
 */
void math_state_restore()
{
    if (last_task_used_math)
        __asm__("fnsave %0"::"m" (last_task_used_math->tss.i387));
    if (current->used_math)
        __asm__("frstor %0"::"m" (current->tss.i387));
    else {
        __asm__("fninit"::);
        current->used_math=1;
    }
    last_task_used_math=current;
}

/*
 *  'schedule()' is the scheduler function. This is GOOD CODE! There
 * probably won't be any reason to change this, as it should work well
 * in all circumstances (ie gives IO-bound processes good response etc).
 * The one thing you might take a look at is the signal-handler code here.
 *
 *   NOTE!!  Task 0 is the 'idle' task, which gets called when no other
 * tasks can run. It can not be killed, and it cannot sleep. The 'state'
 * information in task[0] is never used.
 */
void schedule(void)
{
    int i,next,c;
    struct task_struct ** p;

/* check alarm, wake up any interruptible tasks that have got a signal */

    for(p = &LAST_TASK ; p > &FIRST_TASK ; --p) {
        if (*p) {
            // alarm 用于确定xx秒之后进程允许被唤醒
            if ((*p)->alarm && (*p)->alarm < jiffies) {
                (*p)->signal |= (1<<(SIGALRM-1));
                (*p)->alarm = 0;
            }
            if ((*p)->signal && (*p)->state==TASK_INTERRUPTIBLE) {
                (*p)->state=TASK_RUNNING;
            }
        }
    }

/* this is the scheduler proper: */

    while (1) {
        c = -1;                                 // 所有进程中时间片最大的
        next = 0;                               // 时间片最大的进程
        i = NR_TASKS;
        p = &task[NR_TASKS];
        while (--i) {
            if (!*--p) {
                continue;
            }
            if ((*p)->state == TASK_RUNNING && (*p)->counter > c) {
                c = (*p)->counter, next = i;
            }
        }
        if (c) break;                           // 找到时间片最大的进程则跳出此循环
        // 所有进程都没有时间片，重设时间片，时间片的时间和进程优先级有关系
        for (p = &LAST_TASK ; p > &FIRST_TASK ; --p) {
            if (*p) {
                (*p)->counter = ((*p)->counter >> 1) + (*p)->priority;
            }
        }
    }
    switch_to(next);                            // 切换到下一个进程
}

int sys_pause(void)
{
    current->state = TASK_INTERRUPTIBLE;
    schedule();
    return 0;
}

void sleep_on(struct task_struct **p)
{
    struct task_struct *tmp;

    if (!p)
        return;
    if (current == &(init_task.task))
        panic("task[0] trying to sleep");
    tmp = *p;
    *p = current;
    current->state = TASK_UNINTERRUPTIBLE;
    schedule();
    if (tmp)
        tmp->state=0;
}

void interruptible_sleep_on(struct task_struct **p)
{
    struct task_struct *tmp;

    if (!p)
        return;
    if (current == &(init_task.task))
        panic("task[0] trying to sleep");
    tmp=*p;
    *p=current;
repeat: current->state = TASK_INTERRUPTIBLE;
    schedule();
    if (*p && *p != current) {
        (**p).state=0;
        goto repeat;
    }
    *p=NULL;
    if (tmp)
        tmp->state=0;
}

void wake_up(struct task_struct **p)
{
    if (p && *p) {
        (**p).state=0;
        *p=NULL;
    }
}

// cpl 特权级别，linux中只能是 0 或 3
// 0 表示内核权限
// 3 表示用户权限
void do_timer(long cpl)
{
    if (cpl)
        current->utime++;
    else
        current->stime++;
    if ((--current->counter)>0) return;
    current->counter=0;
    if (!cpl) return;       // 内核权限则返回
    schedule();             // 用户权限则触发调用
}

int sys_alarm(long seconds)
{
    current->alarm = (seconds>0)?(jiffies+HZ*seconds):0;
    return seconds;
}

int sys_getpid(void)
{
    return current->pid;
}

int sys_getppid(void)
{
    return current->father;
}

int sys_getuid(void)
{
    return current->uid;
}

int sys_geteuid(void)
{
    return current->euid;
}

int sys_getgid(void)
{
    return current->gid;
}

int sys_getegid(void)
{
    return current->egid;
}

int sys_nice(long increment)
{
    if (current->priority-increment>0)
        current->priority -= increment;
    return 0;
}

int sys_signal(long signal,long addr,long restorer)
{
    long i;

    switch (signal) {
        case SIGHUP: case SIGINT: case SIGQUIT: case SIGILL:
        case SIGTRAP: case SIGABRT: case SIGFPE: case SIGUSR1:
        case SIGSEGV: case SIGUSR2: case SIGPIPE: case SIGALRM:
        case SIGCHLD:
            i=(long) current->sig_fn[signal-1];
            current->sig_fn[signal-1] = (fn_ptr) addr;
            current->sig_restorer = (fn_ptr) restorer;
            return i;
        default: return -1;
    }
}

void sched_init(void)
{
    int i;
    struct desc_struct * p;

    set_tss_desc(gdt+FIRST_TSS_ENTRY,&(init_task.task.tss));
    set_ldt_desc(gdt+FIRST_LDT_ENTRY,&(init_task.task.ldt));
    p = gdt+2+FIRST_TSS_ENTRY;
    for(i=1;i<NR_TASKS;i++) {
        task[i] = NULL;
        p->a=p->b=0;
        p++;
        p->a=p->b=0;
        p++;
    }
    ltr(0);                                 // 将任务寄存器(Task Registr, TR)加载为指定的任务状态段(TSS)。
                                            // 任务寄存器用于管理任务切换时的上下文切换数据，在保护模式下很重要
    lldt(0);                                // lldt(Load Local Descriptor Table Register) 是x86架构中的一个指令，
                                            // 用于加载任务的本地描述符表(LDT, Local Descriptor Table)的选择子到
                                            // LDTR寄存器
    outb_p(0x36,0x43);                      // 0x43: 是8253/8254可编程定时器芯片(PIT)的命令寄存器
                                            // 通过写入特定的命令字0x43，可以配置定时器的运行模式
                                            //  0x26配置说明：
                                            //    - D7-D6: 00  表示选择计数器0
                                            //    - D5-D4: 11  表示低字节和高字节分别写入
                                            //    - D3-D1: 110 表示模式3(方波生成)
                                            //    - D0:    0   表示计数器值以二进制格式写入
                                            // binary, mode 3, LSB/MSB, ch 0
    outb_p(LATCH & 0xff , 0x40);            /* LSB */
                                            // #define LATCH (1193180/HZ)
                                            // 用于向 8253/8254可编程定时器芯片(PIT)计数器0数据端口(0x40)写入低8位数据，作为计数值的一部分
                                            // PIT是用于生成定时中断的硬件设备，在早期x86系统中非常常见。它的三个计数器可以独立工作，
                                            // 但是计数器0 通常用于产生系统时钟(如：18.2Hz的时钟中断)
                                            // 每个计数器需要加载一个初始值作为计数周期，这个值需要分两次写入(先写低字节、再写高字节)
                                            // PIT 的输入频率通常为 1193180Hz(约1.19MHz)
    outb(LATCH >> 8 , 0x40);                /* MSB */
    set_intr_gate(0x20,&timer_interrupt);
    outb(inb_p(0x21)&~0x01,0x21);           // 对 8259可编程中断控制器 的中断屏蔽寄存器进行修改，具体功能是启用 IRQ0 中断
                                            // 从端口 0x21 读取中断屏蔽寄存器(IMR)的当前值
                                            // IMR 是一个8位寄存器，每一位对应一个IRQ(中断请求)的屏蔽状态：
                                            //  位值为1: 屏蔽对应的IRQ, 不会触发中断
                                            //  位值位0: 启用对应的IRQ，可以触发中断
                                            // IEQ0 对应IMR的低0位
                                            // 这两条指令就是让系统开始接受定时器中断
    set_system_gate(0x80,&system_call);     // 用于设置 系统调用 中断门，从而将中断向量 0x80 映射到系统调用处理函数 system_call
}
