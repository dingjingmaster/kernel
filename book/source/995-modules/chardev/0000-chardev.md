# 字符设备

Linux 为每个设备分配一个设备号，通过设备号对设备唯一标识，设备号分为：主设备号、次设备号。主设备号对应驱动程序，所有主设备号相同的设备具有相同驱动程序；次设备号用来标识连接到系统中相同类型的设备，主设备号+次设备号唯一标识连接到设备。`/proc/devices`查看所有设备主设备号。

## 字符设备结构

```
           字符设备
             /|\
              |
             \|/
           设备文件
           /     \
          /       \
         /         \
    设备结构体    操作函数
```

设备结构:
```c
struct xxx_char_dev {
    struct cdev cdev;   // 所有字符设备的公有属性
    char c
};
```

操作函数:
```c
struct file_operation fops = {
    // ...
};
```
一般通过设备文件来控制具体设备硬件，设备文件是具体设备的抽象。设备结构体来描述设备的属性，操作函数相当于设备的方法。

## 设备加载

```
设备  ------>  udev  ------> /sys/class/xxx 
                |
             /dev/xxx
```

## 同步问题

多进程同时访问设备时候会产生同步问题。

## ioctl
