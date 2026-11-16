# SPDX-License-Identifier: GPL-2.0

# AMD SME/SEV/SEV-ES/SEV-SNP 核心支持。
obj-y += core.o

ifdef CONFIG_FUNCTION_TRACER
# SEV 早期和 #VC 路径不能被 ftrace 插桩。
CFLAGS_REMOVE_core.o = -pg
endif

# 这些路径需要在异常/早期启动环境中可靠运行，关闭各类 sanitizer/instrumentation。
KASAN_SANITIZE_core.o	:= n
KMSAN_SANITIZE_core.o	:= n
KCOV_INSTRUMENT_core.o	:= n

# With some compiler versions the generated code results in boot hangs, caused
# by several compilation units. To be safe, disable all instrumentation.
# 某些编译器版本开启插桩会导致启动挂起，因此保守禁用 KCSAN。
KCSAN_SANITIZE		:= n
