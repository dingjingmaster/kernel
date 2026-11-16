# SPDX-License-Identifier: GPL-2.0
# CoCo 核心代码会在很早期或 noinstr 路径中运行，禁止 ftrace/KASAN/栈保护器插桩。
CFLAGS_REMOVE_core.o	= -pg
KASAN_SANITIZE_core.o	:= n
CFLAGS_core.o		+= -fno-stack-protector

# 通用机密计算能力抽象。
obj-y += core.o

# 根据配置分别编入 Intel TDX guest 和 AMD SME/SEV 支持。
obj-$(CONFIG_INTEL_TDX_GUEST)	+= tdx/
obj-$(CONFIG_AMD_MEM_ENCRYPT)   += sev/
