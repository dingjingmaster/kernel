# libfuse21 

本节旨在说明使用fuse实现文件系统挂载后函数的调用顺序

完整代码查看[https://github.com/dingjingmaster/fuse-example](https://github.com/dingjingmaster/fuse-example)，进程名是：`fuse-example`

涉及的函数如下：
```c
static struct fuse_operations ops = {
    .getattr = fuse_getattr,
    .readlink = fuse_readlink,
    .getdir = fuse_getdir,
    .mknod = fuse_mknod,
    .mkdir = fuse_mkdir,
    .unlink = fuse_unlink,
    .rmdir = fuse_rmdir,
    .symlink = fuse_symlink,
    .rename = fuse_rename,
    .link = fuse_link,
    .chmod = fuse_chmod,
    .chown = fuse_chown,
    .truncate = fuse_truncate,
    .utime = fuse_utime,
    .open = fuse_open,
    .read = fuse_read,
    .write = fuse_write,
    .statfs = fuse_statfs,
    .flush = fuse_flush,
    .release = fuse_release,
    .fsync = fuse_fsync,
    .setxattr = fuse_setxattr,
    .getxattr = fuse_getxattr,
    .listxattr = fuse_listxattr,
    .removexattr = fuse_removexattr,
};
```

## 0. 挂载

```shell
sudo ./fuse-example ./<挂载点> -o allow_other
# 或进入调试模式
sudo ./fuse-example -f -d ./<挂载点> -o allow_other
```

## 1. getattr

挂载时候、在挂载点父目录执行`ls`命令、执行`cd`命令进入挂载后的文件系统，三者都会触发挂载点 "/" 目录的属性信息信息获取，需要手动填充 `struct stat` 结构体。

## 2. getdir

进入挂载点根目录，并执行 `ls` 时候会触发 `getdir` 函数。getdir中列出所有文件夹和文件，同时要注意，列出的文件夹和文件，需要在 `getattr` 中填充其属性。否则执行`ls`会报错：**输入/输出错误，XXX无法访问**。

针对此函数中创建的文件，还会触发 `listxattr` 函数。

## 3. listxattr

扩展属性(xattr)是一种文件系统机制，用于为文件或目录存储额外的元数据，比如：
- 用户设置的标签
- 安全属性(如：selinux)
- ACL信息(如：system.posix_acl_access)

这些属性不是文件内容，也不是标准的权限位或时间戳，但是可以通过专门的系统调用访问，比如：
- getxattr：获取属性值
- setxattr：设置属性值
- listxattr：列出属性名
- removexattr：删除属性名

fuse 相关函数签名如下：
```c
int fuse_listxattr(const char* path, char* list, size_t);
```

参数说明：
- path：要列出属性的文件路径
- list：用于存储属性名的缓存区(用\0分割的字符串列表)
- size：list缓存区的大小。

返回值：
- 如果 list == NULL 或 size == 0：返回所需的缓存区大小，不写如数据。
- 如果 list != NULL 且 size 足够大：将属性名写入 list 并返回总字节数
- 如果出错：返回负的错误码

> 注意：getxattr 和 setxattr 

> 注意：这个函数要么不实现，直接返回0，否则执行`ls -l`时候会报错：“输入输出错误”之类的，还未分析到原因。

## open

执行 `ls -al` 或 `cat` 等命令会打开文件。

此处需要注意，由于 FUSE 21 版本中的 open 能力有限，仅能判断是否需要打开文件（返回0表示能打开，否则返回错误码）。

```c
static int fuse_open(const char* path, int file)
{
    syslog(LOG_INFO,"open, path: %s", path);

    if (!path) {
        syslog(LOG_ERR, "open failed!");
        return -EINVAL;
    }

    if (0 == strcmp(path, "/")) {
        return -EBADE;
    }
    if (0 == strcmp(path, "/dir1")) {
        return -EBADE;
    }
    if (0 == strcmp(path, "/file1")) {
        return 0;
    }

    return -ENOENT;
}
```
## read

根据 `getattr` 属性中文件大小属性读取文件数据。

## flush

文件系统关闭时候会调用`flush`

## release

文件系统调用`flush`之后调用 `release`

## write

文件写入时候调用，需要注意的是，需要返回文件写入的字节数

## mknod

调用 `touch file` 会触发 `mknod` 调用。

调用后会创建文件，通过 `getattr` 检查是否调用成功。

## mkdir

创建文件夹调用此方法

## truncate

清空文件夹
