// SPDX-License-Identifier: GPL-2.0
#include <linux/unistd.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/minix_fs.h>
#include <linux/romfs_fs.h>
#include <linux/initrd.h>
#include <linux/sched.h>
#include <linux/freezer.h>
#include <linux/kmod.h>
#include <uapi/linux/mount.h>

#include "do_mounts.h"

/* initrd 在内核虚拟地址空间中的起止地址。 */
unsigned long initrd_start, initrd_end;
/* 允许 initrd 位于内核起始地址以下，用于部分架构/启动器兼容。 */
int initrd_below_start_ok;
static unsigned int real_root_dev;	/* do_proc_dointvec cannot handle kdev_t */
/* proc_dointvec 不能直接处理 kdev_t，因此用编码后的 unsigned int 保存真实根设备。 */
/* 是否启用旧式 initrd 挂载流程；noinitrd 参数会关闭它。 */
static int __initdata mount_initrd = 1;

/* initrd 的物理起始地址和大小，早期参数解析阶段填充。 */
phys_addr_t phys_initrd_start __initdata;
unsigned long phys_initrd_size __initdata;

#ifdef CONFIG_SYSCTL
/* 向 /proc/sys/kernel/real-root-dev 暴露真实根设备号，供 linuxrc 修改。 */
static struct ctl_table kern_do_mounts_initrd_table[] = {
	{
		.procname       = "real-root-dev",
		.data           = &real_root_dev,
		.maxlen         = sizeof(int),
		.mode           = 0644,
		.proc_handler   = proc_dointvec,
	},
};

static __init int kernel_do_mounts_initrd_sysctls_init(void)
{
        /* late_initcall 阶段注册 real-root-dev sysctl。 */
	register_sysctl_init("kernel", kern_do_mounts_initrd_table);
	return 0;
}
late_initcall(kernel_do_mounts_initrd_sysctls_init);
#endif /* CONFIG_SYSCTL */
/* real-root-dev sysctl 支持结束。 */

static int __init no_initrd(char *str)
{
        /* noinitrd 表示不执行旧式 initrd 流程。 */
	mount_initrd = 0;
	return 1;
}

__setup("noinitrd", no_initrd);

static int __init early_initrdmem(char *p)
{
	phys_addr_t start;
	unsigned long size;
	char *endp;

        /* initrdmem=<start>,<size> 显式指定 initrd 物理地址范围。 */
	start = memparse(p, &endp);
	if (*endp == ',') {
		size = memparse(endp + 1, NULL);

		phys_initrd_start = start;
		phys_initrd_size = size;
	}
	return 0;
}
early_param("initrdmem", early_initrdmem);

static int __init early_initrd(char *p)
{
        /* initrd= 与 initrdmem= 使用同一套早期解析逻辑。 */
	return early_initrdmem(p);
}
early_param("initrd", early_initrd);

static int __init init_linuxrc(struct subprocess_info *info, struct cred *new)
{
        /* linuxrc 在独立的 fs/files 上下文中运行，避免影响内核当前上下文。 */
	ksys_unshare(CLONE_FS | CLONE_FILES);
	console_on_rootfs();
	/* move initrd over / and chdir/chroot in initrd root */
        /* 将 initrd 挂载点移动到 /，并把当前根目录切换到 initrd 根。 */
	init_chdir("/root");
	init_mount(".", "/", NULL, MS_MOVE, NULL);
	init_chroot(".");
        /* 为 linuxrc 建立新的会话。 */
	ksys_setsid();
	return 0;
}

static void __init handle_initrd(char *root_device_name)
{
	struct subprocess_info *info;
	static char *argv[] = { "linuxrc", NULL, };
	extern char *envp_init[];
	int error;

        /* 旧式 initrd 依赖 /linuxrc，保留兼容但已经废弃。 */
	pr_warn("using deprecated initrd support, will be removed in 2021.\n");

        /* 保存真实根设备，linuxrc 可以通过 real-root-dev sysctl 修改它。 */
	real_root_dev = new_encode_dev(ROOT_DEV);
	create_dev("/dev/root.old", Root_RAM0);
	/* mount initrd on rootfs' /root */
        /* 先把 /dev/ram0 中的 initrd 挂载到 rootfs 的 /root。 */
	mount_root_generic("/dev/root.old", root_device_name,
			   root_mountflags & ~MS_RDONLY);
	init_mkdir("/old", 0700);
	init_chdir("/old");

        /* 在 initrd 根中执行 /linuxrc，让它准备真实根设备。 */
	info = call_usermodehelper_setup("/linuxrc", argv, envp_init,
					 GFP_KERNEL, init_linuxrc, NULL, NULL);
	if (!info)
		return;
	call_usermodehelper_exec(info, UMH_WAIT_PROC|UMH_FREEZABLE);

	/* move initrd to rootfs' /old */
        /* linuxrc 返回后，将 initrd 根移动到 rootfs 的 /old。 */
	init_mount("..", ".", NULL, MS_MOVE, NULL);
	/* switch root and cwd back to / of rootfs */
        /* 把根目录和当前目录切回 rootfs 的 /。 */
	init_chroot("..");

	if (new_decode_dev(real_root_dev) == Root_RAM0) {
                /* linuxrc 指定真实根仍是 ram0 时，保留在 /old 中继续运行。 */
		init_chdir("/old");
		return;
	}

        /* 根据 linuxrc 最终指定的真实根设备挂载真正根文件系统。 */
	init_chdir("/");
	ROOT_DEV = new_decode_dev(real_root_dev);
	mount_root(root_device_name);

        /* 尝试把旧 initrd 根移动到新根的 /initrd，供用户态继续访问。 */
	printk(KERN_NOTICE "Trying to move old root to /initrd ... ");
	error = init_mount("/old", "/root/initrd", NULL, MS_MOVE, NULL);
	if (!error)
		printk("okay\n");
	else {
		if (error == -ENOENT)
			printk("/initrd does not exist. Ignored.\n");
		else
			printk("failed\n");
		printk(KERN_NOTICE "Unmounting old root\n");
                /* 如果无法移动旧根，则懒卸载 /old。 */
		init_umount("/old", MNT_DETACH);
	}
}

bool __init initrd_load(char *root_device_name)
{
        /* 仅在启用旧式 initrd 支持时尝试把 /initrd.image 加载进 /dev/ram0。 */
	if (mount_initrd) {
		create_dev("/dev/ram", Root_RAM0);
		/*
		 * Load the initrd data into /dev/ram0. Execute it as initrd
		 * unless /dev/ram0 is supposed to be our actual root device,
		 * in that case the ram disk is just set up here, and gets
		 * mounted in the normal path.
                 *
                 * 将 initrd 数据加载到 /dev/ram0。除非 /dev/ram0 本身就是
                 * 最终真实根设备，否则按旧式 initrd 执行 /linuxrc 流程；
                 * 如果 ram0 就是真实根，则只准备 ramdisk，后续走普通挂载路径。
		 */
		if (rd_load_image("/initrd.image") && ROOT_DEV != Root_RAM0) {
                        /* 镜像已加载进 ram0，临时文件不再需要。 */
			init_unlink("/initrd.image");
			handle_initrd(root_device_name);
			return true;
		}
	}
        /* 未接管根挂载流程时，清理 initrd.image 并让普通根挂载继续。 */
	init_unlink("/initrd.image");
	return false;
}
