# kernel

## clion源码阅读

1. 生成默认配置文件：`make defconfig`
2. 安装bear命令
3. 编译内核：`bear -- make -j32 vmlinux bzImage` 生成 `compile_commands.json`
4. 克隆 kernel-grok 项目 `https://github.com/habemus-papadum/kernel-grok`
5. 修改CMakeLists.txt

```
cmake_minimum_required(VERSION 2.8.8)
project(kernel)

set(SYSROOT sysroot)
SET(CMAKE_C_COMPILER "gcc")
set(CMAKE_C_STANDARD 90)
set(CMAKE_C_FLAGS  ${CMAKE_C_FLAGS} " --sysroot=${SYSROOT}" )

include_directories("include")
include_directories("include/uapi")
include_directories("include/generated")
include_directories("arch/x86/include")
include_directories("arch/x86/include/uapi")
include_directories("arch/x86/include/generated")
include_directories("arch/x86/include/generated/uapi")

add_definitions(-D__KERNEL__)
```

添加内容主要包括：
- 修改sysroot，这需要首先在CMakeLists.txt的同目录下创建一个名为sysroot的空目录，然后设置编译参数"--sysroot=${SYSROOT}"，这是因为sysroot其实是C标准头文件的位置，显然内核是不需要这些东西的，他们存在反而会干扰clion寻找正确头的文件，产生一下不必要得冲突
- 添加头文件路径，如果不进行这一步，那么clion将会出现大量的错误，提示找不到头文件或者变量、常量、函数等。具体添加的内容是我反复测试过的，添加这些内容后基本上保证头文件寻找正确，其中后四个是与架构相关的，x84_64可放心使用
- 使用C90的C语言标准，一开始我使用的是Clion的默认C标准，一直没有出过问题，但是昨天一下子就出现大量报错，不排除是因为我升级了最新的Clion 2021.2.2 ，最终加上了set(CMAKE_C_STANDARD 90)才恢复正常

## 内核学习路线

```
应用程序
------------------------系统调用------------------------------
+------------------------------------------------------------+
|                       系统调用层                            |
+------------------------------------------------------------+
+---------+---------+     +---------+---------+-------+------+
| 内存管理 | 中断管理 |     | 字符设备 | 网络设备 | 块设备 | KVM  |
+---------+---------+     +---------+---------+-------+------+
| 文件系统 | 进程调度 |     |        总线设备（USB/PCI）         |
+---------+---------+     +----------------------------------+
| 体系结构Arch抽象层  |     |         设备管理抽象层             |
+-------------------+     +----------------------------------+
--------------------------------------------------------------
+------------------------------------------------------------+
|                       硬件层面                              |
+------------------------------------------------------------+
```
