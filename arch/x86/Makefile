# SPDX-License-Identifier: GPL-2.0
# Unified Makefile for i386 and x86_64

# select defconfig based on actual architecture
ifeq ($(ARCH),x86)
  ifeq ($(shell uname -m | sed -e 's/i.86/i386/'),i386)
        KBUILD_DEFCONFIG := i386_defconfig
  else
        KBUILD_DEFCONFIG := x86_64_defconfig
  endif
else
        KBUILD_DEFCONFIG := $(ARCH)_defconfig
endif

ifdef CONFIG_CC_IS_GCC
RETPOLINE_CFLAGS	:= $(call cc-option,-mindirect-branch=thunk-extern -mindirect-branch-register)
RETPOLINE_VDSO_CFLAGS	:= $(call cc-option,-mindirect-branch=thunk-inline -mindirect-branch-register)
endif
ifdef CONFIG_CC_IS_CLANG
RETPOLINE_CFLAGS	:= -mretpoline-external-thunk
RETPOLINE_VDSO_CFLAGS	:= -mretpoline
endif
RETPOLINE_CFLAGS	+= $(call cc-option,-mindirect-branch-cs-prefix)

ifdef CONFIG_MITIGATION_RETHUNK
RETHUNK_CFLAGS		:= -mfunction-return=thunk-extern
RETHUNK_RUSTFLAGS	:= -Zfunction-return=thunk-extern
RETPOLINE_CFLAGS	+= $(RETHUNK_CFLAGS)
RETPOLINE_RUSTFLAGS	+= $(RETHUNK_RUSTFLAGS)
endif

export RETHUNK_CFLAGS
export RETHUNK_RUSTFLAGS
export RETPOLINE_CFLAGS
export RETPOLINE_RUSTFLAGS
export RETPOLINE_VDSO_CFLAGS

# For gcc stack alignment is specified with -mpreferred-stack-boundary,
# clang has the option -mstack-alignment for that purpose.
ifneq ($(call cc-option, -mpreferred-stack-boundary=4),)
      cc_stack_align4 := -mpreferred-stack-boundary=2
      cc_stack_align8 := -mpreferred-stack-boundary=3
else ifneq ($(call cc-option, -mstack-alignment=16),)
      cc_stack_align4 := -mstack-alignment=4
      cc_stack_align8 := -mstack-alignment=8
endif

# How to compile the 16-bit code.  Note we always compile for -march=i386;
# that way we can complain to the user if the CPU is insufficient.
REALMODE_CFLAGS	:= -std=gnu11 -m16 -g -Os -DDISABLE_BRANCH_PROFILING -D__DISABLE_EXPORTS \
		   -Wall -Wstrict-prototypes -march=i386 -mregparm=3 \
		   -fno-strict-aliasing -fomit-frame-pointer -fno-pic \
		   -mno-mmx -mno-sse $(call cc-option,-fcf-protection=none)

REALMODE_CFLAGS += -ffreestanding
REALMODE_CFLAGS += -fno-stack-protector
REALMODE_CFLAGS += -Wno-address-of-packed-member
REALMODE_CFLAGS += $(cc_stack_align4)
REALMODE_CFLAGS += $(CLANG_FLAGS)
ifdef CONFIG_CC_IS_CLANG
REALMODE_CFLAGS += -Wno-gnu
endif
export REALMODE_CFLAGS

# BITS is used as extension for files which are available in a 32 bit
# and a 64 bit version to simplify shared Makefiles.
# e.g.: obj-y += foo_$(BITS).o
export BITS

#
# Prevent GCC from generating any FP code by mistake.
#
# This must happen before we try the -mpreferred-stack-boundary, see:
#
#    https://gcc.gnu.org/bugzilla/show_bug.cgi?id=53383
#
KBUILD_CFLAGS += -mno-sse -mno-mmx -mno-sse2 -mno-3dnow -mno-avx
KBUILD_RUSTFLAGS += --target=$(objtree)/scripts/target.json
KBUILD_RUSTFLAGS += -Ctarget-feature=-sse,-sse2,-sse3,-ssse3,-sse4.1,-sse4.2,-avx,-avx2

#
# CFLAGS for compiling floating point code inside the kernel.
#
CC_FLAGS_FPU := -msse -msse2
ifdef CONFIG_CC_IS_GCC
# Stack alignment mismatch, proceed with caution.
# GCC < 7.1 cannot compile code using `double` and -mpreferred-stack-boundary=3
# (8B stack alignment).
# See https://gcc.gnu.org/bugzilla/show_bug.cgi?id=53383
#
# The "-msse" in the first argument is there so that the
# -mpreferred-stack-boundary=3 build error:
#
#  -mpreferred-stack-boundary=3 is not between 4 and 12
#
# can be triggered. Otherwise gcc doesn't complain.
CC_FLAGS_FPU += -mhard-float
CC_FLAGS_FPU += $(call cc-option,-msse -mpreferred-stack-boundary=3,-mpreferred-stack-boundary=4)
endif

ifeq ($(CONFIG_X86_KERNEL_IBT),y)
#
# Kernel IBT has S_CET.NOTRACK_EN=0, as such the compilers must not generate
# NOTRACK prefixes. Current generation compilers unconditionally employ NOTRACK
# for jump-tables, as such, disable jump-tables for now.
#
# (jump-tables are implicitly disabled by RETPOLINE)
#
#   https://gcc.gnu.org/bugzilla/show_bug.cgi?id=104816
#
KBUILD_CFLAGS += $(call cc-option,-fcf-protection=branch -fno-jump-tables)
KBUILD_RUSTFLAGS += -Zcf-protection=branch -Zno-jump-tables
else
KBUILD_CFLAGS += $(call cc-option,-fcf-protection=none)
endif

ifeq ($(CONFIG_X86_32),y)
        BITS := 32
        UTS_MACHINE := i386
        CHECKFLAGS += -D__i386__

        KBUILD_AFLAGS += -m32
        KBUILD_CFLAGS += -m32

        KBUILD_CFLAGS += -msoft-float -mregparm=3 -freg-struct-return

        # Never want PIC in a 32-bit kernel, prevent breakage with GCC built
        # with nonstandard options
        KBUILD_CFLAGS += -fno-pic

        # Align the stack to the register width instead of using the default
        # alignment of 16 bytes. This reduces stack usage and the number of
        # alignment instructions.
        KBUILD_CFLAGS += $(cc_stack_align4)

        # CPU-specific tuning. Anything which can be shared with UML should go here.
        include $(srctree)/arch/x86/Makefile_32.cpu
        KBUILD_CFLAGS += $(cflags-y)

        # temporary until string.h is fixed
        KBUILD_CFLAGS += -ffreestanding

    ifeq ($(CONFIG_STACKPROTECTOR),y)
        ifeq ($(CONFIG_SMP),y)
            KBUILD_CFLAGS += -mstack-protector-guard-reg=fs \
                             -mstack-protector-guard-symbol=__ref_stack_chk_guard
        else
            KBUILD_CFLAGS += -mstack-protector-guard=global
        endif
    endif
else
        BITS := 64
        UTS_MACHINE := x86_64
        CHECKFLAGS += -D__x86_64__

        KBUILD_AFLAGS += -m64
        KBUILD_CFLAGS += -m64

        # Align jump targets to 1 byte, not the default 16 bytes:
        KBUILD_CFLAGS += $(call cc-option,-falign-jumps=1)

        # Pack loops tightly as well:
        KBUILD_CFLAGS += $(call cc-option,-falign-loops=1)

        # Don't autogenerate traditional x87 instructions
        KBUILD_CFLAGS += -mno-80387
        KBUILD_CFLAGS += $(call cc-option,-mno-fp-ret-in-387)

        # By default gcc and clang use a stack alignment of 16 bytes for x86.
        # However the standard kernel entry on x86-64 leaves the stack on an
        # 8-byte boundary. If the compiler isn't informed about the actual
        # alignment it will generate extra alignment instructions for the
        # default alignment which keep the stack *mis*aligned.
        # Furthermore an alignment to the register width reduces stack usage
        # and the number of alignment instructions.
        KBUILD_CFLAGS += $(cc_stack_align8)

	# Use -mskip-rax-setup if supported.
	KBUILD_CFLAGS += $(call cc-option,-mskip-rax-setup)

        # FIXME - should be integrated in Makefile.cpu (Makefile_32.cpu)
        cflags-$(CONFIG_MK8)		+= -march=k8
        cflags-$(CONFIG_MPSC)		+= -march=nocona
        cflags-$(CONFIG_MCORE2)		+= -march=core2
        cflags-$(CONFIG_MATOM)		+= -march=atom
        cflags-$(CONFIG_GENERIC_CPU)	+= -mtune=generic
        KBUILD_CFLAGS += $(cflags-y)

        rustflags-$(CONFIG_MK8)		+= -Ctarget-cpu=k8
        rustflags-$(CONFIG_MPSC)	+= -Ctarget-cpu=nocona
        rustflags-$(CONFIG_MCORE2)	+= -Ctarget-cpu=core2
        rustflags-$(CONFIG_MATOM)	+= -Ctarget-cpu=atom
        rustflags-$(CONFIG_GENERIC_CPU)	+= -Ztune-cpu=generic
        KBUILD_RUSTFLAGS += $(rustflags-y)

        KBUILD_CFLAGS += -mno-red-zone
        KBUILD_CFLAGS += -mcmodel=kernel
        KBUILD_RUSTFLAGS += -Cno-redzone=y
        KBUILD_RUSTFLAGS += -Ccode-model=kernel
endif

#
# If the function graph tracer is used with mcount instead of fentry,
# '-maccumulate-outgoing-args' is needed to prevent a GCC bug
# (https://gcc.gnu.org/bugzilla/show_bug.cgi?id=42109)
#
ifdef CONFIG_FUNCTION_GRAPH_TRACER
  ifndef CONFIG_HAVE_FENTRY
	ACCUMULATE_OUTGOING_ARGS := 1
  endif
endif

ifeq ($(ACCUMULATE_OUTGOING_ARGS), 1)
	# This compiler flag is not supported by Clang:
	KBUILD_CFLAGS += $(call cc-option,-maccumulate-outgoing-args,)
endif

# Workaround for a gcc prelease that unfortunately was shipped in a suse release
KBUILD_CFLAGS += -Wno-sign-compare
#
KBUILD_CFLAGS += -fno-asynchronous-unwind-tables

# Avoid indirect branches in kernel to deal with Spectre
ifdef CONFIG_MITIGATION_RETPOLINE
  KBUILD_CFLAGS += $(RETPOLINE_CFLAGS)
  KBUILD_RUSTFLAGS += $(RETPOLINE_RUSTFLAGS)
  # Additionally, avoid generating expensive indirect jumps which
  # are subject to retpolines for small number of switch cases.
  # LLVM turns off jump table generation by default when under
  # retpoline builds, however, gcc does not for x86. This has
  # only been fixed starting from gcc stable version 8.4.0 and
  # onwards, but not for older ones. See gcc bug #86952.
  ifndef CONFIG_CC_IS_CLANG
    KBUILD_CFLAGS += -fno-jump-tables
  endif
endif

ifdef CONFIG_MITIGATION_SLS
  KBUILD_CFLAGS += -mharden-sls=all
endif

ifdef CONFIG_CALL_PADDING
PADDING_CFLAGS := -fpatchable-function-entry=$(CONFIG_FUNCTION_PADDING_BYTES),$(CONFIG_FUNCTION_PADDING_BYTES)
KBUILD_CFLAGS += $(PADDING_CFLAGS)
export PADDING_CFLAGS

PADDING_RUSTFLAGS := -Zpatchable-function-entry=$(CONFIG_FUNCTION_PADDING_BYTES),$(CONFIG_FUNCTION_PADDING_BYTES)
KBUILD_RUSTFLAGS += $(PADDING_RUSTFLAGS)
export PADDING_RUSTFLAGS
endif

KBUILD_LDFLAGS += -m elf_$(UTS_MACHINE)

ifdef CONFIG_X86_NEED_RELOCS
LDFLAGS_vmlinux := --emit-relocs --discard-none
else
LDFLAGS_vmlinux :=
endif

#
# The 64-bit kernel must be aligned to 2MB.  Pass -z max-page-size=0x200000 to
# the linker to force 2MB page size regardless of the default page size used
# by the linker.
#
ifdef CONFIG_X86_64
LDFLAGS_vmlinux += -z max-page-size=0x200000
endif


archscripts: scripts_basic
	$(Q)$(MAKE) $(build)=arch/x86/tools relocs

###
# Syscall table generation

archheaders:
	$(Q)$(MAKE) $(build)=arch/x86/entry/syscalls all

###
# Kernel objects

libs-y  += arch/x86/lib/

# drivers-y are linked after core-y
drivers-$(CONFIG_MATH_EMULATION) += arch/x86/math-emu/
drivers-$(CONFIG_PCI)            += arch/x86/pci/

# suspend and hibernation support
drivers-$(CONFIG_PM) += arch/x86/power/

drivers-$(CONFIG_VIDEO) += arch/x86/video/

####
# boot loader support. Several targets are kept for legacy purposes

boot := arch/x86/boot

BOOT_TARGETS = bzdisk fdimage fdimage144 fdimage288 hdimage isoimage

PHONY += bzImage $(BOOT_TARGETS)

# Default kernel to build
all: bzImage

# KBUILD_IMAGE specify target image being built
KBUILD_IMAGE := $(boot)/bzImage

bzImage: vmlinux
ifeq ($(CONFIG_X86_DECODER_SELFTEST),y)
	$(Q)$(MAKE) $(build)=arch/x86/tools posttest
endif
	$(Q)$(MAKE) $(build)=$(boot) $(KBUILD_IMAGE)
	$(Q)mkdir -p $(objtree)/arch/$(UTS_MACHINE)/boot
	$(Q)ln -fsn ../../x86/boot/bzImage $(objtree)/arch/$(UTS_MACHINE)/boot/$@

$(BOOT_TARGETS): vmlinux
	$(Q)$(MAKE) $(build)=$(boot) $@

PHONY += install
install:
	$(call cmd,install)

vdso-install-$(CONFIG_X86_64)		+= arch/x86/entry/vdso/vdso64.so.dbg
vdso-install-$(CONFIG_X86_X32_ABI)	+= arch/x86/entry/vdso/vdsox32.so.dbg
vdso-install-$(CONFIG_COMPAT_32)	+= arch/x86/entry/vdso/vdso32.so.dbg

archprepare: checkbin
checkbin:
# CONFIG_MITIGATION_RETPOLINE内核配置配置选项, 用于缓解因 Spectre v2漏洞而引入的
# 安全风险。
# Spectre v2(Spectre Variant 2)是一个与现代CPU的分支预测(Branch Prediction)和投机
# 执行(Speculative Execution)相关的漏洞。
# 攻击者可以利用间接分支预测来推测执行非法代码路径, 从而窃取敏感数据。
#
# Retpoline 缓解措施:
# Retpoline是一种基于编译器的缓解技术, 旨在通过重新实现间接分支跳转的方式, 防止分支预测被滥用。
# 它通过限制CPU分支预测引擎的功能, 确保只有合法代码路径能够被执行, 进而缓解Spectre v2的风险。
#
# Retpoline名字来源于"Return"和"Trampoline"的结合:
# 1. 使用特殊的返回机制, 迫使投机执行返回到安全位置.
# 2. 修改编译器生成的汇编代码, 使间接分支无法被攻击.
#
# 当启用此配置项时:
# 1. 内核将通过Retpoline技术重新编译所有间接分支(比如函数指针调用或间接jmp指令)
# 2. 这使得投机执行路径受到控制, 从而缓解Spectre v2漏洞的利用
# 3. 该选项通常在CPU受Spectre v2影响时候启用, 具体情况由内核在运行时候检测
#
# 性能影响:
# 启用后可能导致性能下降, 尤其在大量使用间接分支的工作负载中. 例如:
# 1. 虚拟化(KVM等)
# 2. 系统调用密集型任务
#
# 依赖条件:
# 1. 编译器支持, GCC 7.3+ 或 Clang 6.0+
# 2. CPU是否存在Spectre v2漏洞, 内核可能在运行时候动态检测并选择性应用
ifdef CONFIG_MITIGATION_RETPOLINE
ifeq ($(RETPOLINE_CFLAGS),)
	@echo "You are building kernel with non-retpoline compiler." >&2
	@echo "Please update your compiler." >&2
	@false
endif
endif

ifdef CONFIG_UNWINDER_ORC
# Linux内核中, 堆栈展开器(unwinder)是用于分析和追踪内核函数调用栈的工具。它在以下场景中非常重要:
# 1. 调试和分析: 通过堆栈追踪识别问题代码路径(如崩溃回溯)
# 2. 性能分析: 帮助性能工具(如: perf)采集调用栈信息
# 3. 错误处理: 内核错误日志(如: oops 和 panic)中的调用栈信息依赖堆栈展开器
# 传统上, Linux内核支持多种堆栈展开方式, 如: DWAF和frame pointer(帧指针), 但他们各有优缺点.
#
# ORC堆栈展开器的介绍
# ORC(Open Runtime Code unwinder)是Linux内核团队开发的一种高效堆栈展开方式, 旨在解决传统方法的不足. 它的特点包括:
# 1. 无须帧指针: ORC不依赖帧指针展开堆栈, 从而避免了强制启用帧指针可能带来的性能损失.
# 2. 预计展开信息: ORC使用objtool工具在编译阶段生成专门的ORC数据表, 这些表存储了每个指令的堆栈元信息
# 3. 更高的可靠性: ORC能够准确展开复杂的内核堆栈, 支持异常情况(如: 中断、错误处理等)
# 4. 低运行时开销: 运行时通过查表完成堆栈展开, 而无需实时解析二进制文件.
#
# 启用此配置后, 内核将使用ORC堆栈展开器代替其他方式. 编译过程会通过objtool生成ORC数据表, 内核运行时候根据这些表进行堆栈展开.
orc_hash_h := arch/$(SRCARCH)/include/generated/asm/orc_hash.h
orc_hash_sh := $(srctree)/scripts/orc_hash.sh
targets += $(orc_hash_h)
quiet_cmd_orc_hash = GEN     $@
      cmd_orc_hash = mkdir -p $(dir $@); \
		     $(CONFIG_SHELL) $(orc_hash_sh) < $< > $@
$(orc_hash_h): $(srctree)/arch/x86/include/asm/orc_types.h $(orc_hash_sh) FORCE
	$(call if_changed,orc_hash)
archprepare: $(orc_hash_h)
endif

archclean:
	$(Q)rm -rf $(objtree)/arch/i386
	$(Q)rm -rf $(objtree)/arch/x86_64

define archhelp
  echo  '* bzImage		- Compressed kernel image (arch/x86/boot/bzImage)'
  echo  '  install		- Install kernel using (your) ~/bin/$(INSTALLKERNEL) or'
  echo  '			  (distribution) /sbin/$(INSTALLKERNEL) or install to '
  echo  '			  $$(INSTALL_PATH) and run lilo'
  echo  ''
  echo  '  fdimage		- Create 1.4MB boot floppy image (arch/x86/boot/fdimage)'
  echo  '  fdimage144		- Create 1.4MB boot floppy image (arch/x86/boot/fdimage)'
  echo  '  fdimage288		- Create 2.8MB boot floppy image (arch/x86/boot/fdimage)'
  echo  '  hdimage		- Create a BIOS/EFI hard disk image (arch/x86/boot/hdimage)'
  echo  '  isoimage		- Create a boot CD-ROM image (arch/x86/boot/image.iso)'
  echo  '			  bzdisk/fdimage*/hdimage/isoimage also accept:'
  echo  '			  FDARGS="..."  arguments for the booted kernel'
  echo  '			  FDINITRD=file initrd for the booted kernel'

endef
