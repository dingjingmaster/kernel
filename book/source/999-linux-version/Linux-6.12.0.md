# Linux 6.12.0

## Makefile

### Linux Makefile 文件组成

|文件名|说明|
|:-----|:---|
|顶层Makefile|它是所有Makefile文件的核心,从总体上控制着内核的编译、连接|
|arch/$(ARCH)/Makefile|对应体系结构的Makefile,它用来决定哪些体系结构的文件参与内核的生成,并提供一些规则来生成特定的内核映像|
|scripts/Makefile.*|Makefile公用的通用规则、脚本等|
|子目录kbuild Makefiles|各级子目录的Makefile相对简单,被上一层Makefile.build调用来编译当前目录的文件|
|顶层.config|配置文件,配置内核时候生成. 所有的Makefile文件(包括顶层目录和各级子目录)都是根据.config来决定使用哪些文件的|

`scripts/Makefile.*`中有公用的通用规则, 展示如下:
|文件名|说明|
|:-----|:---|
|Makefile.build|被顶层Makefile所调用, 与各级子目录的Makefile合起来构成一个完整的Makefile文件, 定义 `.lib`、`built-in.o`以及目标文件`.o`的生成规则。这个Makefile文件生成子目录的`.lib`、`built-in.o`以及目标文件`.o`|
|Makefile.clean|被顶层Makefile所调用, 用来删除目标文件等|
|Makefile.lib|被Makefile.build所调用, 主要是对一些变量的处理, 比如说在 `obj-y` 前边加上 `obj` 目录|
|Kbuild.include|被Makefile.build所调用, 定义了一些函数, 如: `if_changed`、`if_changed_rule`、`echo-cmd`|

### 内核支持的编译目标

查看make支持的编译目标, 执行`make help`，输出如下(对关键几个地方做了翻译和注释):
```
Cleaning targets:
  clean         - Remove most generated files but keep the config and
                  enough build support to build external modules
                  执行make clean会删除大多数编译生成的文件, 但是会保存
                  根目录下的.config文件, 以及支持生成模块的关键文件.
  mrproper      - Remove all generated files + config + various backup files
                  生成所有编译生成的文件
  distclean     - mrproper + remove editor backup and patch files
                  删除所有编译生成的文件以及补丁文件

Configuration targets:
  config            - Update current config utilising a line-oriented program
                      更新配置文件, 一行一行提醒、选择
  nconfig           - Update current config utilising a ncurses menu based program
                      基于ncurses显示配置文件可选项
  menuconfig        - Update current config utilising a menu based program
  xconfig           - Update current config utilising a Qt based front-end
  gconfig           - Update current config utilising a GTK+ based front-end
  oldconfig         - Update current config utilising a provided .config as base
  localmodconfig    - Update current config disabling modules not loaded
                      except those preserved by LMC_KEEP environment variable
  localyesconfig    - Update current config converting local mods to core
                      except those preserved by LMC_KEEP environment variable
  defconfig         - New config with default from ARCH supplied defconfig
  savedefconfig     - Save current config as ./defconfig (minimal config)
  allnoconfig       - New config where all options are answered with no
  allyesconfig      - New config where all options are accepted with yes
  allmodconfig      - New config selecting modules when possible
  alldefconfig      - New config with all symbols set to default
  randconfig        - New config with random answer to all options
  yes2modconfig     - Change answers from yes to mod if possible
  mod2yesconfig     - Change answers from mod to yes if possible
  mod2noconfig      - Change answers from mod to no if possible
  listnewconfig     - List new options
  helpnewconfig     - List new options and help text
  olddefconfig      - Same as oldconfig but sets new symbols to their
                      default value without prompting
  tinyconfig        - Configure the tiniest possible kernel
                      生成最小内核的配置
  testconfig        - Run Kconfig unit tests (requires python3 and pytest)

Configuration topic targets:
  debug.config      - Debugging for CI systems and finding regressions
  hardening.config  - Basic kernel hardening options
  kvm_guest.config  - Bootable as a KVM guest
  nopm.config       - Disable Power Management
  rust.config       - Enable Rust
  x86_debug.config  - Debugging options for tip tree testing
  xen.config        - Bootable as a Xen guest

Other generic targets:
  all               - Build all targets marked with [*]
* vmlinux           - Build the bare kernel
                      生成一个未压缩的可执行Linux内核镜像文件
                      vmlinux是ELF格式的文件, 包含内核代码、数据、符号表.
                      它是内核在构建过程中最原始的形式, 未压缩或特殊处理.
                      所有内核模块代码, 包括汇编链接成此文件.
* modules           - Build all modules
                      编译模块
  modules_install   - Install all modules to INSTALL_MOD_PATH (default: /)
  vdso_install      - Install unstripped vdso to INSTALL_MOD_PATH (default: /)
                      VDSO(Virtual Dynamically-linked Shared Object, 虚拟动态链接共享库).
                      VDSO是特殊共享库,内核将其直接映射到用户进程的地址空间中,用于优化某些
                      系统调用的性能,尤其是那些频繁使用的系统调用,
                      减少内核和用户空间的上下文切换.
  dir/              - Build all files in dir and below
  dir/file.[ois]    - Build specified target only
  dir/file.ll       - Build the LLVM assembly file
                      (requires compiler support for LLVM assembly generation)
  dir/file.lst      - Build specified mixed source/assembly target only
                      (requires a recent binutils and recent build (System.map))
  dir/file.ko       - Build module including final link
  modules_prepare   - Set up for building external modules
                      完成内核编译所需的准备工作.
                      1. 生成必要的内核构建文件: 
                        Module.symvers:包含内核符号的列表,供外部模块链接
                        scripts/:编译内核需要的辅助脚本
                        必要的头文件依赖(include/generated/等)
                      2. 验证内核配置是否可用:
                        确保源码根目录的.config完整,并根据.config生成其他依赖(如:autoconfig.h)
                      3. 为外部模块编译提供支持
  tags/TAGS         - Generate tags file for editors
  cscope            - Generate cscope index
  gtags             - Generate GNU GLOBAL index
  kernelrelease     - Output the release version string (use with make -s)
  kernelversion     - Output the version stored in Makefile (use with make -s)
  image_name        - Output the image name (use with make -s)
  headers_install   - Install sanitised kernel headers to INSTALL_HDR_PATH
                      (default: ./usr)

Static analysers:
  checkstack      - Generate a list of stack hogs and consider all functions
                    with a stack size larger than MINSTACKSIZE (default: 100)
  versioncheck    - Sanity check on version.h usage
  includecheck    - Check for duplicate included header files
  export_report   - List the usages of all exported symbols
  headerdep       - Detect inclusion cycles in headers
  coccicheck      - Check with Coccinelle
  clang-analyzer  - Check with clang static analyzer
  clang-tidy      - Check with clang-tidy

Tools:
  nsdeps          - Generate missing symbol namespace dependencies

Kernel selftest:
  kselftest         - Build and run kernel selftest
                      Build, install, and boot kernel before
                      running kselftest on it
                      Run as root for full coverage
  kselftest-all     - Build kernel selftest
  kselftest-install - Build and install kernel selftest
  kselftest-clean   - Remove all generated kselftest files
  kselftest-merge   - Merge all the config dependencies of
                      kselftest to existing .config.

Rust targets:
  rustavailable     - Checks whether the Rust toolchain is
                      available and, if not, explains why.
  rustfmt           - Reformat all the Rust code in the kernel
  rustfmtcheck      - Checks if all the Rust code in the kernel
                      is formatted, printing a diff otherwise.
  rustdoc           - Generate Rust documentation
                      (requires kernel .config)
  rusttest          - Runs the Rust tests
                      (requires kernel .config; downloads external repos)
  rust-analyzer     - Generate rust-project.json rust-analyzer support file
                      (requires kernel .config)
  dir/file.[os]     - Build specified target only
  dir/file.rsi      - Build macro expanded source, similar to C preprocessing.
                      Run with RUSTFMT=n to skip reformatting if needed.
                      The output is not intended to be compilable.
  dir/file.ll       - Build the LLVM assembly file

Userspace tools targets:
  use "make tools/help"
  or  "cd tools; make help"

Kernel packaging:
  rpm-pkg             - Build both source and binary RPM kernel packages
  srcrpm-pkg          - Build only the source kernel RPM package
  binrpm-pkg          - Build only the binary kernel RPM package
  deb-pkg             - Build both source and binary deb kernel packages
  srcdeb-pkg          - Build only the source kernel deb package
  bindeb-pkg          - Build only the binary kernel deb package
  snap-pkg            - Build only the binary kernel snap package
                        (will connect to external hosts)
  pacman-pkg          - Build only the binary kernel pacman package
  dir-pkg             - Build the kernel as a plain directory structure
  tar-pkg             - Build the kernel as an uncompressed tarball
  targz-pkg           - Build the kernel as a gzip compressed tarball
  tarbz2-pkg          - Build the kernel as a bzip2 compressed tarball
  tarxz-pkg           - Build the kernel as a xz compressed tarball
  tarzst-pkg          - Build the kernel as a zstd compressed tarball
  perf-tar-src-pkg    - Build the perf source tarball with no compression
  perf-targz-src-pkg  - Build the perf source tarball with gzip compression
  perf-tarbz2-src-pkg - Build the perf source tarball with bz2 compression
  perf-tarxz-src-pkg  - Build the perf source tarball with xz compression
  perf-tarzst-src-pkg - Build the perf source tarball with zst compression

Documentation targets:
 Linux kernel internal documentation in different formats from ReST:
  htmldocs        - HTML
  texinfodocs     - Texinfo
  infodocs        - Info
  latexdocs       - LaTeX
  pdfdocs         - PDF
  epubdocs        - EPUB
  xmldocs         - XML
  linkcheckdocs   - check for broken external links
                    (will connect to external hosts)
  refcheckdocs    - check for references to non-existing files under
                    Documentation
  cleandocs       - clean all generated files

  make SPHINXDIRS="s1 s2" [target] Generate only docs of folder s1, s2
  make SPHINXDIRS="s1 s2" [target] 仅生成s1, s2目录下的文档.
  valid values for SPHINXDIRS are: 
    PCI RCU accel accounting admin-guide arch block bpf cdrom core-api
    cpu-freq crypto dev-tools devicetree doc-guide driver-api fault-injection
    fb filesystems firmware-guide fpga gpu hid hwmon i2c iio infiniband
    input isdn kbuild kernel-hacking leds livepatch locking maintainer
    mhi misc-devices mm netlabel networking pcmcia peci power process rust
    scheduler scsi security sound spi staging target tee timers tools trace
    translations usb userspace-api virt w1 watchdog wmi

  make SPHINX_CONF={conf-file} [target] use *additional* sphinx-build
  configuration. This is e.g. useful to build with nit-picking config.

  make DOCS_THEME={sphinx-theme} selects a different Sphinx theme.

  make DOCS_CSS={a .css file} adds a DOCS_CSS override file for html/epub output.

  Default location for the generated documents is Documentation/output

Architecture-specific targets (x86):
* bzImage         - Compressed kernel image (arch/x86/boot/bzImage)
                    生成一个可引导的压缩内核镜像文件, 用于系统启动.
                    bzImage是指"big zImage",即一种大于传统zImage的内核压缩格式.
                    1. 创建可引导的内核镜像: 包含了启动代码、内核自身、压缩/解压缩信息.
                       它可以被引导加载程序(如: grub)加载,并启动.
                    2. 支持更大的内核: 与传统的zImage相比, bzImage能支持更大的内核镜像,
                       因为它解决了早期BIOS启动模式中内核镜像大小受限的问题.
                    3. 适配启动引导流程: bzImage包含了一个自解压模块, 在启动时候, bzImage
                       会被加载到内存中, 解压之后启动内核.
                    bzImage由vmlinux压缩+引导加载所需的启动代码生成.
  install         - Install kernel using (your) ~/bin/installkernel or
                    (distribution) /sbin/installkernel or install to 
                    $(INSTALL_PATH) and run lilo

  fdimage         - Create 1.4MB boot floppy image (arch/x86/boot/fdimage)
  fdimage144      - Create 1.4MB boot floppy image (arch/x86/boot/fdimage)
  fdimage288      - Create 2.8MB boot floppy image (arch/x86/boot/fdimage)
  hdimage         - Create a BIOS/EFI hard disk image (arch/x86/boot/hdimage)
  isoimage        - Create a boot CD-ROM image (arch/x86/boot/image.iso)
                    bzdisk/fdimage*/hdimage/isoimage also accept:
                    FDARGS="..."  arguments for the booted kernel
                    FDINITRD=file initrd for the booted kernel

  i386_defconfig    - Build for i386
  x86_64_defconfig  - Build for x86_64

  make V=n   [targets] 1: verbose build
                       2: give reason for rebuild of target
                       V=1 and V=2 can be combined with V=12
  make O=dir [targets] Locate all output files in "dir", including .config
  make C=1   [targets] Check re-compiled c source with $CHECK
                       (sparse by default)
  make C=2   [targets] Force check of all c source with $CHECK
  make RECORDMCOUNT_WARN=1 [targets] Warn about ignored mcount sections
  make W=n   [targets] Enable extra build checks, n=1,2,3,c,e where
        1: warnings which may be relevant and do not occur too often
        2: warnings which occur quite often but may still be relevant
        3: more obscure warnings, can most likely be ignored
        c: extra checks in the configuration stage (Kconfig)
        e: warnings are being treated as errors
        Multiple levels can be combined with W=12 or W=123

Execute "make" or "make all" to build all targets marked with [*] 
For further info see the ./README file
```
### zImage和bzImage

1. zImage必须将整个内核加载到低于640KB(第一个1MB的低端内存中的常规内存区域)的空间中。这是因为传统的BIOS使用实模式启动，只能直接访问这部分内存。当内核的代码和数据变得更大时候（超过约512KB），zImage无法适应。
2. zImage不适用于复杂引导需求：zImage的设计较简单，缺乏对现代启动场景(如: 大型模块化内核或现代硬件平台)的支持，尤其在系统配置较复杂时候。
3. zImage的解压操作发生在内存中的固定位置，如果内核变得更大，解压位置可能与自身代码或数据冲突。
4. bzImage将解压后的内核加载到高内存(>1MB)区域，而不是限制在640KB以下的低内存中。不过，bzImage在启动时候仍然使用低内存加载解压器。
5. bzImage摆脱低内存的大小限制，能够支持更大的内核映像，适合现代内核的代码和数据要求。
6. bzImage引入了更复杂的启动代码，能更好的与现代引导程序（如: grub或lilo）配合使用。它兼容多种硬件平台和现代BIOS/UEFI，引导流程更加稳定和通用。
7. bzImage在低内存中仅加载引导代码(bootstrap code)，解压后的内核被放置在高内存区域，从而避免了zImage的解压位置冲突问题。

### 从顶层Makefile梳理内核编译过程

1. Makefile中第一个目标 `__all`, 它是make的默认目标
2. 默认目标`__all`第一个依赖`__sub-make`, 初始化make的环境变量
3. `__all`第二个依赖, 如果编译的是内核模块(根据命令行M=判断,非内核源码目录中), 则依赖目标为`modules`, 否则依赖目标为`all`
4. 接下来`all`的依赖目标分别是: `vmlinux`、`dtbs`(如果选择了相关配置)、`modules`、`misc-check`、`scripts_gdb`(如果选择了GDB)
5. 在此之前这里需要关注几个文件: `scripts/subarch.include`使用`uname -m`设置`SUBARCH`变量, 根据`SUBARCH`发现包含的另一个文件`arch/x86/Makefile`加入另一个`all`的依赖项是`bzImage`
6. 至此`all`的所有所有依赖项列出: `vmlinux`、`dtbs`(默认没有,除非选择相关配置)、`modules`、`misc-check`、`scripts_gdb`(默认没有,除非选择相关配置)、`bzImage`(在arch/x86/Makefile中)

接下来分析`all`对应依赖目标之间的依赖关系(不分析非默认目标):

- `bzImage`     依赖 `vmlinux`
- `misc-check`  无依赖
- `modules`     依赖 `modules_prepare`, `modules_prepare` 依赖 `prepare`, `prepare` 依赖 `prepare0`, `prepare0` 依赖 `archprepare`
- `vmlinux`     依赖 ...(依赖有点多、下一步详细说明)

> 由此可以梳理出具体目标的查看顺序: `misc-check`  -->  `archprepare`  -->  `prepare0`  -->  `prepare`  -->  `modules_prepare`  -->  `modules`  -->  `vmlinux`  -->  `bzImage`

接下来依次执行`all`对应的依赖目标即可编译生成内核和模块, 下面分析每个依赖的目标(不分析非默认目标)：

#### misc-check

`misc-check`这个目标执行 `$(srctree)/scripts/misc-check` shell脚本，检查被`.gitignore`忽略且未追踪、或者忽略且已被暂存的文件。

> 没发现这个目标对内核编译有啥影响，只是多个提醒吧

#### archprepare

archprepare 依赖: `checkbin`、`$(srctree)/arch/x86/include/generated/asm/orc_hash.h`、`$(srctree)/arch/x86/include/asm/orc_types.h`

使用脚本`$(srctree)/scripts/orc_hash.sh`

这里使用ORC和Retpoline 这两种技术增加安全性

> 留待后续填坑 ...

#### prepare

`prepare`  -->  `prepare0` 和 `headers`

其中`headers`目标执行的操作:
- `include/uapi`
- `arch/x86/include/uapi`

其中`prepare0`目标执行的操作:
- make script/mod, 生成:  mk_elfconfig modpost, 两个ELF文件

其中`prepare`默认没有执行具体操作

#### mdoules_prepare
- make $(build)=scripts scripts/module.lds

用于生成链接脚本文件`scripts/module.lds`, 此文件用于描述内核模块的链接布局. 内核模块需要使用特定的链接布局(比如符号表的组织和对齐方式), 以保证加载到内核时候能够正确运行.

相关规则由 `scripts/Makefile.build` 和 `scripts/genksyms` 等工具生成

#### modules

modules 没有执行具体操作

#### vmlinux
- 目标: private
- 目标: vmlinux_o, 依赖: `vmlinux.a` 和 每个目录下的 `lib.a` 文件, 其中`lib.a`则是每个模块编译选择'y'时候生成的静态库
- 目标: vmlinux_o、odules.builtin.modinfo、modules.builtin 依赖 vmlinux_o
- 目标: modpost (make -f $(srctree)/scripts/Makefile.modpost)
- 目标: `vmlinux.lds`
- 最后执行: make -f $(srctree)/scripts/Makefile.vmlinux

> 内核依赖lds: `arch/x86/kernel/vmlinux.lds`

接下来看: `vmlinux.a`、所有的`lib.a`、最后看vmlinux_o、modules.builtin.modinfo、modules.builtin

##### vmlinux.a

```
vmlinux.a:$(KBUILD_VMLINUX_OBJS) scripts/head-object-list.txt FORCE
    $(call if_changed,ar_vmlinux.a)
```
在 `scripts/head-object-list.txt` 中保留所有CPU结构的 head.o 文件的路径.

然后执行 cmd_ar_vmlinux.a 这个`shell`命令, 命令对应如下:

```shell
cmd_ar_vmlinux.a = \
    rm -f $@; \
    $(AR) cDPrST $@ $(KBUILD_VMLINUX_OBJS); \
    $(AR) mPiT $$($(AR) t $@ | sed -n 1p) $@ $$($(AR) t $@ | grep -F -f $(srctree)/scripts/head-object-list.txt)
```

ar 中的配置选项解释:
- c: 创建归档文件
- D: 操作时候不对文件进行排序("deterministic mode")
- P: 使用文件的完整路径存储(如果适用)
- r: 添加文件到归档中. 如果文件已经存在, 则替换.
- S: 不写入符号表.
- T: 操作不适用临时文件
- m: 移动归档成员, 重新调整他们在归档中的位置
- i: 将指定的文件插入到某个位置之前
- t: 列出归档文件中的所有成员文件名称

总结:
1. 创建静态库:
```
$(AR) cDPrST $@ $(KBUILD_VMLINUX_OBJS);
```
2. 调整静态库中对象的顺序, 以满足特定的内核链接顺序要求.
```
$(AR) mPiT $$($(AR) t $@ | sed -n 1p) $@ $$($(AR) t $@ | grep -F -f $(srctree)/scripts/head-object-list.txt)
```

##### vmlinux_o

```
make -f $(srctree)/scripts/Makefile.vmlinux_o
```
依次执行如下几条命令:

```
# 此目标为: modules.builtin
# 依赖modules.builtin.modinfo
cmd_modules_builtin = \
    tr '\0' '\n' < $< | \
    sed -n 's/^[[:alnum:]:_]*\.file=//p' | \
    tr ' ' '\n' | uniq | sed -e 's:^:kernel/:' -e 's/$$/.ko/' > $@

# 此目标为: modules.builtin.modinfo
# 依赖 vmlinux.o
# OBJCOPY = objcopy
# OBJCOPYFLAGS := -S --remove-section __ex_table
#  -S: 去除符号表和调试信息(保留最小的目标文件)
#  --remove-section __ex_table: 从目标文件中移除名为 __ex_table 的段(section)
#  __ex_table段的作用: 它存储异常处理相关信息, 用于处理内核中的异常(如内核oops或故障恢复).
#  $(@F) 表示当前目标文件的文件名(不含路径), 内核根据不同文件应用不同的 objcopy 标志
cmd_objcopy = \
    $(OBJCOPY) $(OBJCOPYFLAGS) $(OBJCOPYFLAGS_$(@F)) $< $@

# 生成链接脚本
cmd_gen_initcalls_lds = \
    $(PYTHON3) $(srctree)/scripts/jobserver-exec \
    $(PERL) $(real-prereqs) > $@
# 等价于
cmd_gen_initcalls_lds = \
    python3 $(srctree)/scripts/jobserver-exec \
    perl $^ $@

# .tmp_initcalls.lds 生成命令: 
# python3 $(srctree)/scripts/jobserver-exec perl \
    $(srctree)/scripts/generate_initcall_order.pl \
    vmlinux.a *.a \
    > .tmp_initcalls.lds

# 此目标为：vmlinux.o
# 依赖: $(initcalls-lds) vmlinux.a $(KBUILD_VMLINUX_LIBS)
# 这段代码是Linux内核构建过程中生成vmlinux文件的链接指令, 它描述了 cmd_ld_vmlinux.o 的功能和目的。
cmd_ld_vmlinux.o = \
    $(LD) $(KBUILD_LDFLAGS) -r -o $@ \
    $(vmlinux-o-ld-args-y) \
    $(addprefix -T, $(initcalls-lds)) \
    --whole-archive vmlinux.a --no-whole-archive \
    --start-group $(KBUILD_VMLINUX_LIBS) --end-group \
    $(cmd_objtool)
```
- `$(LD) $(KBUILD_LDFLAGS) -r -o $@` 调用链接程序, 进行部分链接, 生成一个中间文件: `cmd_ld_vmlinux.o`. 
- `$(vmlinux-o-ld-args-y)` 控制具体的链接过程. 
- `$(addprefix -T, $(initcalls-lds)) 指定链接脚本文件的路径(-T)`
- `--whole-archive vmlinux.a`强制将存档文件(vmlinux.a)中所有对象文件包含到最终的链接结果中, 无论是否有未解决的符号依赖.
- `--no-whole-archive` 之后的内容恢复正常的存档处理方式
- `--start-group $(KBUILD_VMLINUX_LIBS) --end-group`: 通过 `--start-group` 和 `--end-group` 将一组静态库组合起来, 解决相互依赖的问题. 通常来说 $(KBUILD_VMLINUX_LIBS) 包含多个静态库 (例如: 内核驱动模块、内存管理模块等).
- `$(cmd_objtool)`: 调用内核的 objtool 工具, 检查目标文件的正确性(比如: 调用栈验证、CFI检查等)

以上构建意义：
1. 生成完整内核映像, 将所有的内核组件(目标文件、静态库等)链接为一个完整的 ELF格式 的可执行文件 vmlinux, 这是内核最终运行的基础.
2. 管理依赖与布局, 使用链接脚本`$(initcalls-lds)` 和 `--start-group/--end-group` 选项确保不同模块的正确初始化和内存布局
3. 提高内核可靠性, 借助 objtool, 在构建过程中自动验证目标文件, 避免潜在的链接错误或运行时问题.
4. 支持自定义与扩展, 通过KBUILD_*和vmlinux.a等机制, 构建过程具备高度的灵活性, 允许开发者根据需求定制内核.

##### vmlinux

```
vmlinux: private vmlinux.o $(KBUILD_LDS) modpost
    make -f $(srctree)/scripts/Makefile.vmlinux

# $(srctree)/scripts/Makefile.vmlinux
# KBUILD_LDS = arch/x86/kernel/vmlinux.lds
# KBUILD_LDFLAGS = .... 源码目录 grep 查看
# ARCH_POSTLINK = $(srctree)/arch/x86/Makefile.postlink
vmlinux: scripts/link-vmlinux.sh vmlinux.o $(KBUILD_LDS)
    $< ld $(KBUILD_LDFLAGS) $(LDFLAGS_vmlinux); \
    $(if $(ARCH_POSTLINK), make -f $(ARCH_POSTLINK) $@, true)

# postlink 中执行的命令(按需执行)
```
#### bzImage

```
bzImage: vmlinux
    make -f $(srctree)/scripts/Makefile.build obj=arch/x86/boot arch/x86/boot/bzImage
    mkdir -p $(objtree)/arch/x86_64/boot
    ln -fsn ../../x86/boot/bzImage $(objtree)/arch/x86_64/boot/bzImage

# bzImage生成的主要操作
```
