## VFS 概览

VFS(Virtual File System)也就是虚拟文件系统层，是内核中所有文件系统的上一层，为应用层编程提供了统一的文件系统访问API接口。

应用层进程通过系统调用：`open(2)`、`stat(2)`、`read(2)`、`write(2)`、`chmod(2)`等，调用VFS提供的文件系统操作。

### 目录入口缓存(dcache)

VFS 实现了 `open(2)`、`stat(2)`、`chmod(2)`等文件系统相关系统调用。

VFS使用应用层传入的`pathname`(路径)参数在dcache中搜索要操作的文件对象。这种机制可以快速的把`pathname`转换成对应的dentry。

> 注意：dentry仅存在于RAM中，不会保存到磁盘。

理论上所有dentry构成了文件系统中文件视图。但是，大多数电脑的RAM不足以支撑同时加载所有的dentry，为了实现将用户层传入的`pathname`转为dentry，VFS会在解析`pathname`以此查找要操作dentry的过程中不断创建新的dentry。为了避免频繁销毁、创建dentry，部分dentry被缓存下来，缓存下来的所有dentry被称作dcache。

### Inode对象

每个 dentry 通常都有一个指向 inode 的指针。每个Inode代表文件系统的一个对象，比如：常规文件、文件夹、有名管道(FIFO)以及其他对象。

Inode是存在于磁盘(块设备)上的，当被需要时候会被复制到RAM中，修改后会被写入磁盘(块设备)上。

多个dentry可以同时指向一个inode(例如：硬链接)。

要查找指定的inode，VFS需要调用指定inode父文件夹的`lookup()`方法，这个方法由具体的文件系统实现。

一旦VFS找到指定的dentry(相当于找到了inode)，就可以执行inode(文件系统对象)对应的操作，比如：打开文件`open(2)`、获取属性`stat(2)`等，通过这些inode操作了读取inode对应数据。

### 文件对象

打开文件前，内核会分配一个`File`结构体对象，在初始化`File`对象时候会设置其中两个重要成员属性：1. 指向File对应dentry的指针; 2. 设置此`File`对应文件系统的文件操作函数，然后把此`File`对象指针放入打开此文件进程的文件描述符表中，把文件描述符表的索引返回给应用层进程。

应用层进程想要执行文件操作时候，通过文件描述符来告诉VFS层，具体要对哪个文件执行操作。因此只要用户层进程打开文件后没有关闭，在此进程运行期间，内核中的`File`对象一直存在，对应的dentry、inode也会一直处于使用状态(资源不被释放)。

## 文件系统对象

### 注册和挂载文件系统

文件系统注册和取消注册的API函数：

```c
#include <linux/fs.h>
extern int register_filesystem(struct file_system_type*);
extern int unregister_filesystem(struct file_system_type*);
```

传入参数类型`struct file_system_type`描述了文件系统的所有信息。当应用层通过`mount`请求挂载此文件系统到指定目录时候，VFS就会调用`struct file_system_type`中的`mount()`方法，此方法会返回新文件系统树的根`vfsmount`。当有`pathname`解析到此文件系统的挂载点，将会自动调到新文件系统的根目录中。

在`/proc/filesystems`中可以看到所有已经成功注册到内核的文件系统。

### struct file_system_type类型

```c
struct file_system_type
{
    /* 文件系统的名字,例如: ext2、iso9660、msdos */
    const char *name;

    /* 一些标记，类似: FS_REQUIRES_DEV、FS_NO_DCACHE */
    int fs_flags;

    /* 初始化 struct fs_context 中的 ops、fs_private字段 */
    int (*init_fs_context)(struct fs_context *);

    /* 指向 struct fs_parameter_spec 的文件系统挂载参数 */
    const struct fs_parameter_spec *parameters;

    /**
     * 文件系统挂载方法
     * 此方法调用成功会返回新文件系统树的根，调用失败返回ERR_PTR(error)
     *  fs_type
     *  flags: 挂载标记
     *  dev_name: 要挂载的设备名
     *  data: 挂载选项, mount -o 后边跟的内容
     *
     * mount()最重要的是填充 superblock 结构的 's_op' 字段，指向了 
     * 'struct super_operations', 这个结构体描述了下一级文件系统的实现。
     * 
     * 文件系统可以使用一个通用的 mount 实现，并提供 通用的回调函数给 fill_super()，回调如下：
     *  1. mount_bdev: 挂载位于块设备上的文件系统
     *  2. mount_nodev: 挂载无设备文件系统
     *  3. mount_single: 绑定挂载(在多个设备上共享挂载点)
     * fill_super() 回调实现如下参数：
     *  struct super_block* sb: 超级块设备。回调函数必须初始化此值
     *  void* data: 以ASCII字符串出现的挂载选项
     *  int silent: 出错时候是否忽略错误
     */
    struct dentry *(*mount) (struct file_system_type* fs_type, int flags, 
                            const char* dev_name, void* data);

    /* 文件系统停止运行会调用的方法 */
    void (*kill_sb) (struct super_block *);

    /* VFS内部使用: 使用 THIS_MODULE 宏初始化 */
    struct module *owner;

    /* VFS内部使用: 使用NULL初始化 */
    struct file_system_type * next;

    /* VFS内部使用: 文件系统superblocks示例的hlist */
    struct hlist_head fs_supers;
    struct lock_class_key s_lock_key;
    struct lock_class_key s_umount_key;
    struct lock_class_key s_vfs_rename_key;
    struct lock_class_key s_writers_key[SB_FREEZE_LEVELS];
    struct lock_class_key i_lock_key;
    struct lock_class_key i_mutex_key;
    struct lock_class_key invalidate_lock_key;
    struct lock_class_key i_mutex_dir_key;
};
```

### struct super_operations

当文件系统挂载成功后，就会创建其对应的超级块对象(Superblock Object)。

> superblock 中的函数除非另有说明，否则不要持有任何锁。这意味着所有的函数仅会被某一个进程上下文调用(例如：调用者不会是中断服务例程或者中断服务例程的下半程)。

```c
struct super_operations 
{
    /**
     *  被alloc_inode()调用，用于分配 struct inode 结构内存并初始化，
     *  如果此函数未定义，则使用默认函数分配一个最简单的'struct inode'
     */
    struct inode *(*alloc_inode)(struct super_block *sb);

    /**
     *  被destroy_inode()调用,用于释放 struct inode 结构体分配的内存。
     *  它与 alloc_inode 配合使用
     */
    void (*destroy_inode)(struct inode *);

    /**
     *  RCU 回调中调用此方法。如果在->destroy_inode 中使用 call_ruc()
     *  释放 'struct inode' ，最好用此方法释放内存
     * FIXME:// 这里描述不够清楚
     */
    void (*free_inode)(struct inode *);

    /**
     *  @brief 当inode被标记为 'dirty', VFS会调用此函数。
     *
     *  @note: 仅仅是 inode 被标记为 'dirty'，与数据无关。
     *   可以设置 I_DIRTY_DATASYNC，以使用 fdatasync() 更新数据。
     */
    void (*dirty_inode) (struct inode *, int flags);

    /**
     *  @brief VFS需要把inode写入磁盘时候会调用此方法。第二个参数表示是否同步写入。
     */
    int (*write_inode) (struct inode *, struct writeback_control *wbc);

    /**
     *  @brief 当最后一次访问 inode 结束时候调用，会持有 inode->i_lock 自旋锁。
     *  此方法不可以是NULL或者 'generic_delete_inode'
     */
    int (*drop_inode) (struct inode *);

    /**
     *  @brief 当VFS想要退出索引节点时候调用。
     */
    void (*evict_inode) (struct inode *);

    /**
     *  @brief 当VFS释放超级块时候调用(比如：卸载文件系统)
     *  调用前 superblock 会上锁
     */
    void (*put_super) (struct super_block *);

    /**
     *  @brief VFS把脏数据写入磁盘时候调用。
     *
     *  第二个参数 'wait' 指示是否等待写出完成
     */
    int (*sync_fs)(struct super_block *sb, int wait);

    /**
     *  @brief 如果提供了 `->freeze_super`就不会调用 `->freeze_fs`
     *  最大的不同是：'->freeze_super' 调用时候不需要 'down_write(&sb->s_umount).'
     *
     *  如果操作系统实现了 '->freeze_fs'，并且希望 '->freeze_fs' 也被调用，
     *  那么它必须从这个回调中显式的调用 '->freeze_fs'
     */
    int (*freeze_super) (struct super_block* sb, enum freeze_holder who);

    /**
     *  @brief 当VFS锁定文件系统并强制其进入一致状态时候调用。
     *  此方法目前由逻辑卷管理器(LVM)和ioctl(FIREEZE)使用
     */
    int (*freeze_fs) (struct super_block*);

    /**
     *  @brief 当VFS解锁文件系统并使其在 ‘->freeze_super’ 之后再次可写时候调用
     */
    int (*thaw_super) (struct super_block* sb, enum freeze_wholder who);

    /**
     *  @brief 当VFS解锁文件系统并使其在'->freeze_fs'之后再次可写时候调用
     */
    int (*unfreeze_fs) (struct super_block*);

    /**
     *  @brief 当VFS需要获取文件系统信息时候调用
     */
    int (*statfs) (struct dentry*, struct kstatfs*);

    /**
     *  @brief 当文件系统重新挂载时候调用。
     *  调用时候内核会上锁
     */
    int (*remount_fs) (struct super_block *, int *, char *);

    /**
     *  @brief 文件系统卸载时候调用
     */
    void (*umount_begin) (struct super_block *);

    /**
     *  @brief 显式挂载信息时候VFS调用。
     *  比如：/proc/<pid>/mounts、/proc/<pid>/mountinfo
     */
    int (*show_options)(struct seq_file *, struct dentry *);

    /**
     *  @brief VFS显示设备名字时候调用 /proc/<pid>/{mounts,mountinfo,mountstats}
     */
    int (*show_devname)(struct seq_file *, struct dentry *);

    /**
     *  @brief VFS显式挂载点根目录时候调用 /proc/<pid>/mountinfo
     */
    int (*show_path)(struct seq_file *, struct dentry *);

    /**
     *  @brief VFS显式文件系统信息时候调用 /proc/<pid>/mountstats
     */
    int (*show_stats)(struct seq_file *, struct dentry *);

    /**
     *  @brief VFS从文件系统配额文件中读取数据
     */
    ssize_t (*quota_read)(struct super_block *, int, char *, size_t, loff_t);

    /**
     *  @brief VFS往文件系统配额中写数据
     */
    ssize_t (*quota_write)(struct super_block *, int, const char *, size_t,loff_t);

    /**
     *  @brief VFS获取'struct dquot'结构
     */
    struct dquot **(*get_dquots)(struct inode *);

    /**
     *  @brief 由sb缓存收缩函数调用，用于文件系统返回其包含的可空闲缓存对象的数量。
     */
    long (*nr_cached_objects)(struct super_block *, struct shrink_control *);

    /**
     *  @brief 由sb缓存回收函数调用，用于文件系统扫描可以被释放的对象
     *  要实现此函数, 需要实现 '->nr_cached_objects'
     */
    long (*free_cached_objects)(struct super_block *, struct shrink_control *);
};
```

初始化 inode 时候要配置 'i_op' 字段，它指向了 'struct inode_operations'，这个结构体提供了针对inode的操作函数。

### struct xattr_handler

在支持扩展属性(xattrs)的文件系统上，超级块 `s_xattr`指向了一个`NULL`结束的数组，扩展属性是`name:value`对。

## Inode 对象

inode对象表示文件系统中的对象。

### struct inode_operations

此结构体描述了VFS如何操作文件系统中的inode，定义如下：

> 注意：所有方法都不会持有任何锁，除非另有说明

```c
struct inode_operations
{
    /**
     *  @brief 由 open(2) 和 create(2) 调用。
     *  得到的dentry可能并不包含inode，需要调用 'd_instantiate()'生成带有inode的dentry
     */
    int (*create) (struct mnt_idmap *, struct inode *,struct dentry *, umode_t, bool);

    /**
     *  @brief VFS在父目录中查找inode的时候调用。
     *  此函数必须调用 'd_add()' 把查找到的inode加入dentry，同时 inode 中的 'i_count' 字段值+1。
     *  如果没找到指定inode，需要在dentry中插入NULL。
     */
    struct dentry * (*lookup) (struct inode *,struct dentry *, unsigned int);

    /**
     *  @brief 被link系统调用调用。
     */
    int (*link) (struct dentry *,struct inode *,struct dentry *);

    int (*unlink) (struct inode *,struct dentry *);

    int (*symlink) (struct mnt_idmap *, struct inode *,struct dentry *, const char *);

    int (*mkdir) (struct mnt_idmap *, struct inode *,struct dentry *,umode_t);

    int (*rmdir) (struct inode *,struct dentry *);

    int (*mknod) (struct mnt_idmap *, struct inode *,struct dentry *,umode_t,dev_t);

    int (*rename) (struct mnt_idmap *, struct inode *, struct dentry *, struct inode *, struct dentry *, unsigned int);

    int (*readlink) (struct dentry *, char __user *,int);

    /**
     *  @brief 由VFS调用，以获取他所指向的索引节点的符号链接。
     */
    const char *(*get_link) (struct dentry *, struct inode *, struct delayed_call *);

    int (*permission) (struct mnt_idmap *, struct inode *, int);

    struct posix_acl * (*get_inode_acl)(struct inode *, int, bool);

    /**
     *  @brief 被chmod(2) 调用
     */
    int (*setattr) (struct mnt_idmap *, struct dentry *, struct iattr *);

    /**
     *  @brief 被stat(2) 调用
     */
    int (*getattr) (struct mnt_idmap *, const struct path *, struct kstat*, u32, unsigned int);

    ssize_t (*listxattr) (struct dentry *, char *, size_t);

    void (*update_time)(struct inode *, struct timespec *, int);

    int (*atomic_open)(struct inode *, struct dentry *, struct file *, unsigned open_flag, umode_t create_mode);

    int (*tmpfile) (struct mnt_idmap *, struct inode *, struct file *, umode_t);

    struct posix_acl * (*get_acl)(struct mnt_idmap *, struct dentry *, int);

    int (*set_acl)(struct mnt_idmap *, struct dentry *, struct posix_acl *, int);

    int (*fileattr_set)(struct mnt_idmap *idmap, struct dentry *dentry, struct fileattr *fa);

    int (*fileattr_get)(struct dentry *dentry, struct fileattr *fa);

    struct offset_ctx *(*get_offset_ctx)(struct inode *inode);
};
```

## 地址空间对象(Address Space Object)

地址空间对象用于对页缓存中的页进行分组和管理。它可以用来跟踪文件（或其他任何东西）中的页面，也可以跟踪文件部分到进程地址空间的映射。

地址空间可以提供许多不同但相关的服务。这些包括通信内存压力，按地址查找页面，以及跟踪标记为Dirty或Writeback的页。

### 回写时候处理错误

### struct address_space_operations

此结构体描述了VFS如何操作文件系统中文件到页面缓存的映射。定义了以下成员：

```c
struct address_space_operations 
{
    int (*writepage)(struct page *page, struct writeback_control *wbc);

    int (*read_folio)(struct file *, struct folio *);

    int (*writepages)(struct address_space *, struct writeback_control *);

    bool (*dirty_folio)(struct address_space *, struct folio *);

    void (*readahead)(struct readahead_control *);

    int (*write_begin)(struct file *, struct address_space *mapping, loff_t pos, unsigned len, struct page **pagep, void **fsdata);

    int (*write_end)(struct file *, struct address_space *mapping, loff_t pos, unsigned len, unsigned copied, struct folio *folio, void *fsdata);

    sector_t (*bmap)(struct address_space *, sector_t); void (*invalidate_folio) (struct folio *, size_t start, size_t len);

    bool (*release_folio)(struct folio *, gfp_t);

    void (*free_folio)(struct folio *);

    ssize_t (*direct_IO)(struct kiocb *, struct iov_iter *iter);

    int (*migrate_folio)(struct mapping *, struct folio *dst, struct folio *src, enum migrate_mode);

    int (*launder_folio) (struct folio *);

    bool (*is_partially_uptodate) (struct folio *, size_t from, size_t count);

    void (*is_dirty_writeback)(struct folio *, bool *, bool *);

    int (*error_remove_folio)(struct mapping *mapping, struct folio *);

    int (*swap_activate)(struct swap_info_struct *sis, struct file *f, sector_t *span)

    int (*swap_deactivate)(struct file *);

    int (*swap_rw)(struct kiocb *iocb, struct iov_iter *iter);
};
```

## 文件对象

file对象表示由进程打开的文件。这在POSIX术语中也称为“打开文件描述”。

### struct file_operations

此结构体描述了VFS如何操作打开的文件。从内核4.18开始，定义了以下成员

```c
struct file_operations 
{
    struct module *owner;

    loff_t (*llseek) (struct file *, loff_t, int);

    ssize_t (*read) (struct file *, char __user *, size_t, loff_t *);

    ssize_t (*write) (struct file *, const char __user *, size_t, loff_t*);

    ssize_t (*read_iter) (struct kiocb *, struct iov_iter *);

    ssize_t (*write_iter) (struct kiocb *, struct iov_iter *);

    int (*iopoll)(struct kiocb *kiocb, bool spin);

    int (*iterate_shared) (struct file *, struct dir_context *);

    __poll_t (*poll) (struct file *, struct poll_table_struct *);

    long (*unlocked_ioctl) (struct file *, unsigned int, unsigned long);

    long (*compat_ioctl) (struct file *, unsigned int, unsigned long);

    int (*mmap) (struct file *, struct vm_area_struct *);

    int (*open) (struct inode *, struct file *);

    int (*flush) (struct file *, fl_owner_t id);

    int (*release) (struct inode *, struct file *);

    int (*fsync) (struct file *, loff_t, loff_t, int datasync);

    int (*fasync) (int, struct file *, int);

    int (*lock) (struct file *, int, struct file_lock *);

    unsigned long (*get_unmapped_area)(struct file *, unsigned long, unsigned long, unsigned long, unsigned long);

    int (*check_flags)(int);

    int (*flock) (struct file *, int, struct file_lock *);

    ssize_t (*splice_write)(struct pipe_inode_info *, struct file *, loff_t *, size_t, unsigned int);

    ssize_t (*splice_read)(struct file *, loff_t *, struct pipe_inode_info*, size_t, unsigned int);

    int (*setlease)(struct file *, long, struct file_lock **, void **);

    long (*fallocate)(struct file *file, int mode, loff_t offset, loff_t len);

    void (*show_fdinfo)(struct seq_file *m, struct file *f);

#ifndef CONFIG_MMU
    unsigned (*mmap_capabilities)(struct file *);
#endif

    ssize_t (*copy_file_range)(struct file *, loff_t, struct file *, loff_t, size_t, unsigned int);

    loff_t (*remap_file_range)(struct file *file_in, loff_t pos_in,struct file *file_out, loff_t pos_out, loff_t len, unsigned int remap_flags);

    int (*fadvise)(struct file *, loff_t, loff_t, int);
};
```

## dcache(Directory Entry Cache)对象

### struct dentry_operations

此结构体描述了文件系统如何重载标准的dentry操作。dentry和dcache是VFS和各个文件系统实现的领域。设备驱动程序在这里没有业务。这些方法可以设置为NULL，因为它们要么是可选的，要么VFS使用默认值。从内核2.6.22开始，定义了以下成员：

```c
struct dentry_operations 
{
    int (*d_revalidate)(struct dentry *, unsigned int);

    int (*d_weak_revalidate)(struct dentry *, unsigned int);

    int (*d_hash)(const struct dentry *, struct qstr *);

    int (*d_compare)(const struct dentry *, unsigned int, const char *, const struct qstr *);

    int (*d_delete)(const struct dentry *);

    int (*d_init)(struct dentry *);

    void (*d_release)(struct dentry *);

    void (*d_iput)(struct dentry *, struct inode *);

    char *(*d_dname)(struct dentry *, char *, int);

    struct vfsmount *(*d_automount)(struct path *);

    int (*d_manage)(const struct path *, bool);

    struct dentry *(*d_real)(struct dentry *, enum d_real_type type);
};
```


