# libfuse25

调用顺序

## init

挂载时候调用，当fuse挂载成功后调用此回调，类似于“构造函数角色”，用来完成所有必要的初始化工作。

init 函数返回一个指针，这个指针指向文件系统的私有数据结构，可以在文件操作的时候通过`fuse_get_context()->private_data`访问此数据，这样避免使用全局变量，方便在多线程环境下管理状态信息。

## destroy

卸载时候调用，清理 `init` 中初始化的数据。传入参数是 `init` 函数中 `return` 回去的那个指针。

## 1. getattr

同 libfuse-21

## 2. opendir

在父目录调用 `ls -l` 命令触发，函数签名：
```c
int (*opendir) (const char* path, struct fuse_file_info* info);
```
- path：要打开的目录路径。
- info：包含文件相关信息的结构体，开发者可以利用其中的 fh(file handle)字段来存储目录对应的内部句柄。
例子：
```c
DIR* dp = opendir(path);
if (dp == NULL) {
    return -errno;
}

info->fh = dp;

return 0;
```

`struct fuse_file_info`结构如下：
```c
/**
 * Information about open files
 *
 * Changed in version 2.5
 */
struct fuse_file_info {
	/** Open flags.	 Available in open() and release() */
	int flags;

	/** Old file handle, don't use */
	unsigned long fh_old;

	/** In case of a write operation indicates if this was caused by a
	    writepage */
	int writepage;

	/** Can be filled in by open, to use direct I/O on this file.
	    Introduced in version 2.4 */
	unsigned int direct_io : 1;

	/** Can be filled in by open, to indicate, that cached file data
	    need not be invalidated.  Introduced in version 2.4 */
	unsigned int keep_cache : 1;

	/** Indicates a flush operation.  Set in flush operation, also
	    maybe set in highlevel lock operation and lowlevel release
	    operation.	Introduced in version 2.6 */
	unsigned int flush : 1;

	/** Can be filled in by open, to indicate that the file is not
	    seekable.  Introduced in version 2.8 */
	unsigned int nonseekable : 1;

	/* Indicates that flock locks for this file should be
	   released.  If set, lock_owner shall contain a valid value.
	   May only be set in ->release().  Introduced in version
	   2.9 */
	unsigned int flock_release : 1;

	/** Padding.  Do not use*/
	unsigned int padding : 27;

	/** File handle.  May be filled in by filesystem in open().
	    Available in all other file operations */
	uint64_t fh;

	/** Lock owner id.  Available in locking operations and flush */
	uint64_t lock_owner;
};
```

> 注意：与 `releasedir` 函数对应

## 3. fuse_readdir

同 libfuse21，只是多了 `struct fuse_file_info` 参数

## 4. create

当执行 `touch` 命令创建文件时候调用。

> 注意：不同于libfuse21触发的是mknod
