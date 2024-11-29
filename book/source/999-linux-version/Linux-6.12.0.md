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

使用脚本`$(srctree)/scripts/orc_hash.sh ` 
