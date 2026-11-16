// SPDX-License-Identifier: GPL-2.0-only
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/ctype.h>
#include <linux/fd.h>
#include <linux/tty.h>
#include <linux/suspend.h>
#include <linux/root_dev.h>
#include <linux/security.h>
#include <linux/delay.h>
#include <linux/mount.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/initrd.h>
#include <linux/async.h>
#include <linux/fs_struct.h>
#include <linux/slab.h>
#include <linux/ramfs.h>
#include <linux/shmem_fs.h>
#include <linux/ktime.h>

#include <linux/nfs_fs.h>
#include <linux/nfs_fs_sb.h>
#include <linux/nfs_mount.h>
#include <linux/raid/detect.h>
#include <uapi/linux/mount.h>

#include "do_mounts.h"

int                    root_mountflags = MS_RDONLY | MS_SILENT;
/* 保存 root= 启动参数传入的根设备名。 */
static char __initdata saved_root_name[64];
/* rootwait 等待策略: 0 表示不等待，-1 表示无限等待，正数表示毫秒超时。 */
static int             root_wait;

/* 最终解析出的根设备号；特殊取值也可表示 NFS、CIFS、通用根等伪设备。 */
dev_t                  ROOT_DEV;

static int __init      load_ramdisk (char* str)
{
    /* load_ramdisk= 已废弃，仅保留兼容提示。 */
    pr_warn ("ignoring the deprecated load_ramdisk= option\n");
    return 1;
}
__setup ("load_ramdisk=", load_ramdisk);

static int __init readonly (char* str)
{
    /* ro 不带参数时，将根文件系统按只读方式挂载。 */
    if (*str)
        return 0;
    root_mountflags |= MS_RDONLY;
    return 1;
}

static int __init readwrite (char* str)
{
    /* rw 不带参数时，清除只读标志，尝试读写挂载根文件系统。 */
    if (*str)
        return 0;
    root_mountflags &= ~MS_RDONLY;
    return 1;
}

__setup ("ro", readonly);
__setup ("rw", readwrite);

static int __init root_dev_setup (char* line)
{
    /* 保存 root= 的原始字符串，稍后再解析为特殊根类型或块设备号。 */
    strscpy (saved_root_name, line, sizeof (saved_root_name));
    return 1;
}

__setup ("root=", root_dev_setup);

static int __init rootwait_setup (char* str)
{
    /* rootwait 不带参数表示无限等待根设备出现。 */
    if (*str)
        return 0;
    root_wait = -1;
    return 1;
}

__setup ("rootwait", rootwait_setup);

static int __init rootwait_timeout_setup (char* str)
{
    int sec;

    /* rootwait=<sec> 指定等待根设备出现的最大秒数。 */
    if (kstrtoint (str, 0, &sec) || sec < 0) {
        pr_warn ("ignoring invalid rootwait value\n");
        goto ignore;
    }

    if (check_mul_overflow (sec, MSEC_PER_SEC, &root_wait)) {
        pr_warn ("ignoring excessive rootwait value\n");
        goto ignore;
    }

    return 1;

ignore:
    /* Fallback to indefinite wait */
    /* 非法或过大的 rootwait= 参数退回为无限等待。 */
    root_wait = -1;

    return 1;
}

__setup ("rootwait=", rootwait_timeout_setup);

static char* __initdata root_mount_data;
static int __init       root_data_setup (char* str)
{
    /* rootflags= 作为挂载数据传递给具体文件系统。 */
    root_mount_data = str;
    return 1;
}

static char* __initdata root_fs_names;
static int __init       fs_names_setup (char* str)
{
    /* rootfstype= 指定候选根文件系统类型列表，多个类型用逗号分隔。 */
    root_fs_names = str;
    return 1;
}

static unsigned int __initdata root_delay;
static int __init              root_delay_setup (char* str)
{
    /* rootdelay= 指定挂载根设备前固定等待的秒数。 */
    root_delay = simple_strtoul (str, NULL, 0);
    return 1;
}

__setup ("rootflags=", root_data_setup);
__setup ("rootfstype=", fs_names_setup);
__setup ("rootdelay=", root_delay_setup);

/* This can return zero length strings. Caller should check */
/* 将 rootfstype= 的逗号分隔列表拆成 NUL 分隔字符串；可能产生空字符串，调用者需检查。 */
static int __init split_fs_names (char* page, size_t size)
{
    int   count = 1;
    char* p     = page;

    strscpy (p, root_fs_names, size);
    while (*p++) {
        if (p[-1] == ',') {
            p[-1] = '\0';
            count++;
        }
    }

    return count;
}

static int __init do_mount_root (const char* name, const char* fs, const int flags, const void* data)
{
    struct super_block* s;
    struct page*        p         = NULL;
    char*               data_page = NULL;
    int                 ret;

    if (data) {
        /* init_mount() requires a full page as fifth argument */
        /* init_mount() 要求第五个参数指向完整页，因此先复制挂载数据到页缓冲区。 */
        p = alloc_page (GFP_KERNEL);
        if (!p)
            return -ENOMEM;
        data_page = page_address (p);
        strscpy_pad (data_page, data, PAGE_SIZE);
    }

    ret = init_mount (name, "/root", fs, flags, data_page);
    if (ret)
        goto out;

    /* 挂载成功后切换到 /root，并记录实际根设备号用于日志和后续逻辑。 */
    init_chdir ("/root");
    s        = current->fs->pwd.dentry->d_sb;
    ROOT_DEV = s->s_dev;
    printk (KERN_INFO "VFS: Mounted root (%s filesystem)%s on device %u:%u.\n", s->s_type->name, sb_rdonly (s) ? " readonly" : "", MAJOR (ROOT_DEV),
            MINOR (ROOT_DEV));

out:
    if (p)
        put_page (p);
    return ret;
}

void __init mount_root_generic (char* name, char* pretty_name, int flags)
{
    struct page* page     = alloc_page (GFP_KERNEL);
    char*        fs_names = page_address (page);
    char*        p;
    char         b[BDEVNAME_SIZE];
    int          num_fs, i;

    /* 为日志准备 unknown-block(major,minor) 形式的根设备描述。 */
    scnprintf (b, BDEVNAME_SIZE, "unknown-block(%u,%u)", MAJOR (ROOT_DEV), MINOR (ROOT_DEV));
    if (root_fs_names)
        num_fs = split_fs_names (fs_names, PAGE_SIZE);
    else
        num_fs = list_bdev_fs_names (fs_names, PAGE_SIZE);
retry:
    /* 依次尝试 rootfstype= 指定类型，或内核已注册的块设备文件系统类型。 */
    for (i = 0, p = fs_names; i < num_fs; i++, p += strlen (p) + 1) {
        int err;

        if (!*p)
            continue;
        err = do_mount_root (name, p, flags, root_mount_data);
        switch (err) {
        case 0: goto out;
        case -EACCES:
        case -EINVAL:
#ifdef CONFIG_BLOCK
            init_flush_fput ();
#endif
            continue;
        }
        /*
		 * Allow the user to distinguish between failed sys_open
		 * and bad superblock on root device.
		 * and give them a list of the available devices
                 *
                 * 这里打印可用分区和文件系统列表，帮助区分是根设备无法打开，
                 * 还是设备可打开但超级块/文件系统类型不匹配。
		 */
        printk ("VFS: Cannot open root device \"%s\" or %s: error %d\n", pretty_name, b, err);
        printk ("Please append a correct \"root=\" boot option; here are the available partitions:\n");
        printk_all_partitions ();

        if (root_fs_names)
            num_fs = list_bdev_fs_names (fs_names, PAGE_SIZE);
        if (!num_fs)
            pr_err ("Can't find any bdev filesystem to be used for mount!\n");
        else {
            pr_err ("List of all bdev filesystems:\n");
            for (i = 0, p = fs_names; i < num_fs; i++, p += strlen (p) + 1)
                pr_err (" %s", p);
            pr_err ("\n");
        }

        panic ("VFS: Unable to mount root fs on %s", b);
    }
    if (!(flags & SB_RDONLY)) {
        /* 读写挂载失败后，再退回只读方式重试一次。 */
        flags |= SB_RDONLY;
        goto retry;
    }

    printk ("List of all partitions:\n");
    printk_all_partitions ();
    printk ("No filesystem could mount root, tried: ");
    for (i = 0, p = fs_names; i < num_fs; i++, p += strlen (p) + 1)
        printk (" %s", p);
    printk ("\n");
    panic ("VFS: Unable to mount root fs on \"%s\" or %s", pretty_name, b);
out:
    put_page (page);
}

#ifdef CONFIG_ROOT_NFS

#    define NFSROOT_TIMEOUT_MIN 5
#    define NFSROOT_TIMEOUT_MAX 30
#    define NFSROOT_RETRY_MAX 5

static void __init mount_nfs_root (void)
{
    char *       root_dev, *root_data;
    unsigned int timeout;
    int          try;

    if (nfs_root_data (&root_dev, &root_data))
        goto fail;

    /*
	 * The server or network may not be ready, so try several
	 * times.  Stop after a few tries in case the client wants
	 * to fall back to other boot methods.
         *
         * NFS 服务器或网络可能尚未就绪，因此按指数退避重试几次。
         * 重试次数有限，避免阻塞其它可能的启动方式。
	 */
    timeout = NFSROOT_TIMEOUT_MIN;
    for (try = 1;; try++) {
        if (!do_mount_root (root_dev, "nfs", root_mountflags, root_data))
            return;
        if (try > NFSROOT_RETRY_MAX)
            break;

        /* Wait, in case the server refused us immediately */
        /* 如果服务器立即拒绝，等待一段时间后再重试。 */
        ssleep (timeout);
        timeout <<= 1;
        if (timeout > NFSROOT_TIMEOUT_MAX)
            timeout = NFSROOT_TIMEOUT_MAX;
    }
fail:
    pr_err ("VFS: Unable to mount root fs via NFS.\n");
}
#else
static inline void mount_nfs_root (void)
{
}
#endif /* CONFIG_ROOT_NFS */
/* NFS 根文件系统挂载支持结束。 */

#ifdef CONFIG_CIFS_ROOT

#    define CIFSROOT_TIMEOUT_MIN 5
#    define CIFSROOT_TIMEOUT_MAX 30
#    define CIFSROOT_RETRY_MAX 5

static void __init mount_cifs_root (void)
{
    char *       root_dev, *root_data;
    unsigned int timeout;
    int          try;

    if (cifs_root_data (&root_dev, &root_data))
        goto fail;

    /* SMB/CIFS 根文件系统同样按指数退避等待服务器或网络就绪。 */
    timeout = CIFSROOT_TIMEOUT_MIN;
    for (try = 1;; try++) {
        if (!do_mount_root (root_dev, "cifs", root_mountflags, root_data))
            return;
        if (try > CIFSROOT_RETRY_MAX)
            break;

        ssleep (timeout);
        timeout <<= 1;
        if (timeout > CIFSROOT_TIMEOUT_MAX)
            timeout = CIFSROOT_TIMEOUT_MAX;
    }
fail:
    pr_err ("VFS: Unable to mount root fs via SMB.\n");
}
#else
static inline void mount_cifs_root (void)
{
}
#endif /* CONFIG_CIFS_ROOT */
/* CIFS/SMB 根文件系统挂载支持结束。 */

static bool __init fs_is_nodev (char* fstype)
{
    struct file_system_type* fs  = get_fs_type (fstype);
    bool                     ret = false;

    /* nodev 文件系统不需要块设备，例如 tmpfs、ramfs 等。 */
    if (fs) {
        ret = !(fs->fs_flags & FS_REQUIRES_DEV);
        put_filesystem (fs);
    }

    return ret;
}

static int __init mount_nodev_root (char* root_device_name)
{
    char *fs_names, *fstype;
    int   err = -EINVAL;
    int   num_fs, i;

    fs_names = (void*)__get_free_page (GFP_KERNEL);
    if (!fs_names)
        return -EINVAL;
    num_fs = split_fs_names (fs_names, PAGE_SIZE);

    /* root= 给出路径且 rootfstype= 指定 nodev 文件系统时，直接尝试按该类型挂载。 */
    for (i = 0, fstype = fs_names; i < num_fs; i++, fstype += strlen (fstype) + 1) {
        if (!*fstype)
            continue;
        if (!fs_is_nodev (fstype))
            continue;
        err = do_mount_root (root_device_name, fstype, root_mountflags, root_mount_data);
        if (!err)
            break;
    }

    free_page ((unsigned long)fs_names);
    return err;
}

#ifdef CONFIG_BLOCK
static void __init mount_block_root (char* root_device_name)
{
    int err = create_dev ("/dev/root", ROOT_DEV);

    /* 为解析出的块设备号创建 /dev/root，然后按候选文件系统类型挂载。 */
    if (err < 0)
        pr_emerg ("Failed to create /dev/root: %d\n", err);
    mount_root_generic ("/dev/root", root_device_name, root_mountflags);
}
#else
static inline void mount_block_root (char* root_device_name)
{
}
#endif /* CONFIG_BLOCK */
/* 块设备根文件系统挂载支持结束。 */

void __init mount_root (char* root_device_name)
{
    /* 根据 ROOT_DEV 的特殊取值选择网络根、通用根、nodev 根或块设备根。 */
    switch (ROOT_DEV) {
    case Root_NFS: mount_nfs_root (); break;
    case Root_CIFS: mount_cifs_root (); break;
    case Root_Generic: mount_root_generic (root_device_name, root_device_name, root_mountflags); break;
    case 0:
        if (root_device_name && root_fs_names && mount_nodev_root (root_device_name) == 0)
            break;
        fallthrough;
    default: mount_block_root (root_device_name); break;
    }
}

/* wait for any asynchronous scanning to complete */
/* 等待异步设备扫描完成，并在 rootwait 策略允许时等待根设备出现。 */
static void __init wait_for_root (char* root_device_name)
{
    ktime_t end;

    if (ROOT_DEV != 0)
        return;

    /* ROOT_DEV 尚未解析出设备号时，循环等待驱动探测和块设备查找完成。 */
    pr_info ("Waiting for root device %s...\n", root_device_name);

    end = ktime_add_ms (ktime_get_raw (), root_wait);

    while (!driver_probe_done () || early_lookup_bdev (root_device_name, &ROOT_DEV) < 0) {
        msleep (5);
        /* rootwait=<sec> 达到超时时间后停止等待；rootwait 则不会进入该分支。 */
        if (root_wait > 0 && ktime_after (ktime_get_raw (), end))
            break;
    }

    async_synchronize_full ();
}

static dev_t __init parse_root_device (char* root_device_name)
{
    int   error;
    dev_t dev;

    /* mtd/ubi 根通常由对应文件系统自行解释 root= 字符串，不要求块设备号。 */
    if (!strncmp (root_device_name, "mtd", 3) || !strncmp (root_device_name, "ubi", 3))
        return Root_Generic;
    /* 网络根文件系统使用特殊 ROOT_DEV 标记。 */
    if (strcmp (root_device_name, "/dev/nfs") == 0)
        return Root_NFS;
    if (strcmp (root_device_name, "/dev/cifs") == 0)
        return Root_CIFS;
    if (strcmp (root_device_name, "/dev/ram") == 0)
        return Root_RAM0;

    /* 常规块设备路径或 major:minor 等形式在早期块设备表中解析。 */
    error = early_lookup_bdev (root_device_name, &dev);
    if (error) {
        if (error == -EINVAL && root_wait) {
            pr_err ("Disabling rootwait; root= is invalid.\n");
            root_wait = 0;
        }
        return 0;
    }
    return dev;
}

/*
 * Prepare the namespace - decide what/where to mount, load ramdisks, etc.
 *
 * 准备初始挂载命名空间: 根据启动参数决定挂载哪个根、加载 initrd/ramdisk，
 * 最后把临时 /root 移动为真正的 / 并 chroot 到其中。
 */
void __init prepare_namespace (void)
{
    if (root_delay) {
        /* rootdelay= 是无条件固定延迟，常用于等待慢速存储设备。 */
        printk (KERN_INFO "Waiting %d sec before mounting root device...\n", root_delay);
        ssleep (root_delay);
    }

    /*
	 * wait for the known devices to complete their probing
	 *
	 * Note: this is a potential source of long boot delays.
	 * For example, it is not atypical to wait 5 seconds here
	 * for the touchpad of a laptop to initialize.
         *
         * 等待已知设备完成探测。这可能带来较长启动延迟，例如笔记本触摸板
         * 初始化时在这里等待数秒并不罕见。
	 */
    wait_for_device_probe ();

    /* 启动早期处理 md/raid 根设备设置。 */
    md_run_setup ();

    if (saved_root_name[0])
        ROOT_DEV = parse_root_device (saved_root_name);

    /* initrd 如果成功接管根挂载流程，则直接进入收尾阶段。 */
    if (initrd_load (saved_root_name))
        goto out;

    if (root_wait)
        wait_for_root (saved_root_name);
    mount_root (saved_root_name);
out:
    /* 挂载 devtmpfs 后，把 /root 移动到 /，并切换进新的根目录。 */
    devtmpfs_mount ();
    init_mount (".", "/", NULL, MS_MOVE, NULL);
    init_chroot (".");
}

static bool is_tmpfs;
static int  rootfs_init_fs_context (struct fs_context* fc)
{
    /* rootfs 可由 tmpfs 或 ramfs 承载；启用 tmpfs 且满足条件时优先使用 tmpfs。 */
    if (IS_ENABLED (CONFIG_TMPFS) && is_tmpfs)
        return shmem_init_fs_context (fc);

    return ramfs_init_fs_context (fc);
}

struct file_system_type rootfs_fs_type = {
        .name            = "rootfs",
        .init_fs_context = rootfs_init_fs_context,
        .kill_sb         = kill_litter_super,
};

void __init init_rootfs (void)
{
    // 一般不会有 TMPFS 未配置的情况
    // 未显式指定 root/rootfstype 时，rootfs 默认使用 tmpfs；若 rootfstype 包含 tmpfs 也使用 tmpfs。
    if (IS_ENABLED (CONFIG_TMPFS)) {
        if (!saved_root_name[0] && !root_fs_names)
            is_tmpfs = true;
        else if (root_fs_names && !!strstr (root_fs_names, "tmpfs"))
            is_tmpfs = true;
    }
}
