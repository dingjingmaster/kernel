# SPDX-License-Identifier: GPL-2.0
#
# Makefile for the x86 low level entry code
#

# x86 低层入口代码运行在异常/系统调用边界，禁止 sanitizer、coverage 和部分插桩。
KASAN_SANITIZE := n
UBSAN_SANITIZE := n
KCOV_INSTRUMENT := n

# common.o 参与 noinstr/入口路径，去掉 ftrace 和栈保护器。
CFLAGS_REMOVE_common.o		= $(CC_FLAGS_FTRACE)

CFLAGS_common.o			+= -fno-stack-protector

# 根据 32/64 位构建对应入口和系统调用分发表。
obj-y				:= entry.o entry_$(BITS).o syscall_$(BITS).o
obj-y				+= common.o

# vDSO/vsyscall 单独放在子目录构建。
obj-y				+= vdso/
obj-y				+= vsyscall/

# 抢占调度 thunk 只在启用抢占时需要。
obj-$(CONFIG_PREEMPTION)	+= thunk.o
# FRED 入口同样属于低层入口路径，禁用栈保护器和 ftrace。
CFLAGS_entry_fred.o		+= -fno-stack-protector
CFLAGS_REMOVE_entry_fred.o	+= -pg $(CC_FLAGS_FTRACE)
obj-$(CONFIG_X86_FRED)		+= entry_64_fred.o entry_fred.o

# 兼容 32 位 ABI 和 x32 ABI 时额外编入对应系统调用分发表。
obj-$(CONFIG_IA32_EMULATION)	+= entry_64_compat.o syscall_32.o
obj-$(CONFIG_X86_X32_ABI)	+= syscall_x32.o
