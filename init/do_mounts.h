/* SPDX-License-Identifier: GPL-2.0 */
#include <linux/kernel.h>
#include <linux/blkdev.h>
#include <linux/init.h>
#include <linux/syscalls.h>
#include <linux/unistd.h>
#include <linux/slab.h>
#include <linux/mount.h>
#include <linux/major.h>
#include <linux/root_dev.h>
#include <linux/init_syscalls.h>
#include <linux/task_work.h>
#include <linux/file.h>

/* 按给定设备名和显示名尝试通用根文件系统挂载，flags 为挂载标志。 */
void  mount_root_generic(char *name, char *pretty_name, int flags);
/* 根据 root= 等启动参数解析出的根设备名挂载最终根文件系统。 */
void  mount_root(char *root_device_name);
/* 根文件系统挂载标志，例如只读/读写，由启动参数和默认策略共同决定。 */
extern int root_mountflags;

static inline __init int create_dev(char *name, dev_t dev)
{
        /* 重新创建指定块设备节点，避免旧节点类型或设备号不匹配。 */
	init_unlink(name);
	return init_mknod(name, S_IFBLK | 0600, new_encode_dev(dev));
}

#ifdef CONFIG_BLK_DEV_RAM

/* 从指定 ramdisk 编号加载磁盘镜像。 */
int __init rd_load_disk(int n);
/* 从给定路径读取并加载 ramdisk 镜像。 */
int __init rd_load_image(char *from);

#else

/* 未启用 ramdisk 支持时，加载接口退化为空操作。 */
static inline int rd_load_disk(int n) { return 0; }
static inline int rd_load_image(char *from) { return 0; }

#endif

#ifdef CONFIG_BLK_DEV_INITRD
/* 尝试加载 initrd，并可能更新后续要挂载的根设备名。 */
bool __init initrd_load(char *root_device_name);
#else
static inline bool initrd_load(char *root_device_name)
{
        /* 未启用 initrd 支持时，不执行加载。 */
	return false;
	}

#endif

/* Ensure that async file closing finished to prevent spurious errors. */
/* 确保异步文件关闭已经完成，避免启动阶段后续操作看到虚假的文件错误。 */
static inline void init_flush_fput(void)
{
        /* 先刷新延迟 fput，再运行当前任务挂起的 task_work。 */
	flush_delayed_fput();
	task_work_run();
}
