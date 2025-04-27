# ioctl函数介绍

ioctl（输入输出控制）函数是设备驱动程序中用于管理设备I/O通道的函数。它允许用户空间程序与内核空间的驱动模块进行交互，执行特定的控制操作，如设置串口波特率或控制马达转速。

## ioctl函数的定义和使用

```c
#include <sys/ioctl.h>

int ioctl(int fd, unsigned long op, ...);  /* glibc, BSD */
int ioctl(int fd, int op, ...);            /* musl, other UNIX */
```
- fd：文件描述符，由 open 函数返回。
- cmd：控制命令，用于指定要执行的操作。
- ...：可选参数，通常是一个指向数据的指针，具体取决于 cmd 的含义。

### 示例代码

```c
int fd = open("/dev/mydevice", O_RDWR);
if (fd < 0) {
    perror("open");
    return -1;
}

int ret = ioctl(fd, MY_IOCTL_CMD, &my_data);
if (ret < 0) {
    perror("ioctl");
    return -1;
}
```

## ioctl 命令码的生成

ioctl 命令码是一个 32 位整数，包含以下四个部分：
- 方向（2 位）：数据传输方向，如读、写或读写。
- 类型（8 位）：设备类型，用于区分不同的设备，一些文档中被翻译为“幻数”或者“魔数”。
- 序号（8 位）：命令编号，用于区分不同的操作，(0 ~ 255)。
- 大小（14 位）：数据大小，用于指定传输的数据类型和长度。

## ioctl应用层和内核层实现

### 应用层代码示例

```c
#include <sys/ioctl.h>
#include <fcntl.h>
#include <stdio.h>

#define LED_ON _IOW('L', 1, int)
#define LED_OFF _IOW('L', 2, int)

int main() 
{
    int fd = open("/dev/led", O_RDWR);
    if (fd < 0) {
        perror("open");
        return -1;
    }

    int led_num = 1;
    ioctl(fd, LED_ON, &led_num);
    ioctl(fd, LED_OFF, &led_num);

    close(fd);
    return 0;
}
```
### 内核层代码示例

```c
#include <linux/ioctl.h>
#include <linux/fs.h>

long led_ioctl(struct file *file, unsigned int cmd, unsigned long arg) 
{
    int led_num;
    switch (cmd) {
        case LED_ON:
            copy_from_user(&led_num, (int __user *)arg, sizeof(int));
            // 打开 LED
            break;
        case LED_OFF:
            copy_from_user(&led_num, (int __user *)arg, sizeof(int));
            // 关闭 LED
            break;
        default:
            return -ENOTTY;
    }

    return 0;
}

struct file_operations fops = {
    .unlocked_ioctl = led_ioctl,
};
```

通过以上示例，可以看到 ioctl 函数如何在用户空间和内核空间之间传递命令和数据，实现对设备的控制。

## Linux中ioctl使用

ioctl涉及的命令和传递的参数结构需要看内核源码中：`include/uapi/`目录下。
