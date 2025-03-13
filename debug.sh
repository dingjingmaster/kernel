#!/bin/bash


# 另一个终端输入:
#   gdb vmlinux
#   target remote :1234
#
# 或者使用图形化调试界面 ddd

# -s 启动GDB服务器(默认端口:1234)
# -S 让qemu暂停
qemu-system-x86_64 -kernel ./arch/x86/boot/bzImage \
	-append "console=ttyS0 nokaslr" \
	-nographic \
	-s \
	-S

