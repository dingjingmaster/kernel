// SPDX-License-Identifier: GPL-2.0-only
/*
 *  linux/init/main.c
 *
 *  Copyright (C) 1991, 1992  Linus Torvalds
 *
 *  GK 2/5/95  -  Changed to support mounting root fs via NFS
 *  Added initrd & change_root: Werner Almesberger & Hans Lermen, Feb '96
 *  Moan early if gcc is old, avoiding bogus kernels - Paul Gortmaker, May '96
 *  Simplified starting of init:  Michael A. Griffith <grif@acm.org>
 */

#define DEBUG /* Enable initcall_debug */

#include <linux/types.h>
#include <linux/extable.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/binfmts.h>
#include <linux/kernel.h>
#include <linux/syscalls.h>
#include <linux/stackprotector.h>
#include <linux/string.h>
#include <linux/ctype.h>
#include <linux/delay.h>
#include <linux/ioport.h>
#include <linux/init.h>
#include <linux/initrd.h>
#include <linux/memblock.h>
#include <linux/acpi.h>
#include <linux/bootconfig.h>
#include <linux/console.h>
#include <linux/nmi.h>
#include <linux/percpu.h>
#include <linux/kmod.h>
#include <linux/kprobes.h>
#include <linux/kmsan.h>
#include <linux/vmalloc.h>
#include <linux/kernel_stat.h>
#include <linux/start_kernel.h>
#include <linux/security.h>
#include <linux/smp.h>
#include <linux/profile.h>
#include <linux/kfence.h>
#include <linux/rcupdate.h>
#include <linux/srcu.h>
#include <linux/moduleparam.h>
#include <linux/kallsyms.h>
#include <linux/buildid.h>
#include <linux/writeback.h>
#include <linux/cpu.h>
#include <linux/cpuset.h>
#include <linux/cgroup.h>
#include <linux/efi.h>
#include <linux/tick.h>
#include <linux/sched/isolation.h>
#include <linux/interrupt.h>
#include <linux/taskstats_kern.h>
#include <linux/delayacct.h>
#include <linux/unistd.h>
#include <linux/utsname.h>
#include <linux/rmap.h>
#include <linux/mempolicy.h>
#include <linux/key.h>
#include <linux/debug_locks.h>
#include <linux/debugobjects.h>
#include <linux/lockdep.h>
#include <linux/kmemleak.h>
#include <linux/padata.h>
#include <linux/pid_namespace.h>
#include <linux/device/driver.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/sched/init.h>
#include <linux/signal.h>
#include <linux/idr.h>
#include <linux/kgdb.h>
#include <linux/ftrace.h>
#include <linux/async.h>
#include <linux/shmem_fs.h>
#include <linux/slab.h>
#include <linux/perf_event.h>
#include <linux/ptrace.h>
#include <linux/pti.h>
#include <linux/blkdev.h>
#include <linux/sched/clock.h>
#include <linux/sched/task.h>
#include <linux/sched/task_stack.h>
#include <linux/context_tracking.h>
#include <linux/random.h>
#include <linux/moduleloader.h>
#include <linux/list.h>
#include <linux/integrity.h>
#include <linux/proc_ns.h>
#include <linux/io.h>
#include <linux/cache.h>
#include <linux/rodata_test.h>
#include <linux/jump_label.h>
#include <linux/kcsan.h>
#include <linux/init_syscalls.h>
#include <linux/stackdepot.h>
#include <linux/randomize_kstack.h>
#include <linux/pidfs.h>
#include <linux/ptdump.h>
#include <net/net_namespace.h>

#include <asm/io.h>
#include <asm/setup.h>
#include <asm/sections.h>
#include <asm/cacheflush.h>

#define CREATE_TRACE_POINTS
#include <trace/events/initcall.h>

#include <kunit/test.h>

static int                      kernel_init (void*);

/**
 * Debug helper: via this flag we know that we are in 'early bootup code'
 * where only the boot processor is running with IRQ disabled.  This means
 * two things - IRQ must not be enabled before the flag is cleared and some
 * operations which are not allowed with IRQ disabled are allowed while the
 * flag is set.
 *
 * 调试辅助标志: 该标志表示当前仍处在只运行启动 CPU 且中断关闭的早期启动阶段。
 * 在清除此标志前不能打开中断；同时，部分通常不允许在关中断状态下执行的操作，
 * 在该早期启动窗口内是允许的。
 */
bool early_boot_irqs_disabled   __read_mostly;

enum system_states system_state __read_mostly;
EXPORT_SYMBOL (system_state);

/*
 * Boot command-line arguments
 *
 * 启动命令行参数数量上限。
 */
#define MAX_INIT_ARGS CONFIG_INIT_ENV_ARG_LIMIT
#define MAX_INIT_ENVS CONFIG_INIT_ENV_ARG_LIMIT

/* Default late time init is NULL. archs can override this later. */
/* late_time_init 默认为 NULL，具体架构可在后续启动流程中覆盖它。 */
void (*__initdata late_time_init) (void);

/* Untouched command line saved by arch-specific code. */
/* 架构相关代码保存的原始启动命令行，保持未修改状态。 */
char __initdata                     boot_command_line[COMMAND_LINE_SIZE];
/* Untouched saved command line (eg. for /proc) */
/* 对外保留的原始命令行副本，例如供 /proc/cmdline 使用。 */
char* saved_command_line            __ro_after_init;
unsigned int saved_command_line_len __ro_after_init;
/* Command line for parameter parsing */
/* 参数解析使用的命令行副本，解析过程会原地修改该缓冲区。 */
static char*                        static_command_line;
/* Untouched extra command line */
/* 来自 bootconfig 等来源的额外内核命令行。 */
static char*                        extra_command_line;
/* Extra init arguments */
/* 额外传递给用户态 init 进程的参数。 */
static char*                        extra_init_args;

#ifdef CONFIG_BOOT_CONFIG
/* Is bootconfig on command line? */
/* 命令行中是否显式指定了 bootconfig。 */
static bool   bootconfig_found;
static size_t initargs_offs;
#else
#    define bootconfig_found false
#    define initargs_offs 0
#endif

static char*                execute_command;
static char*                ramdisk_execute_command = "/init";

/*
 * Used to generate warnings if static_key manipulation functions are used
 * before jump_label_init is called.
 *
 * 用于在 jump_label_init() 之前误用 static key 操作函数时发出告警。
 */
bool static_key_initialized __read_mostly;
EXPORT_SYMBOL_GPL (static_key_initialized);

/*
 * If set, this is an indication to the drivers that reset the underlying
 * device before going ahead with the initialization otherwise driver might
 * rely on the BIOS and skip the reset operation.
 *
 * This is useful if kernel is booting in an unreliable environment.
 * For ex. kdump situation where previous kernel has crashed, BIOS has been
 * skipped and devices will be in unknown state.
 *
 * 如果设置该标志，驱动在初始化设备前应先复位底层硬件，而不是依赖 BIOS
 * 遗留的状态。这对 kdump 等不可靠启动环境很重要，因为前一个内核崩溃后
 * 可能跳过 BIOS 初始化，设备状态不可预期。
 */
unsigned int reset_devices;
EXPORT_SYMBOL (reset_devices);

static int __init set_reset_devices (char* str)
{
    reset_devices = 1;
    return 1;
}

__setup ("reset_devices", set_reset_devices);

static const char* argv_init[MAX_INIT_ARGS + 2] = {
        "init",
        NULL,
};
const char* envp_init[MAX_INIT_ENVS + 2] = {
        "HOME=/",
        "TERM=linux",
        NULL,
};
static const char *panic_later, *panic_param;

static bool __init obsolete_checksetup (char* line)
{
    const struct obs_kernel_param* p;
    bool                           had_early_param = false;

    p                                              = __setup_start;
    do {
        int n = strlen (p->str);
        if (parameqn (line, p->str, n)) {
            if (p->early) {
                /* Already done in parse_early_param?
                 * (Needs exact match on param part).
                 * Keep iterating, as we can have early
                 * params and __setups of same names 8( */
                /* 是否已经由 parse_early_param() 处理过。
                 * 这里要求参数名精确匹配；仍需继续遍历，因为 early_param
                 * 和 __setup 可能注册了相同名称的参数。
                 */
                if (line[n] == '\0' || line[n] == '=')
                    had_early_param = true;
            } else if (!p->setup_func) {
                pr_warn ("Parameter %s is obsolete, ignored\n", p->str);
                return true;
            } else if (p->setup_func (line + n))
                return true;
        }
        p++;
    } while (p < __setup_end);

    return had_early_param;
}

/*
 * This should be approx 2 Bo*oMips to start (note initial shift), and will
 * still work even if initially too large, it will just take slightly longer
 *
 * 初始值大致对应 2 倍 BogoMips；即使初值偏大也能工作，只是校准耗时稍长。
 */
unsigned long loops_per_jiffy = (1 << 12);
EXPORT_SYMBOL (loops_per_jiffy);

static int __init debug_kernel (char* str)
{
    console_loglevel = CONSOLE_LOGLEVEL_DEBUG;
    return 0;
}

static int __init quiet_kernel (char* str)
{
    console_loglevel = CONSOLE_LOGLEVEL_QUIET;
    return 0;
}

early_param ("debug", debug_kernel);
early_param ("quiet", quiet_kernel);

static int __init loglevel (char* str)
{
    int newlevel;

    /*
     * Only update loglevel value when a correct setting was passed,
     * to prevent blind crashes (when loglevel being set to 0) that
     * are quite hard to debug
     *
     * 只有解析到合法值时才更新日志级别，避免把 loglevel 误设为 0 后
     * 关键日志被屏蔽，导致启动崩溃难以定位。
     */
    if (get_option (&str, &newlevel)) {
        console_loglevel = newlevel;
        return 0;
    }

    return -EINVAL;
}

early_param ("loglevel", loglevel);

#ifdef CONFIG_BLK_DEV_INITRD
static void* __init get_boot_config_from_initrd (size_t* _size)
{
    u32   size, csum;
    char* data;
    u32*  hdr;
    int   i;

    if (!initrd_end)
        return NULL;

    data = (char*)initrd_end - BOOTCONFIG_MAGIC_LEN;
    /*
     * Since Grub may align the size of initrd to 4, we must
     * check the preceding 3 bytes as well.
     *
     * Grub 可能把 initrd 大小按 4 字节对齐，因此还要向前检查最多 3 个字节。
     */
    for (i = 0; i < 4; i++) {
        if (!memcmp (data, BOOTCONFIG_MAGIC, BOOTCONFIG_MAGIC_LEN))
            goto found;
        data--;
    }
    return NULL;

found:
    hdr  = (u32*)(data - 8);
    size = le32_to_cpu (hdr[0]);
    csum = le32_to_cpu (hdr[1]);

    data = ((void*)hdr) - size;
    if ((unsigned long)data < initrd_start) {
        pr_err ("bootconfig size %d is greater than initrd size %ld\n", size, initrd_end - initrd_start);
        return NULL;
    }

    if (xbc_calc_checksum (data, size) != csum) {
        pr_err ("bootconfig checksum failed\n");
        return NULL;
    }

    /* Remove bootconfig from initramfs/initrd */
    /* 从 initramfs/initrd 尾部剥离 bootconfig 数据。 */
    initrd_end = (unsigned long)data;
    if (_size)
        *_size = size;

    return data;
}
#else
static void* __init get_boot_config_from_initrd (size_t* _size)
{
    return NULL;
}
#endif

#ifdef CONFIG_BOOT_CONFIG

static char xbc_namebuf[XBC_KEYLEN_MAX] __initdata;

#    define rest(dst, end) ((end) > (dst) ? (end) - (dst) : 0)

static int __init xbc_snprint_cmdline (char* buf, size_t size, struct xbc_node* root)
{
    struct xbc_node *knode, *vnode;
    char*            end = buf + size;
    const char *     val, *q;
    int              ret;

    xbc_node_for_each_key_value (root, knode, val) {
        ret = xbc_node_compose_key_after (root, knode, xbc_namebuf, XBC_KEYLEN_MAX);
        if (ret < 0)
            return ret;

        vnode = xbc_node_get_child (knode);
        if (!vnode) {
            ret = snprintf (buf, rest (buf, end), "%s ", xbc_namebuf);
            if (ret < 0)
                return ret;
            buf += ret;
            continue;
        }
        xbc_array_for_each_value (vnode, val) {
            /*
             * For prettier and more readable /proc/cmdline, only
             * quote the value when necessary, i.e. when it contains
             * whitespace.
             *
             * 为了让 /proc/cmdline 更易读，仅当值中包含空白字符时才加引号。
             */
            q   = strpbrk (val, " \t\r\n") ? "\"" : "";
            ret = snprintf (buf, rest (buf, end), "%s=%s%s%s ", xbc_namebuf, q, val, q);
            if (ret < 0)
                return ret;
            buf += ret;
        }
    }

    return buf - (end - size);
}
#    undef rest

/* Make an extra command line under given key word */
/* 根据给定关键字生成额外命令行。 */
static char* __init xbc_make_cmdline (const char* key)
{
    struct xbc_node* root;
    char*            new_cmdline;
    int              ret, len = 0;

    root = xbc_find_node (key);
    if (!root)
        return NULL;

    /* Count required buffer size */
    /* 先计算所需缓冲区大小。 */
    len = xbc_snprint_cmdline (NULL, 0, root);
    if (len <= 0)
        return NULL;

    new_cmdline = memblock_alloc (len + 1, SMP_CACHE_BYTES);
    if (!new_cmdline) {
        pr_err ("Failed to allocate memory for extra kernel cmdline.\n");
        return NULL;
    }

    ret = xbc_snprint_cmdline (new_cmdline, len + 1, root);
    if (ret < 0 || ret > len) {
        pr_err ("Failed to print extra kernel cmdline.\n");
        memblock_free (new_cmdline, len + 1);
        return NULL;
    }

    return new_cmdline;
}

static int __init bootconfig_params (char* param, char* val, const char* unused, void* arg)
{
    if (strcmp (param, "bootconfig") == 0) {
        bootconfig_found = true;
    }
    return 0;
}

static int __init warn_bootconfig (char* str)
{
    /* The 'bootconfig' has been handled by bootconfig_params(). */
    /* bootconfig 参数已经由 bootconfig_params() 处理，这里只负责抑制告警。 */
    return 0;
}

static void __init setup_boot_config (void)
{
    static char tmp_cmdline[COMMAND_LINE_SIZE] __initdata;
    const char *msg, *data;
    int         pos, ret;
    size_t      size;
    char*       err;

    /* Cut out the bootconfig data even if we have no bootconfig option */
    /* 即使命令行没有 bootconfig 选项，也先尝试从 initrd 中剥离 bootconfig 数据。 */
    data = get_boot_config_from_initrd (&size);
    /* If there is no bootconfig in initrd, try embedded one. */
    /* 如果 initrd 中没有 bootconfig，则尝试使用内嵌 bootconfig。 */
    if (!data)
        data = xbc_get_embedded_bootconfig (&size);

    strscpy (tmp_cmdline, boot_command_line, COMMAND_LINE_SIZE);
    err = parse_args ("bootconfig", tmp_cmdline, NULL, 0, 0, 0, NULL, bootconfig_params);

    if (IS_ERR (err) || !(bootconfig_found || IS_ENABLED (CONFIG_BOOT_CONFIG_FORCE)))
        return;

    /* parse_args() stops at the next param of '--' and returns an address */
    /* parse_args() 遇到 "--" 后会停止，并返回 init 参数起始位置。 */
    if (err)
        initargs_offs = err - tmp_cmdline;

    if (!data) {
        /* If user intended to use bootconfig, show an error level message */
        /* 如果用户显式要求使用 bootconfig，则缺失数据应按错误级别提示。 */
        if (bootconfig_found)
            pr_err ("'bootconfig' found on command line, but no bootconfig found\n");
        else
            pr_info ("No bootconfig data provided, so skipping bootconfig");
        return;
    }

    if (size >= XBC_DATA_MAX) {
        pr_err ("bootconfig size %ld greater than max size %d\n", (long)size, XBC_DATA_MAX);
        return;
    }

    ret = xbc_init (data, size, &msg, &pos);
    if (ret < 0) {
        if (pos < 0)
            pr_err ("Failed to init bootconfig: %s.\n", msg);
        else
            pr_err ("Failed to parse bootconfig: %s at %d.\n", msg, pos);
    } else {
        xbc_get_info (&ret, NULL);
        pr_info ("Load bootconfig: %ld bytes %d nodes\n", (long)size, ret);
        /* keys starting with "kernel." are passed via cmdline */
        /* 以 "kernel." 开头的键会转换为内核命令行参数。 */
        extra_command_line = xbc_make_cmdline ("kernel");
        /* Also, "init." keys are init arguments */
        /* 以 "init." 开头的键会作为用户态 init 参数。 */
        extra_init_args    = xbc_make_cmdline ("init");
    }
    return;
}

static void __init exit_boot_config (void)
{
    xbc_exit ();
}

#else /* !CONFIG_BOOT_CONFIG */

static void __init setup_boot_config (void)
{
    /* Remove bootconfig data from initrd */
    /* 即使未启用 CONFIG_BOOT_CONFIG，也要从 initrd 中剥离 bootconfig 数据。 */
    get_boot_config_from_initrd (NULL);
}

static int __init warn_bootconfig (char* str)
{
    pr_warn ("WARNING: 'bootconfig' found on the kernel command line but CONFIG_BOOT_CONFIG is not set.\n");
    return 0;
}

#    define exit_boot_config() \
        do {                   \
        } while (0)

#endif /* CONFIG_BOOT_CONFIG */

early_param ("bootconfig", warn_bootconfig);

bool __init cmdline_has_extra_options (void)
{
    return extra_command_line || extra_init_args;
}

/* Change NUL term back to "=", to make "param" the whole string. */
/* 将解析时写入的 NUL 还原为 "="，让 param 重新表示完整参数字符串。 */
static void __init repair_env_string (char* param, char* val)
{
    if (val) {
        /* param=val or param="val"? */
        /* 兼容 param=val 和 param="val" 两种形式。 */
        if (val == param + strlen (param) + 1)
            val[-1] = '=';
        else if (val == param + strlen (param) + 2) {
            val[-2] = '=';
            memmove (val - 1, val, strlen (val) + 1);
        } else
            BUG ();
    }
}

/* Anything after -- gets handed straight to init. */
/* 命令行中 "--" 之后的所有内容都会直接传递给用户态 init。 */
static int __init set_init_arg (char* param, char* val, const char* unused, void* arg)
{
    unsigned int i;

    if (panic_later)
        return 0;

    repair_env_string (param, val);

    for (i = 0; argv_init[i]; i++) {
        if (i == MAX_INIT_ARGS) {
            panic_later = "init";
            panic_param = param;
            return 0;
        }
    }
    argv_init[i] = param;
    return 0;
}

/*
 * Unknown boot options get handed to init, unless they look like
 * unused parameters (modprobe will find them in /proc/cmdline).
 *
 * 未被内核识别的启动选项通常会转交给 init；但如果它们看起来像未使用的
 * 模块参数，则保留在 /proc/cmdline 中供 modprobe 等工具处理。
 */
static int __init unknown_bootoption (char* param, char* val, const char* unused, void* arg)
{
    size_t len = strlen (param);

    /* Handle params aliased to sysctls */
    /* 处理映射为 sysctl 的别名参数。 */
    if (sysctl_is_alias (param))
        return 0;

    repair_env_string (param, val);

    /* Handle obsolete-style parameters */
    /* 处理旧式 __setup 参数。 */
    if (obsolete_checksetup (param))
        return 0;

    /* Unused module parameter. */
    /* 带点号的参数通常视为未使用的模块参数。 */
    if (strnchr (param, len, '.'))
        return 0;

    if (panic_later)
        return 0;

    if (val) {
        /* Environment option */
        /* 形如 name=value 的未知参数作为 init 环境变量。 */
        unsigned int i;
        for (i = 0; envp_init[i]; i++) {
            if (i == MAX_INIT_ENVS) {
                panic_later = "env";
                panic_param = param;
            }
            if (!strncmp (param, envp_init[i], len + 1))
                break;
        }
        envp_init[i] = param;
    } else {
        /* Command line option */
        /* 没有取值的未知参数作为 init 命令行参数。 */
        unsigned int i;
        for (i = 0; argv_init[i]; i++) {
            if (i == MAX_INIT_ARGS) {
                panic_later = "init";
                panic_param = param;
            }
        }
        argv_init[i] = param;
    }
    return 0;
}

static int __init init_setup (char* str)
{
    unsigned int i;

    execute_command = str;
    /*
     * In case LILO is going to boot us with default command line,
     * it prepends "auto" before the whole cmdline which makes
     * the shell think it should execute a script with such name.
     * So we ignore all arguments entered _before_ init=... [MJ]
     *
     * LILO 使用默认命令行启动时可能会在前面加上 "auto"，导致 shell
     * 误以为要执行同名脚本。因此 init= 之前的参数在这里都会丢弃。
     */
    for (i = 1; i < MAX_INIT_ARGS; i++)
        argv_init[i] = NULL;
    return 1;
}
__setup ("init=", init_setup);

static int __init rdinit_setup (char* str)
{
    unsigned int i;

    ramdisk_execute_command = str;
    /* See "auto" comment in init_setup */
    /* 同 init_setup() 中关于 "auto" 的处理逻辑。 */
    for (i = 1; i < MAX_INIT_ARGS; i++)
        argv_init[i] = NULL;
    return 1;
}
__setup ("rdinit=", rdinit_setup);

#ifndef CONFIG_SMP
static inline void setup_nr_cpu_ids (void)
{
}
static inline void smp_prepare_cpus (unsigned int maxcpus)
{
}
#endif

/*
 * We need to store the untouched command line for future reference.
 * We also need to store the touched command line since the parameter
 * parsing is performed in place, and we should allow a component to
 * store reference of name/value for future reference.
 *
 * 这里同时保存原始命令行和解析用命令行。原始副本供后续查询使用；
 * 解析副本会被 parse_args() 原地拆分，方便各子系统长期引用参数名和值。
 */
static void __init setup_command_line (char* command_line)
{
    size_t len, xlen = 0, ilen = 0;

    if (extra_command_line)
        xlen = strlen (extra_command_line);
    if (extra_init_args) {
        extra_init_args = strim (extra_init_args);      /* remove trailing space */
        ilen            = strlen (extra_init_args) + 4; /* for " -- " */
        /* 去掉末尾空白；长度额外预留 " -- " 分隔符。 */
    }

    len                = xlen + strlen (boot_command_line) + ilen + 1;

    saved_command_line = memblock_alloc (len, SMP_CACHE_BYTES);
    if (!saved_command_line)
        panic ("%s: Failed to allocate %zu bytes\n", __func__, len);

    len                 = xlen + strlen (command_line) + 1;

    static_command_line = memblock_alloc (len, SMP_CACHE_BYTES);
    if (!static_command_line)
        panic ("%s: Failed to allocate %zu bytes\n", __func__, len);

    if (xlen) {
        /*
         * We have to put extra_command_line before boot command
         * lines because there could be dashes (separator of init
         * command line) in the command lines.
         *
         * extra_command_line 必须放在原启动命令行之前，因为原命令行中
         * 可能包含 "--" 这个 init 参数分隔符。
         */
        strcpy (saved_command_line, extra_command_line);
        strcpy (static_command_line, extra_command_line);
    }
    strcpy (saved_command_line + xlen, boot_command_line);
    strcpy (static_command_line + xlen, command_line);

    if (ilen) {
        /*
         * Append supplemental init boot args to saved_command_line
         * so that user can check what command line options passed
         * to init.
         * The order should always be
         * " -- "[bootconfig init-param][cmdline init-param]
         *
         * 将额外 init 参数追加到 saved_command_line，便于用户查看最终传给
         * init 的参数；顺序固定为 bootconfig init 参数在前，命令行 init 参数在后。
         */
        if (initargs_offs) {
            len = xlen + initargs_offs;
            strcpy (saved_command_line + len, extra_init_args);
            len += ilen - 4; /* strlen(extra_init_args) */
            strcpy (saved_command_line + len, boot_command_line + initargs_offs - 1);
        } else {
            len = strlen (saved_command_line);
            strcpy (saved_command_line + len, " -- ");
            len += 4;
            strcpy (saved_command_line + len, extra_init_args);
        }
    }

    saved_command_line_len = strlen (saved_command_line);
}

/*
 * We need to finalize in a non-__init function or else race conditions
 * between the root thread and the init thread may cause start_kernel to
 * be reaped by free_initmem before the root thread has proceeded to
 * cpu_idle.
 *
 * gcc-3.4 accidentally inlines this function, so use noinline.
 *
 * 收尾工作必须放在非 __init 函数中完成，否则 root 线程和 init 线程之间
 * 可能竞争，导致 root 线程进入 cpu_idle 前 start_kernel 所在 init 内存
 * 已被 free_initmem() 回收。noinline 用于避免旧版 gcc 意外内联该函数。
 */

static __initdata                     DECLARE_COMPLETION (kthreadd_done);

static noinline void __ref __noreturn rest_init (void)
{
    struct task_struct* tsk;
    int                 pid;

    rcu_scheduler_starting ();
    /*
     * We need to spawn init first so that it obtains pid 1, however
     * the init task will end up wanting to create kthreads, which, if
     * we schedule it before we create kthreadd, will OOPS.
     *
     * 必须先创建 init 以确保它获得 PID 1；但 init 之后会创建内核线程，
     * 因此在 kthreadd 创建完成前不能真正调度它运行。
     */
    pid = user_mode_thread (kernel_init, NULL, CLONE_FS);
    /*
     * Pin init on the boot CPU. Task migration is not properly working
     * until sched_init_smp() has been run. It will set the allowed
     * CPUs for init to the non isolated CPUs.
     *
     * 在 sched_init_smp() 完成前任务迁移尚不可用，因此先把 init 固定在
     * 启动 CPU 上；后续会把它的可运行 CPU 调整为非隔离 CPU 集合。
     */
    rcu_read_lock ();
    tsk = find_task_by_pid_ns (pid, &init_pid_ns);
    tsk->flags |= PF_NO_SETAFFINITY;
    set_cpus_allowed_ptr (tsk, cpumask_of (smp_processor_id ()));
    rcu_read_unlock ();

    numa_default_policy ();
    pid = kernel_thread (kthreadd, NULL, NULL, CLONE_FS | CLONE_FILES);
    rcu_read_lock ();
    kthreadd_task = find_task_by_pid_ns (pid, &init_pid_ns);
    rcu_read_unlock ();

    /*
     * Enable might_sleep() and smp_processor_id() checks.
     * They cannot be enabled earlier because with CONFIG_PREEMPTION=y
     * kernel_thread() would trigger might_sleep() splats. With
     * CONFIG_PREEMPT_VOLUNTARY=y the init task might have scheduled
     * already, but it's stuck on the kthreadd_done completion.
     *
     * 从这里开始启用 might_sleep() 和 smp_processor_id() 等调试检查。
     * 太早启用会让 kernel_thread() 在抢占配置下触发误报；自愿抢占配置中，
     * init 即使已被调度，也会等待 kthreadd_done 完成量。
     */
    system_state = SYSTEM_SCHEDULING;

    complete (&kthreadd_done);

    /*
     * The boot idle thread must execute schedule()
     * at least once to get things moving:
     *
     * 启动 idle 线程至少要执行一次 schedule()，让刚创建的任务真正开始运行。
     */
    schedule_preempt_disabled ();
    /* Call into cpu_idle with preempt disabled */
    /* 在抢占关闭状态下进入 CPU idle 循环。 */
    cpu_startup_entry (CPUHP_ONLINE);
}

/* Check for early params. */
/* 检查并处理 early_param 注册的早期启动参数。 */
static int __init do_early_param (char* param, char* val, const char* unused, void* arg)
{
    const struct obs_kernel_param* p;

    for (p = __setup_start; p < __setup_end; p++) {
        if ((p->early && parameq (param, p->str)) || (strcmp (param, "console") == 0 && strcmp (p->str, "earlycon") == 0)) {
            if (p->setup_func (val) != 0) {
                pr_warn ("Malformed early option '%s'\n", param);
            }
        }
    }
    /* We accept everything at this stage. */
    /* 早期参数阶段不拒绝未知项，真正处理留给后续解析流程。 */
    return 0;
}

void __init parse_early_options (char* cmdline)
{
    parse_args ("early options", cmdline, NULL, 0, 0, 0, NULL, do_early_param);
}

/* Arch code calls this early on, or if not, just before other parsing. */
/* 架构代码可提前调用；若未调用，则在常规参数解析前调用。 */
void __init parse_early_param (void)
{
    static int done __initdata;
    static char     tmp_cmdline[COMMAND_LINE_SIZE] __initdata;

    if (done) {
        return;
    }

    /* All fall through to do_early_param. */
    /* 所有参数最终都落到 do_early_param() 中筛选处理。 */
    strscpy (tmp_cmdline, boot_command_line, COMMAND_LINE_SIZE);
    parse_early_options (tmp_cmdline);
    done = 1;
}

void __init __weak arch_post_acpi_subsys_init (void)
{
}

void __init __weak smp_setup_processor_id (void)
{
}

void __init __weak smp_prepare_boot_cpu (void)
{
}

#if THREAD_SIZE >= PAGE_SIZE
void __init __weak thread_stack_cache_init (void)
{
}
#endif

void __init __weak poking_init (void)
{
}

void __init __weak pgtable_cache_init (void)
{
}

void __init __weak trap_init (void)
{
}

bool initcall_debug;
core_param (initcall_debug, initcall_debug, bool, 0644);

#ifdef TRACEPOINTS_ENABLED
static void __init initcall_debug_enable (void);
#else
static inline void initcall_debug_enable (void)
{
}
#endif

#ifdef CONFIG_RANDOMIZE_KSTACK_OFFSET
DEFINE_STATIC_KEY_MAYBE_RO (CONFIG_RANDOMIZE_KSTACK_OFFSET_DEFAULT, randomize_kstack_offset);
DEFINE_PER_CPU (u32, kstack_offset);

static int __init early_randomize_kstack_offset (char* buf)
{
    int  ret;
    bool bool_result;

    ret = kstrtobool (buf, &bool_result);
    if (ret)
        return ret;

    if (bool_result)
        static_branch_enable (&randomize_kstack_offset);
    else
        static_branch_disable (&randomize_kstack_offset);
    return 0;
}
early_param ("randomize_kstack_offset", early_randomize_kstack_offset);
#endif

static void __init print_unknown_bootoptions (void)
{
    char*              unknown_options;
    char*              end;
    const char* const* p;
    size_t             len;

    if (panic_later || (!argv_init[1] && !envp_init[2]))
        return;

    /*
     * Determine how many options we have to print out, plus a space
     * before each
     *
     * 先统计需要打印的未知参数总长度，并为每个参数预留前导空格。
     */
    len = 1; /* null terminator */
    for (p = &argv_init[1]; *p; p++) {
        len++;
        len += strlen (*p);
    }
    for (p = &envp_init[2]; *p; p++) {
        len++;
        len += strlen (*p);
    }

    unknown_options = memblock_alloc (len, SMP_CACHE_BYTES);
    if (!unknown_options) {
        pr_err ("%s: Failed to allocate %zu bytes\n", __func__, len);
        return;
    }
    end = unknown_options;

    for (p = &argv_init[1]; *p; p++)
        end += sprintf (end, " %s", *p);
    for (p = &envp_init[2]; *p; p++)
        end += sprintf (end, " %s", *p);

    /* Start at unknown_options[1] to skip the initial space */
    /* 从 unknown_options[1] 开始打印，用于跳过第一个前导空格。 */
    pr_notice ("Unknown kernel command line parameters \"%s\", will be passed to user space.\n", &unknown_options[1]);
    memblock_free (unknown_options, len);
}

static void __init early_numa_node_init (void)
{
#ifdef CONFIG_USE_PERCPU_NUMA_NODE_ID
#    ifndef cpu_to_node
    int cpu;

    /* The early_cpu_to_node() should be ready here. */
    /* 此时 early_cpu_to_node() 应已可用，可据此设置 CPU 所属 NUMA 节点。 */
    for_each_possible_cpu (cpu)
        set_cpu_numa_node (cpu, early_cpu_to_node (cpu));
#    endif
#endif
}

asmlinkage __visible __init __no_sanitize_address __noreturn __no_stack_protector void start_kernel (void)
{
    char* command_line;
    char* after_dashes;

    set_task_stack_end_magic (&init_task);      // init_task, 内核启动的第一个进程
    smp_setup_processor_id ();
    debug_objects_early_init ();
    init_vmlinux_build_id ();                       // mark_rodata_ro() 初始化后相关变量变为只读

    cgroup_init_early ();

    local_irq_disable ();
    early_boot_irqs_disabled = true;

    /**
     * Interrupts are still disabled. Do necessary setups, then
     * enable them.
     */
    boot_cpu_init ();       // 设置CPU0的状态为可用
    page_address_init ();   // 空实现
    pr_notice ("%s", linux_banner);
    setup_arch (&command_line);

    /* Static keys and static calls are needed by LSMs */
    // jump_label_init() 的主要任务是遍历内核镜像中的 __jump_table 节，并将代码中预留的占位指令替换为最优的跳转指令。
    // 默认状态：在编译时，为了保证逻辑正确，内核在所有 static_branch 位置通常预留了一个 5 字节的 NOP 指令。
    // 初始化动作：该函数会根据每个分支的初始布尔值（True 或 False），决定是将 NOP 保持原样，还是将其修改为 jmp (绝对跳转) 指令。
    jump_label_init ();

    // 静态调用是为了解决**间接跳转（Indirect Calls）**带来的性能损耗而设计的。
    // 传统方式：内核中许多地方使用函数指针（如 ops->func()）。在 CPU 层面，这表现为 call *%rax。由于 2025 年依然受 Spectre 等漏洞影响，这类跳转通常需要经过复杂的 Retpoline 或硬件防护（如 IBT），性能开销极大。
    // Static Call 方式：static_call_init() 会遍历内核镜像中的 .static_call_sites 节，将原本通过指针跳转的间接指令，硬编码替换为直接的 call <address> 或 jmp <address> 指令。
    static_call_init ();

    // 内核启动早期用于初始化 LSM（Linux Security Modules，Linux 安全模块） 框架的关键函数
    // 它确保了安全策略在系统运行任何用户态代码或挂载复杂文件系统之前就已经生效
    early_security_init ();
    setup_boot_config ();
    setup_command_line (command_line);

    // setup_nr_cpu_ids() 是一个用于确定系统逻辑 CPU 数量上限的关键初始化函数。
    // 该函数的主要任务是计算并设置全局变量 nr_cpu_ids
    setup_nr_cpu_ids ();

    // 是为每个 CPU 核心分配并建立独立的静态 Per-CPU 变量区域
    // 在 vmlinux 静态镜像中，所有的 Per-CPU 变量都存放在 .data..percpu 段中，这只是一个母本（Template）
    // 1. 计算需求：根据前一步 setup_nr_cpu_ids() 确定的 CPU 数量，计算总内存需求
    // 2. 申请内存：为每一个可能的 CPU 核心申请一块足够大的物理内存
    // 3. 克隆数据：将 .data..percpu 段中的初始化数据完整地“复印”到每一块新申请的内存中
    // 4. 建立映射：设置基地址寄存器(如 x86 的 GS 段基址, ARM64的TPIDR_EL1),使得每个 CPU 在访问同一个变量名时, 实际指向的是属于自己的那个副本
    setup_per_cpu_areas ();

    // 为当前正在执行启动流程的那个 CPU(即 Boot CPU / CPU 0)完成最后的运行环境设置, 使其具备运行完整内核任务的能力
    // 该函数紧随 setup_per_cpu_areas() 执行, 标志着系统从“单核模式”向“多核准备模式”的过渡
    // 虽然此时系统还没有唤醒其他从核心(Secondary CPUs), 但 Boot CPU 需要先把自己武装好
    // 1. 绑定 Per-CPU 区域：将之前由 setup_per_cpu_areas() 分配好的内存副本正式关联到当前 CPU 的寄存器(如 x86 的 GS 寄存器或 ARM64 的 TPIDR_ELx)
    // 2. 初始化 CPU 映射表：将当前 CPU 标记为 present(存在)和 online(在线)
    // 3. 设置 CPU 状态索引：在内核的 cpu_data 结构体中填充当前硬件的详细信息(如缓存大小、特性标志、型号 ID 等)
    smp_prepare_boot_cpu (); /* arch-specific boot-cpu hooks */

    // 在某些架构如 x86 中被称为 numa_init 序列的一部分是内核启动早期用于建立 NUMA（非一致存储访问） 拓扑结构的初始化函数。
    // 内存被物理地连接在不同的 CPU 插槽或核心簇上。访问本地内存比跨节点访问远程内存快得多
    early_numa_node_init ();

    // 它为系统中每一个潜在的 CPU 核心建立起一套状态管理机制，使得 CPU 可以在系统运行期间动态地“上线”或“离线”。
    boot_cpu_hotplug_init ();

    pr_notice ("Kernel command line: %s\n", saved_command_line);
    /* parameters may set static keys */
    parse_early_param ();
    after_dashes = parse_args ("Booting kernel",
        static_command_line,
        __start___param,
        __stop___param - __start___param,
        -1,
        -1,
        NULL,
        &unknown_bootoption);
    print_unknown_bootoptions ();
    if (!IS_ERR_OR_NULL (after_dashes))
        parse_args ("Setting init args",
            after_dashes,
            NULL,
            0,
            -1,
            -1,
            NULL,
            set_init_arg);
    if (extra_init_args)
        parse_args ("Setting extra init args",
            extra_init_args,
            NULL,
            0,
            -1,
            -1,
            NULL,
            set_init_arg);

    /* Architectural and non-timekeeping rng init, before allocator init */
    random_init_early (command_line);

    /*
     * These use large bootmem allocations and must precede
     * initalization of page allocator
     */
    setup_log_buf (0);

    // 是虚拟文件系统(VFS)启动最早期执行的初始化函数
    // 它在 start_kernel() 的极早期阶段被调用,主要负责为内核中最核心的两个缓存: Dentry Cache(目录项缓存)和 Inode Cache(索引节点缓存)分配初始的哈希表空间
    // 
    vfs_caches_init_early ();

    // 内核启动早期的一个关键函数,其核心任务是对内核主镜像中的异常处理表(Exception Table, __ex_table)进行排序.
    // 排序目的：在链接阶段, 各 .o 文件中的异常条目是按编译顺序合并的, 地址是乱序的.
    // sort_main_extable() 使用二分查找算法(Binary Search)的逻辑, 将这些条目按指令地址(insn)从小到大重新排序
    sort_main_extable ();

    // 初始化硬件异常处理. 它定义了当 CPU 遇到非法操作（如除零、页错误、非法指令）时，应该跳转到哪段内核代码去处理
    trap_init ();

    // 内核内存管理子系统初始化的核心枢纽函数
    // 标志着内核从早期简单的内存管理(memblock) 向功能完整的核心内存管理(Core Memory Management)的正式转变
    mm_core_init ();

    // 内核启动早期的一个安全初始化函数. 它的核心任务是初始化用于修改只读代码段的特殊映射机制
    // 为了防止攻击者篡改指令，内核代码段().text)被标记为只读(Read-Only)和不可写(NX).
    // 然而, 内核在运行时仍需修改指令（例如: Ftrace 挂载, Jump Label 开关、静态调用替换等). 直接修改代码段会触发页错误(Page Fault).
    // poking_init() 建立了一套名为 Text Poking 的机制, 允许内核在严格受控的情况下, “临时”且“安全”地重写这些受保护的代码区域。
    // 创建专用页表(Poking Address Space): poking_init() 会在内存中申请一个专用的、私有的虚拟地址空间副本
    // 建立影子映射(Shadow Mapping): 它将实际的只读代码段映射到这个特殊的虚拟地址上, 但在该地址下赋予其可写权限.
    // 初始化写保护切换逻辑: 确保每次修改指令时, CPU 都能通过这个临时的“窗口”写入新指令, 并在写入完成后立即失效, 从而防止恶意程序利用该窗口.
    poking_init ();

    // ftrace_init() 是内核启动阶段用于初始化 函数追踪器 (Function Tracer) 的核心函数. 它负责建立起内核动态代码分析和性能监控的基础框架.
    // ftrace_init() 最重要的任务是对内核指令进行“预处理”
    // 1. 扫描 __mcount_loc 节：在编译内核时，如果开启了 CONFIG_DYNAMIC_FTRACE，编译器会在每个函数的入口处插入一条特殊的调用指令(通常是 mcount 或 __fentry__).编译器还会将这些指令的地址全部记录在 vmlinux 的 __mcount_loc 节中
    // 2. 指令热补丁 (Hot-patching): ftrace_init() 启动后, 会遍历这些地址, 并将所有的 call 指令替换为 NOP (空指令)
    // 3. 目的: 这样在默认情况下, 内核运行速度几乎不受影响(零开销). 只有当你通过 /sys/kernel/tracing 开启某个函数的追踪时, ftrace 才会再次利用 poking_init() 建立的机制, 将 NOP 动态替换回跳转指令.
    ftrace_init ();

    /* trace_printk can be enabled here */
    // early_trace_init() 是内核追踪子系统的第一阶段初始化函数. 它负责在系统启动早期(甚至在文件系统和中断完全就绪前)建立最基础的日志记录能力
    early_trace_init ();

    /*
     * Set up the scheduler prior starting any interrupts (such as the
     * timer interrupt). Full topology setup happens at smp_init()
     * time - but meanwhile we still have a functioning scheduler.
     */
    // sched_init() 是内核启动序列中最核心的初始化函数之一. 它的任务是从零开始构建进程调度系统, 为内核从“单任务启动状态”转变为“多任务多 CPU 状态”奠定基础.
    sched_init ();

    if (WARN (!irqs_disabled (), "Interrupts were enabled *very* early, fixing it\n")) {
        local_irq_disable ();
    }
    // 内核启动早期用于初始化基数树(Radix Tree)
    // 基数树是一种空间利用率极高且查找速度极快的紧凑多路搜索树
    // 由于基数树的节点（Nodes）在系统运行期间会被极其频繁地创建和销毁（例如每打开一个文件、每缓存一个内存页都会用到），如果每次都通过标准的内存分配器申请内存，性能开销将不可接受
    // radix_tree_init() 的主要任务是:
    // 1. 初始化 Slab 缓存：调用 kmem_cache_create() 为基数树节点创建一个专用的内存池(通常称为 radix_tree_node_cachep)
    // 2. 配置节点预分配逻辑：设置内核在内存紧张时如何预留基数树节点, 以确保关键路径(如页高速缓存查找)不会因为内存不足而阻塞
    radix_tree_init ();

    // 它标志着内核现代数据结构框架的建立，用于取代老旧的红黑树（rbtree）来管理内存区域
    // 枫树（Maple Tree）是 Linux 内核近年来引入的一种 B-tree 变体，专门为管理虚拟内存区域（VMA）而设计。maple_tree_init() 的主要任务是:
    // 1. 初始化 Slab 缓存：调用 kmem_cache_create() 为枫树节点（Maple Nodes）创建一个名为 maple_node 的专用内存池
    // 2. 配置并发属性：确保枫树节点在多核并发访问时符合 RCU(Read-Copy-Update) 安全要求
    // 3. 优化内存布局：由于枫树节点的大小通常经过精心设计以匹配 CPU 缓存行(Cache Line),该函数负责在分配时确保存储对齐, 从而最大限度地减少 2025 年高性能处理器上的缓存失效
    maple_tree_init ();

    /*
     * Set up housekeeping before setting up workqueues to allow the unbound
     * workqueue to take non-housekeeping into account.
     */
    // 专门用于处理 “内核管家任务”的隔离
    // 在多核处理器中，内核需要执行许多背景任务（如 RCU 回调、时钟滴答处理、内核线程工作队列等）。这些任务被称为 “Housekeeping”
    // housekeeping_init() 的任务是根据用户在启动参数中设置的 isolcpus= 或 nohz_full= 指令，将系统中的 CPU 划分为两类:
    // 1. Housekeeping CPUs: 负责处理所有的内核杂务(打扫房间)
    // 2. Isolated CPUs(隔离核): 尽量免除内核杂务的打扰, 全身心投入运行用户指定的关键应用
    housekeeping_init ();

    /*
     * Allow workqueue creation and work item queueing/cancelling
     * early.  Work item execution depends on kthreads and starts after
     * workqueue_init().
     */
    // workqueue_init_early() 是通用工作队列(Workqueue, wq)子系统的第一阶段初始化函数
    // 它的核心任务是在系统启动的极早期(甚至在中断和完整内存管理就绪前),建立起最基础的任务异步执行机制
    workqueue_init_early ();

    // 是内核启动早期用于初始化 RCU (Read-Copy-Update) 机制的核心函数
    // 是 Linux 内核实现高并发、无锁读取的关键技术，几乎所有内核子系统（如网络、文件系统、调度器）都极度依赖它
    rcu_init ();

    /* Trace events are available after this */
    // 核追踪子系统（Tracing System）的第二阶段完整初始化函数
    trace_init ();

    if (initcall_debug) {
        // initcall_debug_enable 是一个全局布尔变量(标志位), 用于控制内核在启动过程中是否打印每一个初始化函数(initcall)的详细执行信息
        initcall_debug_enable ();
    }

    // context_tracking_init() 是内核启动后期的一个关键函数，主要用于初始化上下文跟踪(Context Tracking)子系统
    // 它是实现 NO_HZ_FULL(全无滴答/无中断)模式的核心技术基础
    // 该函数的主要任务是初始化用于追踪 CPU 在用户态（User）、内核态（Kernel）和异常上下文之间切换的状态机
    // 核心功能：监控 CPU 状态切换:
    // 1. 状态监测: 当 CPU 进入或退出用户态时, 上下文跟踪系统会捕获这些切换
    // 2. 触发 RCU 宽限期: 如果某个核心被设置为隔离核(Isolated CPU), 上下文跟踪会通知 RCU 子系统: “该核心已进入用户态, 不再需要通过时钟滴答来同步 RCU 宽限期”
    // 3. 虚拟化支持: 在 2025 年的云计算场景中, 该函数也为 KVM 客户机切换时的上下文跟踪提供支持
    context_tracking_init ();

    /* init some links before init_ISA_irqs() */
    // 负责在系统启动早期，为内核建立起最基础的中断描述符（Interrupt Descriptors）管理架构
    early_irq_init ();
    // 初始化架构相关的中断控制器和中断入口，使硬件中断能够被内核接收和分发
    init_IRQ ();
    // 初始化内核时钟 tick 框架，为周期性调度时钟和时间统计做准备
    tick_init ();
    // 初始化 RCU 的 nohz 支持，使无滴答 CPU 也能正确参与 RCU 宽限期推进
    rcu_init_nohz ();
    // 初始化低精度定时器子系统，即传统 timer_list 机制
    init_timers ();

    // 初始化可睡眠 RCU (Sleepable Read-Copy-Update) 子系统的函数
    srcu_init ();

    // hrtimers_init() 是内核启动早期用于初始化 高精度定时器(High-Resolution Timers)框架的核心函数
    hrtimers_init ();
    // 初始化软中断框架，为 NET_RX、TIMER、TASKLET 等延后执行上下文建立入口
    softirq_init ();
    // 初始化内核计时核心，建立 wall time、monotonic time 等基础时间源状态
    timekeeping_init ();
    // 执行架构相关时间初始化，通常会配置 clocksource、clockevent 或平台定时器
    time_init ();

    /* This must be after timekeeping is initialized */
    /* 随机数子系统依赖已初始化的计时信息，因此必须放在 timekeeping_init() 之后。 */
    random_init ();

    /* These make use of the fully initialized rng */
    // 内核启动早期用于初始化 KFENCE (Kernel Electric Fence) 的函数
    // KFENCE 是一种低开销、基于内存分页的检测工具, 专门用于捕获内核中的堆溢出(Heap out-of-bounds),
    // 释放后使用(Use-after-free)等内存错误. 它被设计为可以在生产环境（Production）中长期开启
    kfence_init ();

    // boot_init_stack_canary() 是一个至关重要的安全初始化函数.
    // 它的核心任务是为内核启动过程中的第一个进程（即 init_task，0 号进程）设置栈金丝雀（Stack Canary）
    // 这是防御 栈溢出攻击(Stack Smashing) 的第一道防线
    // 该函数利用硬件生成的随机数或高质量熵源, 生成一个不可预测的数值(Canary), 并将其注入到 init_task 的进程描述符中
    // 原理: 在函数调用时, 编译器会在栈帧的返回地址前插入这个随机值. 函数返回前会检查该值是否被篡改. 如果黑客通过溢出覆盖了返回地址, 必然会破坏这个“金丝雀”值, 内核检测到不一致后会立即触发 Panic，阻止恶意代码执行
    // 全局设置: 在 x86 架构下, 该函数还会将生成的随机值写入特殊的段寄存器(如 %gs:40), 以便编译器生成的检查代码能快速访问
    boot_init_stack_canary ();

    // perf_event_init() 是内核性能监测子系统(Perf Events)的核心初始化函数. 它负责建立起一套统一的、能够跨越 CPU 硬件和内核软件的观测基础设施
    // 主要任务是初始化性能事件的管理框架，使用户态工具（如 perf 命令）能够捕获各种系统指标
    perf_event_init ();

    // profile_init() 是内核启动早期用于初始化旧版内核剖析工具(Kernel Profiling)的函数
    // 虽然现代内核更倾向于使用更强大的 Perf 和 eBPF，但 profile_init() 所代表的机制仍然作为基础的采样手段存在于内核中
    // profile_init() 的主要任务是为内核提供一种简单的统计方法，用来查看 CPU 时间主要消耗在哪些内核函数上
    // 1. 分配剖析缓冲区 (Profiling Buffer)：根据内核启动参数 profile=N（N 通常是位移值，用于定义采样精度），该函数会调用 memblock_alloc() 分配一块连续内存
    // 2. 建立地址映射：将内核的代码段（.text）划分为多个小的直方图区间（bins）
    // 3. 采样逻辑：一旦初始化完成，每当系统发生时钟中断（Tick）时，如果 CPU 处于内核态，内核就会记录当前的程序计数器（PC/IP）落在哪个区间，并增加该区间的计数
    profile_init ();

    // call_function_init() 是内核启动早期用于初始化 SMP（对称多处理）函数调用基础设施 的函数
    // 它是实现 IPI（Inter-Processor Interrupts，核间中断） 机制的逻辑基础，允许一个 CPU 核心指挥另一个核心执行特定的函数
    call_function_init ();
    WARN (!irqs_disabled (), "Interrupts were enabled early\n");

    early_boot_irqs_disabled = false;
    local_irq_enable ();

    // kmem_cache_init_late() 是 Slab/Slub 分配器初始化的最后一个阶段
    // 它在 start_kernel() 的后期执行, 此时系统的基础基础设施(如中断、文件系统、工作队列)已经就绪, 可以完成那些依赖于高级功能的内存池设置
    // 由于此时内核已经具备了完整的多线程和文件系统操作能力，kmem_cache_init_late() 主要负责:
    // 1. 启用 Slab 别名(Aliasing): 在系统启动初期，为了安全和调试，内核可能会创建许多独立的缓存。到了后期，为了节省内存，kmem_cache_init_late() 会扫描具有相同属性（大小、对齐等）的缓存，并将它们合并
    // 2. 注册 Sysfs 接口: 这是该函数最重要的任务之一。它在 /sys/kernel/slab 目录下为每一个活跃的 Slab 缓存创建文件夹和统计文件
    //    通过这些文件，用户可以观察 /sys/kernel/slab/<name>/objects 来监控内存使用情况
    // 3. 激活高性能特性: 初始化与 CPU 离线/在线 相关的 Slab 缓存动态调整逻辑
    // 4. 完成 Slab 调试工具初始化: 如果开启了 CONFIG_SLUB_DEBUG, 它会在此处配置更复杂的内存破坏检测逻辑
    kmem_cache_init_late ();

    /*
     * HACK ALERT! This is early. We're enabling the console before
     * we've done PCI setups etc, and console_init() must be aware of
     * this. But we do want output early, in case something goes wrong.
     */
    // 是内核启动序列中一个极具“仪式感”的时刻. 它的核心任务是: 初始化并激活真正的终端控制台设备, 使用户能够看到内核日志(dmesg)的输出
    // 在调用此函数之前, 所有的内核日志(printk)都只能存储在内存缓冲区(log_buf)中，或者通过极原始的 early_printk(如果硬件支持)输出
    console_init ();
    if (panic_later) {
        panic ("Too many boot %s vars at `%s'", panic_later, panic_param);
    }

    // lockdep_init() 是内核死锁检测器 (Lock Dependency Validator) 的核心初始化函数
    // 它是 Linux 内核中最强大的调试工具之一, 负责在系统运行过程中实时监控所有锁的加锁顺序, 以预测并防止潜在的死锁风险
    lockdep_init ();

    /*
     * Need to run this when irqs are enabled, because it wants
     * to self-test [hard/soft]-irqs on/off lock inversion bugs
     * too:
     */
    // locking_selftest() 是内核启动阶段的一个自检函数, 专门用于验证死锁检测器 (Lockdep) 的功能是否正常, 以及内核各种锁原语(Spinlock, Mutex, RWSEM 等)的正确性
    locking_selftest ();

#ifdef CONFIG_BLK_DEV_INITRD
    if (initrd_start
        && !initrd_below_start_ok
        && page_to_pfn (virt_to_page ((void*)initrd_start)) < min_low_pfn) {
        pr_crit ("initrd overwritten (0x%08lx < 0x%08lx) - disabling it.\n",
            page_to_pfn (virt_to_page ((void*)initrd_start)), min_low_pfn);
        initrd_start = 0;
    }
#endif
    // setup_per_cpu_pageset() 是内存管理子系统(MM)初始化的重要环节. 它的核心任务是为每个 CPU 核心初始化页面高速缓存(Per-CPU Pageset，简称 PCP)
    // 它是解决多核系统在频繁申请和释放内存页时产生锁竞争的关键机制
    // 在 Linux 的伙伴系统（Buddy System）中，全局的物理页分配是受锁保护的。如果成百上千个 CPU 同时申请内存，全局锁会成为性能瓶颈
    // 1. 初始化 PCP 结构：为每个 CPU 的每个内存管理区(Zone)初始化 struct per_cpu_pages
    // 2. 预填充本地缓存：配置每个 CPU 可以在私有缓存中保留多少个物理页(通常是 high 和 batch 参数)
    // 3. 实现无锁分配：当一个进程申请单张页面(Order 0)时, 内核会优先从当前 CPU 的 PCP 缓存中直接取走，完全不需要获取全局伙伴系统锁
    setup_per_cpu_pageset ();

    // numa_policy_init() 是内核启动后期用于初始化 NUMA 策略管理系统 的函数
    // 如果说 early_numa_node_init 是划分内存的“行政区划”, 那么 numa_policy_init() 就是制定“人口（数据）落户政策”的开端
    // 该函数的主要任务是初始化内核默认的内存分配策略，并为后续进程自定义策略提供基础设施：
    // 1. 设定默认策略 (Default Policy): 初始化全局默认的内存分配方式(通常是 MPOL_PREFERRED_LOCAL), 即优先从执行当前代码的 CPU 所在的本地节点分配内存
    // 2. 初始化 SLAB 缓存: 调用 kmem_cache_create() 为 struct mempolicy 创建专门的 Slab 缓存池. 由于每个进程、甚至每个内存区域都可以有独立的 NUMA 策略, 这些结构体会被频繁创建
    // 3. 支持共享内存策略: 为共享内存（shmem/tmpfs）中的任务初始化基于索引的策略管理框架
    numa_policy_init ();
    // ACPI 早期初始化，解析基础 ACPI 表并为后续电源管理、设备枚举做准备
    acpi_early_init ();
    if (late_time_init) {
        // 某些架构会在这里补充较晚阶段的时间子系统初始化
        late_time_init ();
    }
    // 初始化调度时钟，为调度器提供高频、低开销的运行时间统计
    sched_clock_init ();
    // 校准 loops_per_jiffy，用于早期 busy-wait 延时函数
    calibrate_delay ();

    // 执行架构级 CPU 初始化收尾工作，确保 CPU 特性和缓解措施进入最终状态
    arch_cpu_finalize_init ();

    // 初始化 PID 分配器使用的 IDR 结构
    pid_idr_init ();
    // 初始化匿名 VMA 缓存，用于进程匿名内存映射和反向映射管理
    anon_vma_init ();
#ifdef CONFIG_X86
    if (efi_enabled (EFI_RUNTIME_SERVICES)) {
        // x86 下进入 EFI 虚拟地址模式，之后可通过虚拟地址调用 EFI Runtime Services
        efi_enter_virtual_mode ();
    }
#endif
    // 初始化线程栈缓存，提升后续任务创建和销毁时的栈分配效率
    thread_stack_cache_init ();
    // 初始化凭据子系统，为 task_struct 的权限、用户、组信息提供缓存和初始对象
    cred_init ();
    // 初始化 fork/clone 所需的数据结构和限制，准备创建普通进程
    fork_init ();
    // 初始化 procfs 相关缓存，为 /proc 中的进程和内核视图做准备
    proc_caches_init ();
    // 初始化 UTS 命名空间，提供 hostname、domainname 等命名空间隔离基础
    uts_ns_init ();
    // 初始化内核 keyring 子系统，用于密钥、凭据和完整性校验材料管理
    key_init ();
    // 初始化完整的 Linux Security Modules 框架，注册并启用安全钩子
    security_init ();
    // 执行较晚阶段的调试基础设施初始化
    dbg_late_init ();
    // 初始化网络命名空间框架，为后续网络协议栈和设备隔离做准备
    net_ns_init ();
    // 初始化虚拟文件系统(VFS)所需的所有关键缓存结构, 并构建最初的文件系统层次结构
    vfs_caches_init ();

    // 初始化页缓存框架，支撑文件数据缓存和 address_space 管理
    pagecache_init ();
    // 初始化信号子系统，为进程间信号投递和默认处理逻辑建立基础
    signals_init ();
    // 初始化 seq_file 基础设施，供 /proc、debugfs 等顺序输出接口使用
    seq_file_init ();
    // 创建 /proc 根目录并注册核心 procfs 条目
    proc_root_init ();
    // 初始化 namespace 文件系统，用于通过文件描述符引用各类命名空间
    nsfs_init ();
    // 初始化 pidfs，用于以文件系统对象形式表示和管理 PID
    pidfs_init ();
    // 初始化 cpuset 控制器，提供 CPU/内存节点约束能力
    cpuset_init ();
    // 初始化 cgroup 核心框架和早期控制器状态
    cgroup_init ();
    // 初始化 taskstats 早期支持，用于任务统计信息导出
    taskstats_init_early ();
    // 初始化延迟记账子系统，统计任务在 I/O、调度、内存回收等路径上的等待时间
    delayacct_init ();

    // 初始化 ACPI 子系统主体，开始支持更完整的 ACPI 设备和电源管理功能
    acpi_subsystem_init ();
    // 架构可选的 ACPI 子系统初始化后钩子
    arch_post_acpi_subsys_init ();
    // 初始化 KCSAN 数据竞争检测框架
    kcsan_init ();

    /* Do the rest non-__init'ed, we're now alive */
    /* 后续切到非 __init 路径执行，此时内核已具备基本运行能力。 */
    rest_init ();

    /*
     * Avoid stack canaries in callers of boot_init_stack_canary for gcc-10
     * and older.
     *
     * 避免 gcc-10 及更早版本在 boot_init_stack_canary() 调用链上生成栈保护检查。
     */
#if !__has_attribute(__no_stack_protector__)
    prevent_tail_call_optimization ();
#endif
}

/* Call all constructor functions linked into the kernel. */
/* 调用链接进内核镜像的所有构造函数。 */
static void __init do_ctors (void)
{
/*
 * For UML, the constructors have already been called by the
 * normal setup code as it's just a normal ELF binary, so we
 * cannot do it again - but we do need CONFIG_CONSTRUCTORS
 * even on UML for modules.
 *
 * UML 作为普通 ELF 程序启动时已经由常规初始化路径调用过构造函数，
 * 因此这里不能重复调用；但模块支持仍需要 CONFIG_CONSTRUCTORS。
 */
#if defined(CONFIG_CONSTRUCTORS) && !defined(CONFIG_UML)
    ctor_fn_t* fn = (ctor_fn_t*)__ctors_start;

    for (; fn < (ctor_fn_t*)__ctors_end; fn++)
        (*fn) ();
#endif
}

#ifdef CONFIG_KALLSYMS
struct blacklist_entry {
    struct list_head next;
    char*            buf;
};

static __initdata_or_module LIST_HEAD (blacklisted_initcalls);

static int __init           initcall_blacklist (char* str)
{
    char*                   str_entry;
    struct blacklist_entry* entry;

    /* str argument is a comma-separated list of functions */
    /* str 参数是用逗号分隔的 initcall 函数名列表。 */
    do {
        str_entry = strsep (&str, ",");
        if (str_entry) {
            pr_debug ("blacklisting initcall %s\n", str_entry);
            entry = memblock_alloc (sizeof (*entry), SMP_CACHE_BYTES);
            if (!entry)
                panic ("%s: Failed to allocate %zu bytes\n", __func__, sizeof (*entry));
            entry->buf = memblock_alloc (strlen (str_entry) + 1, SMP_CACHE_BYTES);
            if (!entry->buf)
                panic ("%s: Failed to allocate %zu bytes\n", __func__, strlen (str_entry) + 1);
            strcpy (entry->buf, str_entry);
            list_add (&entry->next, &blacklisted_initcalls);
        }
    } while (str_entry);

    return 1;
}

static bool __init_or_module initcall_blacklisted (initcall_t fn)
{
    struct blacklist_entry* entry;
    char                    fn_name[KSYM_SYMBOL_LEN];
    unsigned long           addr;

    if (list_empty (&blacklisted_initcalls))
        return false;

    addr = (unsigned long)dereference_function_descriptor (fn);
    sprint_symbol_no_offset (fn_name, addr);

    /*
     * fn will be "function_name [module_name]" where [module_name] is not
     * displayed for built-in init functions.  Strip off the [module_name].
     *
     * 符号名可能形如 "function_name [module_name]"；内建 init 函数没有模块名。
     * 比较黑名单时只需要函数名，因此去掉模块名后缀。
     */
    strreplace (fn_name, ' ', '\0');

    list_for_each_entry (entry, &blacklisted_initcalls, next) {
        if (!strcmp (fn_name, entry->buf)) {
            pr_debug ("initcall %s blacklisted\n", fn_name);
            return true;
        }
    }

    return false;
}
#else
static int __init initcall_blacklist (char* str)
{
    pr_warn ("initcall_blacklist requires CONFIG_KALLSYMS\n");
    return 0;
}

static bool __init_or_module initcall_blacklisted (initcall_t fn)
{
    return false;
}
#endif
__setup ("initcall_blacklist=", initcall_blacklist);

static __init_or_module void trace_initcall_start_cb (void* data, initcall_t fn)
{
    ktime_t* calltime = data;

    printk (KERN_DEBUG "calling  %pS @ %i\n", fn, task_pid_nr (current));
    *calltime = ktime_get ();
}

static __init_or_module void trace_initcall_finish_cb (void* data, initcall_t fn, int ret)
{
    ktime_t rettime, *calltime = data;

    rettime = ktime_get ();
    printk (KERN_DEBUG "initcall %pS returned %d after %lld usecs\n", fn, ret, (unsigned long long)ktime_us_delta (rettime, *calltime));
}

static ktime_t initcall_calltime;

#ifdef TRACEPOINTS_ENABLED
static void __init initcall_debug_enable (void)
{
    int ret;

    ret = register_trace_initcall_start (trace_initcall_start_cb, &initcall_calltime);
    ret |= register_trace_initcall_finish (trace_initcall_finish_cb, &initcall_calltime);
    WARN (ret, "Failed to register initcall tracepoints\n");
}
#    define do_trace_initcall_start trace_initcall_start
#    define do_trace_initcall_finish trace_initcall_finish
#else
static inline void do_trace_initcall_start (initcall_t fn)
{
    if (!initcall_debug)
        return;
    trace_initcall_start_cb (&initcall_calltime, fn);
}
static inline void do_trace_initcall_finish (initcall_t fn, int ret)
{
    if (!initcall_debug)
        return;
    trace_initcall_finish_cb (&initcall_calltime, fn, ret);
}
#endif /* !TRACEPOINTS_ENABLED */

int __init_or_module do_one_initcall (initcall_t fn)
{
    int  count = preempt_count ();
    char msgbuf[64];
    int  ret;

    if (initcall_blacklisted (fn))
        return -EPERM;

    do_trace_initcall_start (fn);
    ret = fn ();
    do_trace_initcall_finish (fn, ret);

    msgbuf[0] = 0;

    if (preempt_count () != count) {
        sprintf (msgbuf, "preemption imbalance ");
        preempt_count_set (count);
    }
    if (irqs_disabled ()) {
        strlcat (msgbuf, "disabled interrupts ", sizeof (msgbuf));
        local_irq_enable ();
    }
    WARN (msgbuf[0], "initcall %pS returned with %s\n", fn, msgbuf);

    add_latent_entropy ();
    return ret;
}

static initcall_entry_t* initcall_levels[] __initdata = {
        __initcall0_start, __initcall1_start, __initcall2_start, __initcall3_start, __initcall4_start,
        __initcall5_start, __initcall6_start, __initcall7_start, __initcall_end,
};

/* Keep these in sync with initcalls in include/linux/init.h */
/* 这些级别名称必须与 include/linux/init.h 中的 initcall 分级保持一致。 */
static const char* initcall_level_names[] __initdata = {
        "pure", "core", "postcore", "arch", "subsys", "fs", "device", "late",
};

static int __init ignore_unknown_bootoption (char* param, char* val, const char* unused, void* arg)
{
    return 0;
}

static void __init do_initcall_level (int level, char* command_line)
{
    initcall_entry_t* fn;

    parse_args (initcall_level_names[level], command_line, __start___param, __stop___param - __start___param, level, level, NULL, ignore_unknown_bootoption);

    trace_initcall_level (initcall_level_names[level]);
    for (fn = initcall_levels[level]; fn < initcall_levels[level + 1]; fn++)
        do_one_initcall (initcall_from_entry (fn));
}

static void __init do_initcalls (void)
{
    int    level;
    size_t len = saved_command_line_len + 1;
    char*  command_line;

    command_line = kzalloc (len, GFP_KERNEL);
    if (!command_line)
        panic ("%s: Failed to allocate %zu bytes\n", __func__, len);

    for (level = 0; level < ARRAY_SIZE (initcall_levels) - 1; level++) {
        /* Parser modifies command_line, restore it each time */
        /* 参数解析器会修改 command_line，因此每个级别执行前都要恢复原始副本。 */
        strcpy (command_line, saved_command_line);
        do_initcall_level (level, command_line);
    }

    kfree (command_line);
}

/*
 * Ok, the machine is now initialized. None of the devices
 * have been touched yet, but the CPU subsystem is up and
 * running, and memory and process management works.
 *
 * Now we can finally start doing some real work..
 *
 * 至此机器基础环境已初始化完成。设备尚未真正探测，但 CPU 子系统、
 * 内存管理和进程管理已经可用，可以开始执行驱动和各子系统 initcall。
 */
static void __init do_basic_setup (void)
{
    cpuset_init_smp ();
    driver_init ();
    init_irq_proc ();
    do_ctors ();
    do_initcalls ();
}

static void __init do_pre_smp_initcalls (void)
{
    initcall_entry_t* fn;

    trace_initcall_level ("early");
    for (fn = __initcall_start; fn < __initcall0_start; fn++)
        do_one_initcall (initcall_from_entry (fn));
}

static int run_init_process (const char* init_filename)
{
    const char* const* p;

    // 将当前候选路径设置为 argv[0]，随后在内核线程中执行该用户态 init 程序
    argv_init[0] = init_filename;
    pr_info ("Run %s as init process\n", init_filename);
    pr_debug ("  with arguments:\n");
    for (p = argv_init; *p; p++)
        pr_debug ("    %s\n", *p);
    pr_debug ("  with environment:\n");
    for (p = envp_init; *p; p++)
        pr_debug ("    %s\n", *p);
    return kernel_execve (init_filename, argv_init, envp_init);
}

static int try_to_run_init_process (const char* init_filename)
{
    int ret;

    // 尝试启动指定 init；不存在时静默交给下一个候选路径，其它错误需要打印出来
    ret = run_init_process (init_filename);

    if (ret && ret != -ENOENT) {
        pr_err ("Starting init: %s exists but couldn't execute it (error %d)\n", init_filename, ret);
    }

    return ret;
}

static noinline void __init kernel_init_freeable (void);

#if defined(CONFIG_STRICT_KERNEL_RWX) || defined(CONFIG_STRICT_MODULE_RWX)
bool rodata_enabled __ro_after_init = true;

#    ifndef arch_parse_debug_rodata
static inline bool arch_parse_debug_rodata (char* str)
{
    return false;
}
#    endif

static int __init set_debug_rodata (char* str)
{
    if (arch_parse_debug_rodata (str))
        return 0;

    // rodata=on/off 控制严格内核只读映射保护是否启用
    if (str && !strcmp (str, "on"))
        rodata_enabled = true;
    else if (str && !strcmp (str, "off"))
        rodata_enabled = false;
    else
        pr_warn ("Invalid option string for rodata: '%s'\n", str);
    return 0;
}
early_param ("rodata", set_debug_rodata);
#endif

static void mark_readonly (void)
{
    if (IS_ENABLED (CONFIG_STRICT_KERNEL_RWX) && rodata_enabled) {
        /*
         * load_module() results in W+X mappings, which are cleaned
         * up with init_free_wq. Let's make sure that queued work is
         * flushed so that we don't hit false positives looking for
         * insecure pages which are W+X.
         *
         * 模块加载阶段可能临时产生 W+X 映射，这些映射会通过 init_free_wq 清理。
         * 在检查不安全页之前先刷新相关工作队列，避免误报。
         */
        // 完成 init 阶段模块内存释放相关工作，避免遗留临时可写可执行映射
        flush_module_init_free_work ();
        // 将 jump label 元数据切换到只读状态
        jump_label_init_ro ();
        // 把内核 rodata/text 等受保护区域设置为只读或只执行
        mark_rodata_ro ();
        // 检查是否仍存在同时可写可执行的内核映射
        debug_checkwx ();
        // 运行 rodata 保护自测，确认只读映射确实不可写
        rodata_test ();
    } else if (IS_ENABLED (CONFIG_STRICT_KERNEL_RWX)) {
        pr_info ("Kernel memory protection disabled.\n");
    } else if (IS_ENABLED (CONFIG_ARCH_HAS_STRICT_KERNEL_RWX)) {
        pr_warn ("Kernel memory protection not selected by kernel config.\n");
    } else {
        pr_warn ("This architecture does not have kernel memory protection.\n");
    }
}

void __weak free_initmem (void)
{
    // 释放标记为 __init 的代码和数据段，默认用毒化值填充便于发现误用
    free_initmem_default (POISON_FREE_INITMEM);
}

static int __ref kernel_init (void* unused)
{
    int ret;

    /*
     * Wait until kthreadd is all set-up.
     *
     * 等待 kthreadd 完成初始化，确保后续 init 路径可以安全创建内核线程。
     */
    wait_for_completion (&kthreadd_done);

    kernel_init_freeable ();
    /* need to finish all async __init code before freeing the memory */
    /* 释放 init 内存前必须等待所有异步 __init 代码执行完毕。 */
    async_synchronize_full ();

    // 进入释放 init 内存阶段，后续不能再依赖 __init 代码或数据
    system_state = SYSTEM_FREEING_INITMEM;
    // 释放 kprobe 在 init 段中的探测点和相关资源
    kprobe_free_init_mem ();
    // 释放 ftrace 在 init 段中的跟踪元数据
    ftrace_free_init_mem ();
    // 释放 kgdb 在 init 段中的调试资源
    kgdb_free_init_mem ();
    // 释放 bootconfig 解析树等启动配置临时资源
    exit_boot_config ();
    free_initmem ();
    mark_readonly ();

    /*
     * Kernel mappings are now finalized - update the userspace page-table
     * to finalize PTI.
     *
     * 内核映射已经最终确定，可以同步更新用户态页表并完成 PTI 设置。
     */
    pti_finalize ();

    // 从此系统进入正常运行阶段
    system_state = SYSTEM_RUNNING;
    // 恢复默认 NUMA 策略，清理启动阶段可能临时设置的策略状态
    numa_default_policy ();

    // 通知 RCU 内核启动阶段结束，切换到正常运行期语义
    rcu_end_inkernel_boot ();

    // 处理通过启动参数传入的 sysctl 设置
    do_sysctl_args ();

    if (ramdisk_execute_command) {
        // 优先尝试 initramfs/initrd 中的早期用户态 init，默认路径通常是 /init
        ret = run_init_process (ramdisk_execute_command);
        if (!ret)
            return 0;
        pr_err ("Failed to execute %s (error %d)\n", ramdisk_execute_command, ret);
    }

    /*
     * We try each of these until one succeeds.
     *
     * The Bourne shell can be used instead of init if we are
     * trying to recover a really broken machine.
     *
     * 依次尝试这些 init 路径，直到有一个成功。系统严重损坏时也允许
     * 退回到 Bourne shell，方便救援。
     */
    if (execute_command) {
        // 如果用户通过 init= 指定了路径，则它必须成功执行，否则直接 panic
        ret = run_init_process (execute_command);
        if (!ret)
            return 0;
        panic ("Requested init %s failed (error %d).", execute_command, ret);
    }

    if (CONFIG_DEFAULT_INIT[0] != '\0') {
        // 尝试 Kconfig 中配置的默认 init 路径
        ret = run_init_process (CONFIG_DEFAULT_INIT);
        if (ret)
            pr_err ("Default init %s failed (error %d)\n", CONFIG_DEFAULT_INIT, ret);
        else
            return 0;
    }

    if (!try_to_run_init_process ("/sbin/init") || !try_to_run_init_process ("/etc/init") || !try_to_run_init_process ("/bin/init")
        || !try_to_run_init_process ("/bin/sh"))
        return 0;

    panic ("No working init found.  Try passing init= option to kernel. "
           "See Linux Documentation/admin-guide/init.rst for guidance.");
}

/* Open /dev/console, for stdin/stdout/stderr, this should never fail */
/* 打开 /dev/console 并复制为标准输入、标准输出、标准错误，理论上不应失败。 */
void __init console_on_rootfs (void)
{
    struct file* file = filp_open ("/dev/console", O_RDWR, 0);

    if (IS_ERR (file)) {
        pr_err ("Warning: unable to open an initial console.\n");
        return;
    }
    init_dup (file);
    init_dup (file);
    init_dup (file);
    fput (file);
}

static noinline void __init kernel_init_freeable (void)
{
    /* Now the scheduler is fully set up and can do blocking allocations */
    /* 此时调度器已完整可用，允许执行可能阻塞的内存分配。 */
    gfp_allowed_mask = __GFP_BITS_MASK;

    /*
     * init can allocate pages on any node
     *
     * init 进程可以从任意内存节点分配页面。
     */
    set_mems_allowed (node_states[N_MEMORY]);

    // 记录 Ctrl-Alt-Del 等重启动作要通知的 init 进程 PID
    cad_pid = get_pid (task_pid (current));

    // 架构相关的从 CPU 准备工作，但尚未真正把它们全部拉起
    smp_prepare_cpus (setup_max_cpus);

    // 启动工作队列执行线程，使早期排队的 work item 可以开始运行
    workqueue_init ();

    // 初始化 init_mm 的内部字段，为后续内存管理路径补齐运行期状态
    init_mm_internals ();

    // 初始化 RCU Tasks 系列机制，用于跟踪任务上下文级别的宽限期
    rcu_init_tasks_generic ();
    // 执行 early initcall，它们需要在 SMP 完全启动前完成
    do_pre_smp_initcalls ();
    // 初始化 lockup detector，用于检测 CPU 长时间无响应等问题
    lockup_detector_init ();

    // 启动其余 CPU，系统从单启动 CPU 进入 SMP 运行状态
    smp_init ();
    // 完成调度器 SMP 拓扑初始化，启用完整的多 CPU 调度能力
    sched_init_smp ();

    // 根据 CPU 拓扑完善 workqueue 的亲和性和调度域信息
    workqueue_init_topology ();
    // 初始化异步调用框架，供后续子系统并行执行启动任务
    async_init ();
    // 初始化 padata 并行数据处理框架
    padata_init ();
    // 页面分配器后期初始化，启用依赖 SMP/热插拔等能力的部分
    page_alloc_init_late ();

    // 执行驱动模型、构造函数和各级 initcall，启动大部分内核子系统与设备
    do_basic_setup ();

    // 运行内建 KUnit 测试
    kunit_run_all_tests ();

    // 等待 initramfs 解包完成，确保早期用户态文件可见
    wait_for_initramfs ();
    // 在根文件系统上打开控制台，建立 init 的标准输入输出
    console_on_rootfs ();

    /*
     * check if there is an early userspace init.  If yes, let it do all
     * the work
     *
     * 检查是否存在早期用户态 init；如果存在，就让它负责后续挂载和初始化。
     */
    if (init_eaccess (ramdisk_execute_command) != 0) {
        // 没有早期 init 时，由内核准备真正的根文件系统命名空间
        ramdisk_execute_command = NULL;
        prepare_namespace ();
    }

    /*
     * Ok, we have completed the initial bootup, and
     * we're essentially up and running. Get rid of the
     * initmem segments and start the user-mode stuff..
     *
     * rootfs is available now, try loading the public keys
     * and default modules
     *
     * rootfs 现已可用，接下来尝试加载完整性校验公钥和默认模块。
     */

    // 加载完整性子系统需要的公钥，例如 IMA/EVM 使用的 keyring
    integrity_load_keys ();
}
