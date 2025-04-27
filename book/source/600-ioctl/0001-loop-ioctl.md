# Loop设备相关ioctl

## loop filters

内核为了让 loop 设备支持更多高级功能，添加了一些“过滤器”层，这些过滤器层可以在loop设备读取/写入真实后端文件的时候，自动做一些转换，比如：
- 加密/解密
- 压缩/解压缩
- 检查数据完整性
- 校验和(checksum)

标准的loop filter 类型包括：
|宏|值|作用|
|:----|:----|:----|
|LO_CRYPT_NONE|0|无加密(默认)|
|LO_CRYPT_XOR|1|简单的XOR加密（很老，几乎不用了）|
|LO_CRYPT_DES|2|DES加密(很老了)|
|LO_CRYPT_FISH2|3|FISH2加密(很少用)|
|LO_CRYPT_BLOW|4|Blowfish加密|
|LO_CRYPT_CAST128|5|CAST-128加密|
|LO_CRYPT_IDEA|6|IDEA加密|
|LO_CRYPT_CRYPTOAPI|18|用Linux CryptoAPI做加密(比较现代)|

> 以上命令截至 Linux 6.x

在应用层里目前常见的是：
- lo_encrypt_type == 0，什么也不做，直接passthrough
- 如果需要加密(比如挂载一个加密的镜像)，就需要配合 `lo_encrypt_type` 和 `lo_encrypt_key`
- 一些新系统直接走`dm-crypt`(Device Mapper加密)，而不是loop设备自带的filter
- 很多加密类型已经废弃，现代系统更推荐用`dm-crypt` + `LUKS`, loop filter指示为了兼容旧的

filter的工作流程：
假设设置了一个loop设备，配置了`lo_encrypt_type=LO_CRYPT_XOR`，那么当：
- 应用程序`read()`loop设备时候，内核实际上：
    - 先从后端文件读取出原始数据
    - 然后在内核内部做一次XOR解密
    - 然后把解密后的数据交给应用程序
- 引用程序`write()`也是先加密再写

> 这一切都是在内核中完成，对引用程序是透明的。这些filter是内核内置的，想要开发自己的filter，需要修改内核源码(loop.c)

## loop command

用来管理 loop 设备的一系列控制指令。比如：
- 绑定后端文件
- 解除绑定
- 查询状态
- 修改参数
- 支持加密、偏移、自动清理等

常见loop设备的ioctl command总表：
|ioctl命令|作用|备注|
|:----|:-----|:-----|
|LOOP_SET_FD|绑定后端文件到loop设备|只绑定，不设置其它参数|
|LOOP_CLR_FD|解绑|清空设备，恢复成空闲状态|
|LOOP_SET_STATUS|设置loop参数(老版)|已过时，被LOOP_SET_STATUS64取代|
|LOOP_GET_STATUS|获取loop参数(老版)|已过时，同上|
|LOOP_SET_STATUS64|设置loop参数|支持64位偏移、大小等|
|LOOP_GET_STATUS64|获取loop参数|同上|
|LOOP_CHANGE_FD|更换后端文件(热切换)|比CLR_FD + SET_FD快，且不中断数据流|
|LOOP_SET_CAPACITY|通知loop更新容量|比如后端文件大小改变了|
|LOOP_CTL_ADD|新建一个空闲loop设备|需要/dev/loop-control|
|LOOP_CTL_REMOVE|移除一个loop设备|同上|
|LOOP_CTL_GET_FREE|找一个空闲的loop设备号|同上|
|LOOP_CONFIGURE|一次性配置文件和状态(新接口)|Linux 5.8+|
|LOOP_SET_DIRECT_TO|打开/关闭Direct I/O(绕过 page cache)|Linux 5.9+|

### 使用时机
- 简单挂载一个文件，使用`LOOP_SET_FD`就可以了
- 挂载时候指定偏移、限制大小、加密：使用LOOP_SET_STATUS64 或 LOOP_CONFIGURE
- 查找一个空闲的loop设备：先open"/dev/loop-control"，然后调用 LOOP_CTL_GET_FREE

## 相关数据结构

|结构体|用途|状态|
|:----|:----|:----|
|struct loop_info|早期版本的loop参数(偏移、小于4G)|过时|
|struct loop_info64|现代版本的loop参数(支持64位偏移和大小)|广泛使用|
|struct loop_config|新式接口，一次性设置文件+状态(配合LOOP_CONFIGURE)|最新Linux5.8+|

### struct loop_info

```c
/* Backwards compatibility version */
struct loop_info {
    int                 lo_number;      /* ioctl r/o */
    __kernel_old_dev_t  lo_device;      /* ioctl r/o */
    unsigned long       lo_inode;       /* ioctl r/o */
    __kernel_old_dev_t  lo_rdevice;     /* ioctl r/o */
    int                 lo_offset;
    int                 lo_encrypt_type;        /* obsolete, ignored */
    int                 lo_encrypt_key_size;    /* ioctl w/o */
    int                 lo_flags;
    char                lo_name[LO_NAME_SIZE];
    unsigned char       lo_encrypt_key[LO_KEY_SIZE];    /* ioctl w/o */
    unsigned long       lo_init[2];
    char                reserved[4];
};
```

### struct loop_info64

```c
struct loop_info64 {
    __u64   lo_device;      /* ioctl r/o */
    __u64   lo_inode;       /* ioctl r/o */
    __u64   lo_rdevice;     /* ioctl r/o */
    __u64   lo_offset;
    __u64   lo_sizelimit;   /* bytes, 0 == max available */
    __u32   lo_number;      /* ioctl r/o */
    __u32   lo_encrypt_type;        /* obsolete, ignored */
    __u32   lo_encrypt_key_size;    /* ioctl w/o */
    __u32   lo_flags;
    __u8    lo_file_name[LO_NAME_SIZE];
    __u8    lo_crypt_name[LO_NAME_SIZE];
    __u8    lo_encrypt_key[LO_KEY_SIZE];    /* ioctl w/o */
    __u64   lo_init[2];
};
```

### struct loop_config

```c
struct loop_config {
    __u32               fd;
    __u32               block_size;
    struct loop_info64  info;
    __u64               __reserved[8];
};
```
