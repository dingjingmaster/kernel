/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Filesystem superblock creation and reconfiguration context.
 *
 * Copyright (C) 2018 Red Hat, Inc. All Rights Reserved.
 * Written by David Howells (dhowells@redhat.com)
 */

#ifndef _LINUX_FS_CONTEXT_H
#define _LINUX_FS_CONTEXT_H

#include <linux/kernel.h>
#include <linux/refcount.h>
#include <linux/errno.h>
#include <linux/security.h>
#include <linux/mutex.h>

struct cred;
struct dentry;
struct file_operations;
struct file_system_type;
struct mnt_namespace;
struct net;
struct pid_namespace;
struct super_block;
struct user_namespace;
struct vfsmount;
struct path;

enum fs_context_purpose {
	FS_CONTEXT_FOR_MOUNT,		/* New superblock for explicit mount */
	FS_CONTEXT_FOR_SUBMOUNT,	/* New superblock for automatic submount */
	FS_CONTEXT_FOR_RECONFIGURE,	/* Superblock reconfiguration (remount) */
};

/*
 * Userspace usage phase for fsopen/fspick.
 */
enum fs_context_phase {
	FS_CONTEXT_CREATE_PARAMS,	/* Loading params for sb creation */
	FS_CONTEXT_CREATING,		/* A superblock is being created */
	FS_CONTEXT_AWAITING_MOUNT,	/* Superblock created, awaiting fsmount() */
	FS_CONTEXT_AWAITING_RECONF,	/* Awaiting initialisation for reconfiguration */
	FS_CONTEXT_RECONF_PARAMS,	/* Loading params for reconfiguration */
	FS_CONTEXT_RECONFIGURING,	/* Reconfiguring the superblock */
	FS_CONTEXT_FAILED,		/* Failed to correctly transition a context */
};

/*
 * Type of parameter value.
 */
enum fs_value_type {
	fs_value_is_undefined,
	fs_value_is_flag,		/* Value not given a value */
	fs_value_is_string,		/* Value is a string */
	fs_value_is_blob,		/* Value is a binary blob */
	fs_value_is_filename,		/* Value is a filename* + dirfd */
	fs_value_is_file,		/* Value is a file* */
};

/*
 * Configuration parameter.
 */
struct fs_parameter {
	const char		*key;		/* Parameter name */
	enum fs_value_type	type:8;		/* The type of value here */
	union {
		char		*string;
		void		*blob;
		struct filename	*name;
		struct file	*file;
	};
	size_t	size;
	int	dirfd;
};

struct p_log {
	const char *prefix;
	struct fc_log *log;
};

/**
 * @brief 
 * Filesystem context for holding the parameters used in the creation or
 * reconfiguration of a superblock.
 *
 * Superblock creation fills in ->root whereas reconfiguration begins with this
 * already set.
 *
 * See Documentation/filesystems/mount_api.rst
 *
 * struct fs_context 是虚拟文件系统(VFS)层的核心数据结构, 被形象地称为"挂载上下文"
 * 它是现代 Linux 挂载 API（fsopen / fspick 体系）的灵魂，取代了过去简单但扩展性差的参数传递方式
 * - 设计目标: 实现"配置"与"执行"的分离
 * - 生命周期: 由 fsopen() 创建, 在参数设置完成后由 fsmount() 转换为真正的挂载点, 最后被销毁
 */
struct fs_context {
	// 指向该文件系统特定的解析和挂载函数
	const struct fs_context_operations *ops;
	struct mutex		uapi_mutex;	/* Userspace access mutex */

	// 文件系统类型
	struct file_system_type	*fs_type;

	// 私有数据: 文件系统自己的配置结构体(如 Ext4 的超级块选项)
	void			*fs_private;	/* The filesystem's context */
	void			*sget_key;

	// 生成的根节点: get_tree 成功后, 该字段指向该文件系统的根目录
	struct dentry		*root;		/* The root and superblock */

	// 用户命名空间: 用于验证当前用户是否有权挂载该设备
	struct user_namespace	*user_ns;	/* The user namespace for this mount */
	struct net		*net_ns;	/* The network namespace for this mount */
	const struct cred	*cred;		/* The mounter's credentials */

	// 错误日志记录器: 通过 errorf() 等宏，将详细的挂载失败原因传回给用户态
	struct p_log		log;		/* Logging buffer */

	// 数据源: 即挂载源(如 /dev/sda1)
	const char		*source;	/* The source name (eg. dev path) */
	void			*security;	/* LSM options */
	void			*s_fs_info;	/* Proposed s_fs_info */
	unsigned int		sb_flags;	/* Proposed superblock flags (SB_*) */
	unsigned int		sb_flags_mask;	/* Superblock flags that were changed */
	unsigned int		s_iflags;	/* OR'd with sb->s_iflags */
	enum fs_context_purpose	purpose:8;
	enum fs_context_phase	phase:8;	/* The phase the context is in */
	bool			need_free:1;	/* Need to call ops->free() */
	bool			global:1;	/* Goes into &init_user_ns */
	bool			oldapi:1;	/* Coming from mount(2) */
	bool			exclusive:1;    /* create new superblock, reject existing one */
};

/**
 * @brief 
 * 全新文件系统挂载 API（fs_context 机制）的核心
 * 
 * 主要功能:
 * 1. 参数解析: 将用户态传来的字符串(如 rw, relatime, size=1G)转换为内核内部的变量.
 * 2. 状态校验: 在真正读取磁盘前, 检查配置参数是否合法且冲突.
 * 3. 获取树(Get Tree): 真正触发磁盘 I/O 或内存分配, 构建根目录的 dentry
 */
struct fs_context_operations {
	// 	清理：如果挂载失败或上下文销毁，释放分配的临时资源
	void (*free)(struct fs_context *fc);

	// 负责深拷贝（Deep Copy）当前的挂载上下文结构体
	// 多阶段挂载: 在现代 VFS 挂载流程中, 内核可能需要保存一份当前配置的副本.
	//           例如: 在尝试复杂的挂载操作时, 如果一种配置失败, 内核可以利用备份的副本快速重试另一种配置
	// 资源复制: 它不仅仅是内存拷贝, 还需要正确处理引用计数(如增加特定私有数据的引用), 
	//           确保两个独立的 fs_context 实例不会因为共享同一个指针而导致提前释放(Use-After-Free)
	int (*dup)(struct fs_context *fc, struct fs_context *src_fc);

	// 参数解析器：每当用户指定一个选项（如 -o nodev）时，该函数被调用。它取代了复杂的字符串拆解逻辑。
	int (*parse_param)(struct fs_context *fc, struct fs_parameter *param);

	// parse_monolithic 负责处理旧式(Legacy)的, 未经解析的原始挂载选项字符串
	// 向后兼容性: 在引入 fs_context 之前, 用户态通过 mount(2) 系统调用的最后一个参数传递一整串以逗号分隔的选项
	//            (例如 "rw,relatime,iocharset=utf8")
	// 现代流程: 通常使用 .parse_param 逐个键值对地处理参数
	// Monolithic 流程: 如果用户态直接调用了旧的 mount 系统调用, 或者文件系统需要处理不符合 key=value 格式的复杂原始数据, 
	//                 内核会调用此函数将整串字符一次性交给文件系统处理
	// 如果文件系统没有定义此函数, 内核通常会提供一个默认实现(generic_parse_monolithic), 
	// 它会自动将长字符串拆分为单个参数并反复调用 .parse_param
	int (*parse_monolithic)(struct fs_context *fc, void *data);

	// 获取根节点：执行挂载的核心。它负责填充 fc->root。如果没这个，挂载就无法产生目录
	int (*get_tree)(struct fs_context *fc);

	// 重新配置：处理 remount（重新挂载）操作，允许在不卸载的情况下修改挂载选项
	int (*reconfigure)(struct fs_context *fc);
};

/*
 * fs_context manipulation functions.
 */
extern struct fs_context *fs_context_for_mount(struct file_system_type *fs_type,
						unsigned int sb_flags);
extern struct fs_context *fs_context_for_reconfigure(struct dentry *dentry,
						unsigned int sb_flags,
						unsigned int sb_flags_mask);
extern struct fs_context *fs_context_for_submount(struct file_system_type *fs_type,
						struct dentry *reference);

extern struct fs_context *vfs_dup_fs_context(struct fs_context *fc);
extern int vfs_parse_fs_param(struct fs_context *fc, struct fs_parameter *param);
extern int vfs_parse_fs_string(struct fs_context *fc, const char *key,
			       const char *value, size_t v_size);
int vfs_parse_monolithic_sep(struct fs_context *fc, void *data,
			     char *(*sep)(char **));
extern int generic_parse_monolithic(struct fs_context *fc, void *data);
extern int vfs_get_tree(struct fs_context *fc);
extern void put_fs_context(struct fs_context *fc);
extern int vfs_parse_fs_param_source(struct fs_context *fc,
				     struct fs_parameter *param);
extern void fc_drop_locked(struct fs_context *fc);
int reconfigure_single(struct super_block *s,
		       int flags, void *data);

extern int get_tree_nodev(struct fs_context *fc,
			 int (*fill_super)(struct super_block *sb,
					   struct fs_context *fc));
extern int get_tree_single(struct fs_context *fc,
			 int (*fill_super)(struct super_block *sb,
					   struct fs_context *fc));
extern int get_tree_keyed(struct fs_context *fc,
			 int (*fill_super)(struct super_block *sb,
					   struct fs_context *fc),
			 void *key);

int setup_bdev_super(struct super_block *sb, int sb_flags,
		struct fs_context *fc);

#define GET_TREE_BDEV_QUIET_LOOKUP		0x0001
int get_tree_bdev_flags(struct fs_context *fc,
		int (*fill_super)(struct super_block *sb,
				  struct fs_context *fc), unsigned int flags);

extern int get_tree_bdev(struct fs_context *fc,
			       int (*fill_super)(struct super_block *sb,
						 struct fs_context *fc));

extern const struct file_operations fscontext_fops;

/*
 * Mount error, warning and informational message logging.  This structure is
 * shareable between a mount and a subordinate mount.
 */
struct fc_log {
	refcount_t	usage;
	u8		head;		/* Insertion index in buffer[] */
	u8		tail;		/* Removal index in buffer[] */
	u8		need_free;	/* Mask of kfree'able items in buffer[] */
	struct module	*owner;		/* Owner module for strings that don't then need freeing */
	char		*buffer[8];
};

extern __attribute__((format(printf, 4, 5)))
void logfc(struct fc_log *log, const char *prefix, char level, const char *fmt, ...);

#define __logfc(fc, l, fmt, ...) logfc((fc)->log.log, NULL, \
					l, fmt, ## __VA_ARGS__)
#define __plog(p, l, fmt, ...) logfc((p)->log, (p)->prefix, \
					l, fmt, ## __VA_ARGS__)
/**
 * infof - Store supplementary informational message
 * @fc: The context in which to log the informational message
 * @fmt: The format string
 *
 * Store the supplementary informational message for the process if the process
 * has enabled the facility.
 */
#define infof(fc, fmt, ...) __logfc(fc, 'i', fmt, ## __VA_ARGS__)
#define info_plog(p, fmt, ...) __plog(p, 'i', fmt, ## __VA_ARGS__)
#define infofc(p, fmt, ...) __plog((&(fc)->log), 'i', fmt, ## __VA_ARGS__)

/**
 * warnf - Store supplementary warning message
 * @fc: The context in which to log the error message
 * @fmt: The format string
 *
 * Store the supplementary warning message for the process if the process has
 * enabled the facility.
 */
#define warnf(fc, fmt, ...) __logfc(fc, 'w', fmt, ## __VA_ARGS__)
#define warn_plog(p, fmt, ...) __plog(p, 'w', fmt, ## __VA_ARGS__)
#define warnfc(fc, fmt, ...) __plog((&(fc)->log), 'w', fmt, ## __VA_ARGS__)

/**
 * errorf - Store supplementary error message
 * @fc: The context in which to log the error message
 * @fmt: The format string
 *
 * Store the supplementary error message for the process if the process has
 * enabled the facility.
 */
#define errorf(fc, fmt, ...) __logfc(fc, 'e', fmt, ## __VA_ARGS__)
#define error_plog(p, fmt, ...) __plog(p, 'e', fmt, ## __VA_ARGS__)
#define errorfc(fc, fmt, ...) __plog((&(fc)->log), 'e', fmt, ## __VA_ARGS__)

/**
 * invalf - Store supplementary invalid argument error message
 * @fc: The context in which to log the error message
 * @fmt: The format string
 *
 * Store the supplementary error message for the process if the process has
 * enabled the facility and return -EINVAL.
 */
#define invalf(fc, fmt, ...) (errorf(fc, fmt, ## __VA_ARGS__), -EINVAL)
#define inval_plog(p, fmt, ...) (error_plog(p, fmt, ## __VA_ARGS__), -EINVAL)
#define invalfc(fc, fmt, ...) (errorfc(fc, fmt, ## __VA_ARGS__), -EINVAL)

#endif /* _LINUX_FS_CONTEXT_H */
