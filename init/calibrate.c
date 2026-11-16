// SPDX-License-Identifier: GPL-2.0
/* calibrate.c: default delay calibration
 *
 * Excised from init/main.c
 *  Copyright (C) 1991, 1992  Linus Torvalds
 *
 * 默认延时循环校准代码，用于计算 loops_per_jiffy，供 busy-wait 类延时函数使用。
 * 本文件从早期 init/main.c 中拆出。
 */

#include <linux/jiffies.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/timex.h>
#include <linux/smp.h>
#include <linux/percpu.h>

unsigned long     lpj_fine;
unsigned long     preset_lpj;
static int __init lpj_setup (char* str)
{
    /* lpj= 启动参数允许用户直接指定 loops_per_jiffy，跳过运行时校准。 */
    preset_lpj = simple_strtoul (str, NULL, 0);
    return 1;
}

__setup ("lpj=", lpj_setup);

#ifdef ARCH_HAS_READ_CURRENT_TIMER

/* This routine uses the read_current_timer() routine and gets the
 * loops per jiffy directly, instead of guessing it using delay().
 * Also, this code tries to handle non-maskable asynchronous events
 * (like SMIs)
 *
 * 该路径通过 read_current_timer() 直接读取当前计时器来估算每个 jiffy 的循环数，
 * 不再依赖 delay() 反复试探。同时它会尽量识别 SMI 等不可屏蔽异步事件带来的误差。
 */
#    define DELAY_CALIBRATION_TICKS ((HZ < 100) ? 1 : (HZ / 100))
#    define MAX_DIRECT_CALIBRATION_RETRIES 5

static unsigned long calibrate_delay_direct (void)
{
    unsigned long pre_start, start, post_start;
    unsigned long pre_end, end, post_end;
    unsigned long start_jiffies;
    unsigned long timer_rate_min, timer_rate_max;
    unsigned long good_timer_sum   = 0;
    unsigned long good_timer_count = 0;
    unsigned long measured_times[MAX_DIRECT_CALIBRATION_RETRIES];
    int           max = -1; /* index of measured_times with max/min values or not set */
    /* max/min 记录 measured_times 中最大/最小有效样本的下标，-1 表示尚未设置。 */
    int           min = -1;
    int           i;

    if (read_current_timer (&pre_start) < 0)
        return 0;

    /*
     * A simple loop like
     *    while ( jiffies < start_jiffies+1)
     *        start = read_current_timer();
     * will not do. As we don't really know whether jiffy switch
     * happened first or timer_value was read first. And some asynchronous
     * event can happen between these two events introducing errors in lpj.
     *
     * So, we do
     * 1. pre_start <- When we are sure that jiffy switch hasn't happened
     * 2. check jiffy switch
     * 3. start <- timer value before or after jiffy switch
     * 4. post_start <- When we are sure that jiffy switch has happened
     *
     * Note, we don't know anything about order of 2 and 3.
     * Now, by looking at post_start and pre_start difference, we can
     * check whether any asynchronous event happened or not
     *
     * 不能只用简单循环等待 jiffies 翻转，因为无法确认是 jiffy 先切换，
     * 还是 timer_value 先被读取；两者之间还可能插入异步事件，污染 lpj。
     *
     * 因此这里在 jiffy 翻转前后各记录一次确定边界:
     * 1. pre_start: 确信 jiffy 尚未翻转时的计时器值
     * 2. 检查 jiffy 是否翻转
     * 3. start: 翻转前后附近读取到的计时器值
     * 4. post_start: 确信 jiffy 已经翻转后的计时器值
     *
     * 通过 pre_start 和 post_start 的差距，可以判断其间是否发生了较长异步事件。
     */

    for (i = 0; i < MAX_DIRECT_CALIBRATION_RETRIES; i++) {
        /* 采样一个 jiffy 起点附近的计时器边界。 */
        pre_start = 0;
        read_current_timer (&start);
        start_jiffies = jiffies;
        while (time_before_eq (jiffies, start_jiffies + 1)) {
            pre_start = start;
            read_current_timer (&start);
        }
        read_current_timer (&post_start);

        /* 再采样 DELAY_CALIBRATION_TICKS 个 jiffy 后的计时器边界。 */
        pre_end = 0;
        end     = post_start;
        while (time_before_eq (jiffies, start_jiffies + 1 + DELAY_CALIBRATION_TICKS)) {
            pre_end = end;
            read_current_timer (&end);
        }
        read_current_timer (&post_end);

        timer_rate_max = (post_end - pre_start) / DELAY_CALIBRATION_TICKS;
        timer_rate_min = (pre_end - post_start) / DELAY_CALIBRATION_TICKS;

        /*
         * If the upper limit and lower limit of the timer_rate is
         * >= 12.5% apart, redo calibration.
         *
         * 如果计时器频率上下界相差达到 12.5% 或更多，说明样本不稳定，需要丢弃。
         */
        if (start >= post_end)
            printk (KERN_NOTICE "calibrate_delay_direct() ignoring "
                                "timer_rate as we had a TSC wrap around"
                                " start=%lu >=post_end=%lu\n",
                    start, post_end);
        if (start < post_end && pre_start != 0 && pre_end != 0 &&
            (timer_rate_max - timer_rate_min) < (timer_rate_max >> 3)) {
            good_timer_count++;
            good_timer_sum += timer_rate_max;
            measured_times[i] = timer_rate_max;
            if (max < 0 || timer_rate_max > measured_times[max])
                max = i;
            if (min < 0 || timer_rate_max < measured_times[min])
                min = i;
        } else
            measured_times[i] = 0;
    }

    /*
     * Find the maximum & minimum - if they differ too much throw out the
     * one with the largest difference from the mean and try again...
     *
     * 找出最大和最小样本；如果离散度太大，就丢弃距离平均值最远的样本后重算。
     */
    while (good_timer_count > 1) {
        unsigned long estimate;
        unsigned long maxdiff;

        /* compute the estimate */
        /* 用当前有效样本求平均估算值。 */
        estimate = (good_timer_sum / good_timer_count);
        maxdiff  = estimate >> 3;

        /* if range is within 12% let's take it */
        /* 如果最大最小值范围在约 12% 内，则接受该估算值。 */
        if ((measured_times[max] - measured_times[min]) < maxdiff)
            return estimate;

        /* ok - drop the worse value and try again... */
        /* 否则丢弃偏离平均值更远的极值样本后继续收敛。 */
        good_timer_sum   = 0;
        good_timer_count = 0;
        if ((measured_times[max] - estimate) < (estimate - measured_times[min])) {
            printk (KERN_NOTICE "calibrate_delay_direct() dropping "
                                "min bogoMips estimate %d = %lu\n",
                    min, measured_times[min]);
            measured_times[min] = 0;
            min                 = max;
        } else {
            printk (KERN_NOTICE "calibrate_delay_direct() dropping "
                                "max bogoMips estimate %d = %lu\n",
                    max, measured_times[max]);
            measured_times[max] = 0;
            max                 = min;
        }

        for (i = 0; i < MAX_DIRECT_CALIBRATION_RETRIES; i++) {
            if (measured_times[i] == 0)
                continue;
            good_timer_count++;
            good_timer_sum += measured_times[i];
            if (measured_times[i] < measured_times[min])
                min = i;
            if (measured_times[i] > measured_times[max])
                max = i;
        }
    }

    printk (KERN_NOTICE "calibrate_delay_direct() failed to get a good "
                        "estimate for loops_per_jiffy.\nProbably due to long platform "
                        "interrupts. Consider using \"lpj=\" boot option.\n");
    return 0;
}
#else
static unsigned long calibrate_delay_direct (void)
{
    return 0;
}
#endif

/*
 * This is the number of bits of precision for the loops_per_jiffy.  Each
 * time we refine our estimate after the first takes 1.5/HZ seconds, so try
 * to start with a good estimate.
 * For the boot cpu we can skip the delay calibration and assign it a value
 * calculated based on the timer frequency.
 * For the rest of the CPUs we cannot assume that the timer frequency is same as
 * the cpu frequency, hence do the calibration for those.
 *
 * LPS_PREC 表示 loops_per_jiffy 收敛时保留的精度位数。首次估算后每次细化
 * 大约需要 1.5/HZ 秒，因此初值越好，启动越快。
 *
 * 启动 CPU 如果已能根据计时器频率计算出值，可以跳过校准；其它 CPU 不能假设
 * 计时器频率等于 CPU 频率，因此仍需各自校准。
 */
#define LPS_PREC 8

static unsigned long calibrate_delay_converge (void)
{
    /* First stage - slowly accelerate to find initial bounds */
    /* 第一阶段逐步增加延时循环，找到跨过一个 jiffy 的初始上下界。 */
    unsigned long lpj, lpj_base, ticks, loopadd, loopadd_base, chop_limit;
    int           trials = 0, band = 0, trial_in_band = 0;

    lpj   = (1 << 12);

    /* wait for "start of" clock tick */
    /* 等待进入一个新的时钟 tick 起点。 */
    ticks = jiffies;
    while (ticks == jiffies)
        ; /* nothing */
    /* 空循环等待 jiffies 变化。 */
    /* Go .. */
    /* 从较小 lpj 开始按 band 逐步放大，直到延时跨过当前 tick。 */
    ticks = jiffies;
    do {
        if (++trial_in_band == (1 << band)) {
            ++band;
            trial_in_band = 0;
        }
        __delay (lpj * band);
        trials += band;
    } while (ticks == jiffies);
    /*
     * We overshot, so retreat to a clear underestimate. Then estimate
     * the largest likely undershoot. This defines our chop bounds.
     *
     * 已经越过一个 tick，因此退回到明确低估的值，再估算可能的最大低估范围，
     * 作为后续二分逼近的边界。
     */
    trials -= band;
    loopadd_base = lpj * band;
    lpj_base     = lpj * trials;

recalibrate:
    lpj        = lpj_base;
    loopadd    = loopadd_base;

    /*
     * Do a binary approximation to get lpj set to
     * equal one clock (up to LPS_PREC bits)
     *
     * 用二分逼近方式把 lpj 调整到约等于一个时钟 tick 的循环数，
     * 精度达到 LPS_PREC 位。
     */
    chop_limit = lpj >> LPS_PREC;
    while (loopadd > chop_limit) {
        lpj += loopadd;
        ticks = jiffies;
        while (ticks == jiffies)
            ;                 /* nothing */
        /* 等待下一个 tick 起点后再测试当前 lpj。 */
        ticks = jiffies;
        __delay (lpj);
        if (jiffies != ticks) /* longer than 1 tick */
            /* 如果延时超过一个 tick，说明当前 lpj 偏大，撤销本次增量。 */
            lpj -= loopadd;
        loopadd >>= 1;
    }
    /*
     * If we incremented every single time possible, presume we've
     * massively underestimated initially, and retry with a higher
     * start, and larger range. (Only seen on x86_64, due to SMIs)
     *
     * 如果每次增量都被接受，说明初始值可能严重低估；提高起点并扩大搜索范围重试。
     * 这种情况主要见于 x86_64 上 SMI 干扰导致的估算偏差。
     */
    if (lpj + loopadd * 2 == lpj_base + loopadd_base * 2) {
        lpj_base = lpj;
        loopadd_base <<= 2;
        goto recalibrate;
    }

    return lpj;
}

static DEFINE_PER_CPU (unsigned long, cpu_loops_per_jiffy) = {0};

/*
 * Check if cpu calibration delay is already known. For example,
 * some processors with multi-core sockets may have all cores
 * with the same calibration delay.
 *
 * Architectures should override this function if a faster calibration
 * method is available.
 *
 * 检查当前 CPU 的延时校准值是否已经可知。例如某些多核 socket 中所有核心
 * 可能共享同样的校准值。若架构有更快的校准方式，可以覆盖该弱符号函数。
 */
unsigned long __attribute__ ((weak)) calibrate_delay_is_known (void)
{
    return 0;
}

/*
 * Indicate the cpu delay calibration is done. This can be used by
 * architectures to stop accepting delay timer registrations after this point.
 *
 * 通知架构代码 CPU 延时校准已经完成。架构可据此停止接受新的 delay timer 注册。
 */

void __attribute__ ((weak)) calibration_delay_done (void)
{
}

void calibrate_delay (void)
{
    unsigned long lpj;
    static bool   printed;
    int           this_cpu = smp_processor_id ();

    /* 优先复用当前 CPU 已保存的校准值。 */
    if (per_cpu (cpu_loops_per_jiffy, this_cpu)) {
        lpj = per_cpu (cpu_loops_per_jiffy, this_cpu);
        if (!printed)
            pr_info ("Calibrating delay loop (skipped) "
                     "already calibrated this CPU");
    } else if (preset_lpj) {
        /* lpj= 启动参数指定值优先级最高，直接采用。 */
        lpj = preset_lpj;
        if (!printed)
            pr_info ("Calibrating delay loop (skipped) "
                     "preset value.. ");
    } else if ((!printed) && lpj_fine) {
        /* 启动 CPU 可使用基于计时器频率预先计算出的精确值。 */
        lpj = lpj_fine;
        pr_info ("Calibrating delay loop (skipped), "
                 "value calculated using timer frequency.. ");
    } else if ((lpj = calibrate_delay_is_known ())) {
        /* 架构可提供已知校准值，避免通用慢速校准。 */
        ;
    } else if ((lpj = calibrate_delay_direct ()) != 0) {
        /* 可直接读当前计时器时，优先使用直接校准路径。 */
        if (!printed)
            pr_info ("Calibrating delay using timer "
                     "specific routine.. ");
    } else {
        /* 最后退回到基于 jiffies 和 __delay() 的收敛校准。 */
        if (!printed)
            pr_info ("Calibrating delay loop... ");
        lpj = calibrate_delay_converge ();
    }
    /* 保存当前 CPU 的校准值，并更新全局 loops_per_jiffy。 */
    per_cpu (cpu_loops_per_jiffy, this_cpu) = lpj;
    if (!printed)
        pr_cont ("%lu.%02lu BogoMIPS (lpj=%lu)\n", lpj / (500000 / HZ), (lpj / (5000 / HZ)) % 100,
                 lpj);

    loops_per_jiffy = lpj;
    printed         = true;

    calibration_delay_done ();
}
