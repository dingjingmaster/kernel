/* SPDX-License-Identifier: GPL-2.0 */
#include <linux/mount.h>
#include <linux/seq_file.h>
#include <linux/poll.h>
#include <linux/ns_common.h>
#include <linux/fs_pin.h>

struct mnt_namespace {
	struct ns_common	ns;
	struct mount *	root;
	struct rb_root		mounts; /* Protected by namespace_sem */
	struct user_namespace	*user_ns;
	struct ucounts		*ucounts;
	u64			seq;	/* Sequence number to prevent loops */
	wait_queue_head_t poll;
	u64 event;
	unsigned int		nr_mounts; /* # of mounts in the namespace */
	unsigned int		pending_mounts;
	struct rb_node		mnt_ns_tree_node; /* node in the mnt_ns_tree */
	refcount_t		passive; /* number references not pinning @mounts */
} __randomize_layout;

struct mnt_pcp {
	int mnt_count;
	int mnt_writers;
};

struct mountpoint {
	struct hlist_node m_hash;
	struct dentry *m_dentry;
	struct hlist_head m_list;
	int m_count;
};

struct mount {
	struct hlist_node mnt_hash;	// 用来挂载描述符加入全局散列表mount_hashtable，关键字是（父挂载描述符，挂载点）
	struct mount *mnt_parent;	// 指向父亲，即文件系统1的mount实例
	struct dentry *mnt_mountpoint;	// 指向挂载点的目录
	struct vfsmount mnt;	// 指向文件系统2的根目录、指向文件系统2的超级块
	union {
		struct rcu_head mnt_rcu;
		struct llist_node mnt_llist;
	};
#ifdef CONFIG_SMP
	struct mnt_pcp __percpu *mnt_pcp;
#else
	int mnt_count;
	int mnt_writers;
#endif
	struct list_head mnt_mounts;	/* 子链表的头节点 list of children, anchored here */
	struct list_head mnt_child;	/* 加入父亲的孩子链表 and going through their mnt_child */
	struct list_head mnt_instance;	/* 把挂载描述符添加到超级块的挂载实例链表中，一个文件系统可以多次挂载  mount instance on sb->s_mounts */
	const char *mnt_devname;	/* 存储设备的名称 Name of device e.g. /dev/dsk/hda1 */
	union {
		struct rb_node mnt_node;	/* Under ns->mounts */
		struct list_head mnt_list;
	};
	struct list_head mnt_expire;	/* link in fs-specific expiry list */
	struct list_head mnt_share;	/* circular list of shared mounts */
	struct list_head mnt_slave_list;/* list of slave mounts */
	struct list_head mnt_slave;	/* slave list entry */
	struct mount *mnt_master;	/* slave is on master->mnt_slave_list */
	struct mnt_namespace *mnt_ns;	/* 指向挂载命名空间 containing namespace */
	struct mountpoint *mnt_mp;	/* 指向挂载点 where is it mounted */
	union {
		struct hlist_node mnt_mp_list;	/* list mounts with the same mountpoint */
		struct hlist_node mnt_umount;
	};
	struct list_head mnt_umounting; /* list entry for umount propagation */
#ifdef CONFIG_FSNOTIFY
	struct fsnotify_mark_connector __rcu *mnt_fsnotify_marks;
	__u32 mnt_fsnotify_mask;
#endif
	int mnt_id;			/* mount identifier, reused */
	u64 mnt_id_unique;		/* mount ID unique until reboot */
	int mnt_group_id;		/* peer group identifier */
	int mnt_expiry_mark;		/* true if marked for expiry */
	struct hlist_head mnt_pins;
	struct hlist_head mnt_stuck_children;
} __randomize_layout;

#define MNT_NS_INTERNAL ERR_PTR(-EINVAL) /* distinct from any mnt_namespace */

static inline struct mount *real_mount(struct vfsmount *mnt)
{
	return container_of(mnt, struct mount, mnt);
}

static inline int mnt_has_parent(struct mount *mnt)
{
	return mnt != mnt->mnt_parent;
}

static inline int is_mounted(struct vfsmount *mnt)
{
	/* neither detached nor internal? */
	return !IS_ERR_OR_NULL(real_mount(mnt)->mnt_ns);
}

extern struct mount *__lookup_mnt(struct vfsmount *, struct dentry *);

extern int __legitimize_mnt(struct vfsmount *, unsigned);

static inline bool __path_is_mountpoint(const struct path *path)
{
	struct mount *m = __lookup_mnt(path->mnt, path->dentry);
	return m && likely(!(m->mnt.mnt_flags & MNT_SYNC_UMOUNT));
}

extern void __detach_mounts(struct dentry *dentry);

static inline void detach_mounts(struct dentry *dentry)
{
	if (!d_mountpoint(dentry))
		return;
	__detach_mounts(dentry);
}

static inline void get_mnt_ns(struct mnt_namespace *ns)
{
	refcount_inc(&ns->ns.count);
}

extern seqlock_t mount_lock;

struct proc_mounts {
	struct mnt_namespace *ns;
	struct path root;
	int (*show)(struct seq_file *, struct vfsmount *);
};

extern const struct seq_operations mounts_op;

extern bool __is_local_mountpoint(struct dentry *dentry);
static inline bool is_local_mountpoint(struct dentry *dentry)
{
	if (!d_mountpoint(dentry))
		return false;

	return __is_local_mountpoint(dentry);
}

static inline bool is_anon_ns(struct mnt_namespace *ns)
{
	return ns->seq == 0;
}

static inline void move_from_ns(struct mount *mnt, struct list_head *dt_list)
{
	WARN_ON(!(mnt->mnt.mnt_flags & MNT_ONRB));
	mnt->mnt.mnt_flags &= ~MNT_ONRB;
	rb_erase(&mnt->mnt_node, &mnt->mnt_ns->mounts);
	list_add_tail(&mnt->mnt_list, dt_list);
}

bool has_locked_children(struct mount *mnt, struct dentry *dentry);
struct mnt_namespace *__lookup_next_mnt_ns(struct mnt_namespace *mnt_ns, bool previous);
static inline struct mnt_namespace *lookup_next_mnt_ns(struct mnt_namespace *mntns)
{
	return __lookup_next_mnt_ns(mntns, false);
}
static inline struct mnt_namespace *lookup_prev_mnt_ns(struct mnt_namespace *mntns)
{
	return __lookup_next_mnt_ns(mntns, true);
}
static inline struct mnt_namespace *to_mnt_ns(struct ns_common *ns)
{
	return container_of(ns, struct mnt_namespace, ns);
}
