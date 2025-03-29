# 系统调用

## 系统调用封装器

glibc 针对不同的系统调用使用三种操作系统内核系统调用封装器：汇编、宏和定制。

我们首先讨论汇编封装器。然后我们再谈谈其他两种。

### 汇编封装器

glibc 中简单的内核系统调用会从 syscalls.list 中保存的调用列表翻译成汇编封装器，然后再进行编译。

syscalls.list 各列含义如下：
- File name：生成系统调用目标文件的文件名
- Caller：调用者
- Syscall name：系统调用的名字
- Args：系统调用的参数类型、个数以及返回值类型(冒号前表示返回值类型, 后面表示参数类型和个数)。前缀含义：
    - E: errno和返回值不是由调用设置
    - V: errno未设置，但调用返回errno或零(成功)
    - C: 未知
- Strong name：系统调用对应函数的名字
- Weak names：系统调用对应函数的名字的别称，可以使用别称来调用函数
```
# File name  Caller   Syscall name  Args     Strong name    Weak names
accept       -        accept        Ci:iBN   __libc_accept  accept
access       -        access        i:si     __access       access
```
其中第4列，系统调用签名关键字母含义如下：
- a: 未经检查的地址(例如: mmap的第一个参数)
- b: 非空缓存区(例如: read的第2个参数, mmap的返回值)
- B: 可选的NULL缓存区(例如：getsockopt的第4个参数)
- f: 2个证书的缓存区(例如：socketpair的第4个参数)
- F: fcntl的第3个参数
- i: 标量(任何符号和大小：int、long、long long、enum，等)
- I: ioctl的第3个参数
- n: 标量缓存区长度(例如：read的第3个参数)
- N: 指向值/返回标量缓存区长度的指针(例如: recvform的第6个参数)
- p: 指向类型对象的非NULL指针(例如：任何非void* arg)
- P: 可选的指向类型对象的NULL指针(例如：sigaction的第3个参数)
- s: 非空字符串(例如：open的第一个参数)
- S: 可选的NULL字符串(例如：acct的第一个参数)
- U: unsigned long int(32位类型扩展为64位类型)
- v: vararg标量(例如：open的可选第3个参数)
- V: 每页字节向量(mincore的第3个参数)
- W: 等待状态，可选的指向int的NULL指针(例如: wait的第2个参数)

在构建目录中反汇编 socket syscall，你会看到 syscall-template.S 封装程序：
```shell
$ objdump -ldr socket/socket.o

socket/socket.o：     文件格式 elf64-x86-64


Disassembly of section .text:

0000000000000000 <__socket>:
socket():
/data/source/glibc-2.41/socket/../sysdeps/unix/syscall-template.S:120
   0:	f3 0f 1e fa          	endbr64
   4:	b8 29 00 00 00       	mov    $0x29,%eax
   9:	0f 05                	syscall
   b:	48 3d 01 f0 ff ff    	cmp    $0xfffffffffffff001,%rax
  11:	73 01                	jae    14 <__socket+0x14>
/data/source/glibc-2.41/socket/../sysdeps/unix/syscall-template.S:122
  13:	c3                   	ret
/data/source/glibc-2.41/socket/../sysdeps/unix/syscall-template.S:123
  14:	48 8b 0d 00 00 00 00 	mov    0x0(%rip),%rcx        # 1b <__socket+0x1b>
			17: R_X86_64_GOTTPOFF	__libc_errno-0x4
  1b:	f7 d8                	neg    %eax
  1d:	64 89 01             	mov    %eax,%fs:(%rcx)
  20:	48 83 c8 ff          	or     $0xffffffffffffffff,%rax
  24:	c3                   	ret
```

使用封装器的系统调用列表保存在 syscalls.list 文件中：
```
dingjing@gentoo-pc /data/source/glibc-2.41 $ find -name syscalls.list

./sysdeps/unix/bsd/syscalls.list
./sysdeps/unix/syscalls.list
./sysdeps/unix/sysv/linux/sh/syscalls.list
./sysdeps/unix/sysv/linux/powerpc/powerpc32/syscalls.list
./sysdeps/unix/sysv/linux/alpha/syscalls.list
./sysdeps/unix/sysv/linux/hppa/syscalls.list
./sysdeps/unix/sysv/linux/csky/syscalls.list
./sysdeps/unix/sysv/linux/sparc/sparc32/syscalls.list
./sysdeps/unix/sysv/linux/sparc/sparc64/syscalls.list
./sysdeps/unix/sysv/linux/syscalls.list
./sysdeps/unix/sysv/linux/m68k/syscalls.list
./sysdeps/unix/sysv/linux/arm/syscalls.list
./sysdeps/unix/sysv/linux/i386/syscalls.list
./sysdeps/unix/sysv/linux/s390/s390-32/syscalls.list
./sysdeps/unix/sysv/linux/x86_64/syscalls.list
./sysdeps/unix/sysv/linux/x86_64/x32/syscalls.list
./sysdeps/unix/sysv/linux/wordsize-64/syscalls.list
./sysdeps/unix/sysv/linux/mips/syscalls.list
./sysdeps/unix/sysv/linux/mips/mips64/n32/syscalls.list
./sysdeps/unix/sysv/linux/microblaze/syscalls.list
./sysdeps/unix/sysv/linux/arc/syscalls.list
```

sysdep 目录排序有助于决定哪些系统调用适用。因此，例如在 x86_64 上，将采用以下方式：
```
./sysdeps/unix/sysv/linux/x86_64/syscalls.list
./sysdeps/unix/sysv/linux/wordsize-64/syscalls.list
./sysdeps/unix/sysv/linux/syscalls.list
```

处理系统调用包装器的 makefile 规则在 sysdeps/unix/Makefile 中：

```
ifndef avoid-generated
$(common-objpfx)sysd-syscalls: $(..)sysdeps/unix/make-syscalls.sh \
                               $(wildcard $(+sysdep_dirs:%=%/syscalls.list)) \
                               $(common-objpfx)libc-modules.stmp
        for dir in $(+sysdep_dirs); do \
          test -f $$dir/syscalls.list && \
          { sysdirs='$(sysdirs)' \
            asm_CPP='$(COMPILE.S) -E -x assembler-with-cpp' \
            $(SHELL) $(dir $<)$(notdir $<) $$dir || exit 1; }; \
          test $$dir = $(..)sysdeps/unix && break; \
        done > $@T
        mv -f $@T $@
endif
```
syscalls.list 文件由名为 sysdep/unix/make-syscalls.sh 的脚本处理。脚本使用名为 syscall-template.S 的模板生成汇编文件，该文件使用各个系统的宏来构建系统调用的封装。最后，每中系统的宏由 sysdep.h 头文件提供：
```
./sysdeps/sh/sysdep.h
./sysdeps/powerpc/sysdep.h
./sysdeps/powerpc/powerpc64/sysdep.h
./sysdeps/powerpc/powerpc32/sysdep.h
./sysdeps/or1k/sysdep.h
./sysdeps/hppa/sysdep.h
./sysdeps/csky/sysdep.h
./sysdeps/sparc/sysdep.h
./sysdeps/m68k/coldfire/sysdep.h
./sysdeps/m68k/sysdep.h
./sysdeps/m68k/m680x0/sysdep.h
./sysdeps/generic/sysdep.h
./sysdeps/unix/sh/sysdep.h
./sysdeps/unix/powerpc/sysdep.h
./sysdeps/unix/sysdep.h
./sysdeps/unix/arm/sysdep.h
./sysdeps/unix/i386/sysdep.h
./sysdeps/unix/sysv/linux/sh/sysdep.h
./sysdeps/unix/sysv/linux/sh/sh4/sysdep.h
./sysdeps/unix/sysv/linux/powerpc/sysdep.h
./sysdeps/unix/sysv/linux/powerpc/powerpc64/sysdep.h
./sysdeps/unix/sysv/linux/loongarch/sysdep.h
./sysdeps/unix/sysv/linux/or1k/sysdep.h
./sysdeps/unix/sysv/linux/alpha/sysdep.h
./sysdeps/unix/sysv/linux/hppa/sysdep.h
./sysdeps/unix/sysv/linux/csky/sysdep.h
./sysdeps/unix/sysv/linux/sparc/sysdep.h
./sysdeps/unix/sysv/linux/sparc/sparc32/sysdep.h
./sysdeps/unix/sysv/linux/sparc/sparc64/sysdep.h
./sysdeps/unix/sysv/linux/sysdep.h
./sysdeps/unix/sysv/linux/m68k/coldfire/sysdep.h
./sysdeps/unix/sysv/linux/m68k/sysdep.h
./sysdeps/unix/sysv/linux/m68k/m680x0/sysdep.h
./sysdeps/unix/sysv/linux/arm/sysdep.h
./sysdeps/unix/sysv/linux/i386/sysdep.h
./sysdeps/unix/sysv/linux/s390/s390-32/sysdep.h
./sysdeps/unix/sysv/linux/s390/sysdep.h
./sysdeps/unix/sysv/linux/s390/s390-64/sysdep.h
./sysdeps/unix/sysv/linux/aarch64/sysdep.h
./sysdeps/unix/sysv/linux/x86_64/sysdep.h
./sysdeps/unix/sysv/linux/x86_64/x32/sysdep.h
./sysdeps/unix/sysv/linux/riscv/sysdep.h
./sysdeps/unix/sysv/linux/mips/sysdep.h
./sysdeps/unix/sysv/linux/mips/mips32/sysdep.h
./sysdeps/unix/sysv/linux/mips/mips64/sysdep.h
./sysdeps/unix/sysv/linux/microblaze/sysdep.h
./sysdeps/unix/sysv/linux/arc/sysdep.h
./sysdeps/unix/x86_64/sysdep.h
./sysdeps/unix/mips/sysdep.h
./sysdeps/unix/mips/mips32/sysdep.h
./sysdeps/unix/mips/mips64/sysdep.h
./sysdeps/arm/sysdep.h
./sysdeps/i386/sysdep.h
./sysdeps/s390/s390-32/sysdep.h
./sysdeps/s390/s390-64/sysdep.h
./sysdeps/aarch64/sysdep.h
./sysdeps/x86_64/sysdep.h
./sysdeps/x86_64/x32/sysdep.h
./sysdeps/x86/sysdep.h
./sysdeps/mach/sysdep.h
./sysdeps/mach/x86/sysdep.h
./sysdeps/microblaze/sysdep.h
./sysdeps/arc/sysdep.h
```
每个系统调用都是由 make-syscall.sh 生成的 sysd-syscalls 中的规则构建的，该规则定义了几个宏之后`#include<syscall-template.S>`：
- SYSCALL_NAME： 系统调用名称
- SYSCALL_NARGS：此调用的参数数量
- SYSCALL_ULONG_ARG_1：此调用采用的第一个无符号长整型参数。0表示没有 unsigned long int参数。通过解析Args列获取(syscalls.list)
- SYSCALL_ULONG_ARG_1：此调用采用的第二个无符号长整型参数。0表示最多有一个unsigned long int参数。通过解析Args列获取
- SYSCALL_SYMBOL：主要符号名称。从Strong name列获取
- SYSCALL_NOERRNO：1定义无错误版本，即没有出错返回。通过解析Args列设置
- SYSCALL_ERRVAL：1定义错误值版本，直接返回错误号，不是返回-1并将错误号放入errno中。通过解析Args设置

weak_alias(__socket, socket)：定义了 `__socket` 函数的别称，可以调用socket来调用 `__socket`。socket从Weak names列获取

将所有这些部件组合在一起，就能生成这样一个包装器：
```
(echo '#define SYSCALL_NAME socket'; \
 echo '#define SYSCALL_NARGS 3'; \
 echo '#define SYSCALL_ULONG_ARG_1 0'; \
 echo '#define SYSCALL_ULONG_ARG_2 0'; \
 echo '#define SYSCALL_SYMBOL __socket'; \
 echo '#define SYSCALL_NOERRNO 0'; \
 echo '#define SYSCALL_ERRVAL 0'; \
 echo '#include <syscall-template.S>'; \
 echo 'weak_alias (__socket, socket)'; \
 echo 'hidden_weak (socket)'; \
) | gcc -c    -I../include -I/data/source/glibc-2.41/build/socket  -I/data/source/glibc-2.41/build  -I../sysdeps/unix/sysv/linux/x86_64/64  -I../sysdeps/unix/sysv/linux/x86_64/include -I../sysdeps/unix/sysv/linux/x86_64  -I../sysdeps/unix/sysv/linux/x86/include -I../sysdeps/unix/sysv/linux/x86  -I../sysdeps/x86/nptl  -I../sysdeps/unix/sysv/linux/wordsize-64  -I../sysdeps/x86_64/nptl  -I../sysdeps/unix/sysv/linux/include -I../sysdeps/unix/sysv/linux  -I../sysdeps/nptl  -I../sysdeps/pthread  -I../sysdeps/gnu  -I../sysdeps/unix/inet  -I../sysdeps/unix/sysv  -I../sysdeps/unix/x86_64  -I../sysdeps/unix  -I../sysdeps/posix  -I../sysdeps/x86_64/64  -I../sysdeps/x86_64/fpu/multiarch  -I../sysdeps/x86_64/fpu  -I../sysdeps/x86/fpu  -I../sysdeps/x86_64/multiarch  -I../sysdeps/x86_64  -I../sysdeps/x86/include -I../sysdeps/x86  -I../sysdeps/ieee754/float128  -I../sysdeps/ieee754/ldbl-96/include -I../sysdeps/ieee754/ldbl-96  -I../sysdeps/ieee754/dbl-64  -I../sysdeps/ieee754/flt-32  -I../sysdeps/wordsize-64  -I../sysdeps/ieee754  -I../sysdeps/generic  -I.. -I../libio -I.  -D_LIBC_REENTRANT -include /data/source/glibc-2.41/build/libc-modules.h -DMODULE_NAME=libc -include ../include/libc-symbols.h  -DPIC     -DTOP_NAMESPACE=glibc -DASSEMBLER -fcf-protection -include cet.h -g -Werror=undef -Wa,--noexecstack      -o /data/source/glibc-2.41/build/socket/socket.o -x assembler-with-cpp - -MD -MP -MF /data/source/glibc-2.41/build/socket/socket.o.dt -MT /data/source/glibc-2.41/build/socket/socket.o 
```
> 注意使用了 `-x assembler-with-cpp`，因此这些封装器只能使用汇编。

### 宏系统调用封装

宏系统调用由 `*.c` 文件处理，这些文件要比简单的封装复杂得多。

某些系统调用可能需要将内核结果转换为用户空间结构，因此 glibc 需要一种在 C 代码中进行内联系统调用的方法。

这需要通过 sysdep.h 文件中定义的宏来处理。

这些宏都被称为 `INTERNAL_*` 和 `INLINE_*`，并提供了多种变体供源代码使用。

例如，在 wait 函数的实现（sysdeps/unix/sysv/linux/wait.c）中就可以看到这些宏的使用：
```c
/* Wait for a child to die.  When one does, put its status in *STAT_LOC
   and return its process ID.  For errors, return (pid_t) -1.  */
pid_t __libc_wait (int *stat_loc)
{
    pid_t result = SYSCALL_CANCEL (wait4, WAIT_ANY, stat_loc, 0, (struct rusage *) NULL);
    return result;
}
```

`__libc_wait` 函数调用 `SYSCALL_CANCEL` 宏，该宏定义为 (sysdeps/unix/sysdep.h)：
```c
#define SYSCALL_CANCEL(...) \
    ({                                                    \
        long int sc_ret;                                  \
        if (SINGLE_THREAD_P)                              \
            sc_ret = INLINE_SYSCALL_CALL (__VA_ARGS__);   \
        else {                                            \
            int sc_cancel_oldtype = LIBC_CANCEL_ASYNC (); \
            sc_ret = INLINE_SYSCALL_CALL (__VA_ARGS__);   \
            LIBC_CANCEL_RESET (sc_cancel_oldtype);        \
        }                                                 \
        sc_ret;                                           \
    })
```

`LIBC_CANCEL_ASYNC` 调用`__{libc,pthread,librt}_enable_asynccancel`，在调用系统调用之前原子地启用异步取消模式。在另一个句柄中，`LIBC_CANCEL_RESET` 通过调用 `__{libc,pthread,librt}_disable_asynccancel` 来原子地禁用异步取消模式，并在需要时采取相应措施。

### 定制系统调用封装

在 glibc 中，有些地方的系统调用不使用标准汇编或 C 代码宏。
最典型的例子就是 fork 和 vfork 的实现，它需要根据不同的体系结构在 Linux 上使用特定的调用约定。例如 x86_64 (sysdeps/unix/sysv/linux/x86_64/vfork.S)：

```c
ENTRY (__vfork)
    /* Pop the return PC value into RDI.  We need a register that is preserved by the syscall and that we're allowed to destroy. */
    popq    %rdi
    cfi_adjust_cfa_offset(-8)
    cfi_register(%rip, %rdi)

    /* Stuff the syscall number in RAX and enter into the kernel.  */
    movl    $SYS_ify (vfork), %eax
    syscall

    /* Push back the return PC.  */
    pushq   %rdi
    cfi_adjust_cfa_offset(8)

    cmpl    $-4095, %eax
    jae SYSCALL_ERROR_LABEL         /* Branch forward if it failed.  */

    /* Normal return.  */
    ret
PSEUDO_END (__vfork)
```

它使用 sysdep.h 宏进行函数返回（`SYSCALL_ERROR_LABEL`），但由于一些特定的 ABI 和语义限制，它需要一些特定的汇编实现。

事实上，大多数定制的情况可能都应改为使用宏包装。


## open

首先执行 `man 2 open`，可以看到函数由 `fcntl.h` 提供，然后在glibc的 `include/fcntl.h` 查看，发现其函数声明在`io/fcntl.h`中，声明如下：

```c
/**
  Open FILE and return a new file descriptor for it, or -1 on error.
  OFLAG determines the type of access used.  If O_CREAT or O_TMPFILE is set
  in OFLAG, the third argument is taken as a `mode_t', the mode of the
  created file.

  This function is a cancellation point and therefore not marked with
  __THROW. */

#ifndef __USE_FILE_OFFSET64
extern int open (const char *__file, int __oflag, ...) __nonnull ((1));
#else
# ifdef __REDIRECT
extern int __REDIRECT (open, (const char *__file, int __oflag, ...), open64) __nonnull ((1));
# else
#  define open open64
# endif
#endif
#ifdef __USE_LARGEFILE64
extern int open64 (const char *__file, int __oflag, ...) __nonnull ((1));
#endif
```

然后在`io/open.c`中找到open函数相关定义如下：
```c

/* Open FILE with access OFLAG.  If O_CREAT or O_TMPFILE is in OFLAG,
   a third argument is the file protection.  */
int __libc_open (const char *file, int oflag)
{
    int mode;

    if (file == NULL) {
        __set_errno (EINVAL);
        return -1;
    }

    if (__OPEN_NEEDS_MODE (oflag)) {
        va_list arg;
        va_start(arg, oflag);
        mode = va_arg(arg, int);
        va_end(arg);
    }

    __set_errno (ENOSYS);
    
    return -1;
}
libc_hidden_def (__libc_open)
weak_alias (__libc_open, __open)
libc_hidden_weak (__open)
weak_alias (__libc_open, open)

stub_warning (open)

/* __open_2 is a generic wrapper that calls __open.
   So give a stub warning for that symbol too.  */
stub_warning (__open_2)
```
上述代码中几个关键宏的含义如下：
- `libc_hidden_def`：glibc内部用于控制符号可见性的一部分，用于定义仅供glibc内部使用、而不希望导出到动态符号表中的符号。在`include/libc-symbols.h`中定义(阅读理解代码可直接忽略)。
- `weak_alias`：glibc内部创建符号的弱别名，使得一个符号可以通过另一个名字来引用，“弱”的含义是：如果存在同名强符号定义，则弱别名会被覆盖。这在实现api向后兼容和符号重定向时候非常有用。具体定义如下(定义在`include/libc-symbols.h`中)：
    ```c
    #define weak_alias(old, new)  \
        extern __typeof(old) new __attribute((weak, alias(#old)))
    ```
- `stub_warning`：宏用于为某个符号(通常是一个函数)添加一个“存根(stub)警告”。如果这个符号只提供了一个占位实现(存根)，那么当程序链接或运行时候，使用该符号会触发一个警告信息，提示开发者“该函数指示一个存根，不具备完整实现或不应该被使用”。常用于多平台或多配置环境中，某些函数可能在部分平台上没有实现时候使用。具体定义如下(定义在`include/libc-symbols.h`中)：
    ```c
    #define stub_warning(old, msg) \
        __asm__(".weak" #old); \
        __asm__(".section .gnu.warning." #old "\n\t" \
                ".ascii \"" msg "\"\n\t" \
                ".text")
    ```
    - `".weak" #old`：将符号old声明为弱符号，使得如果有其他强定义可以覆盖它
    - `".section .gnu.warning." #old`：切换到一个专门用来存放符号警告信息的节
    - `".ascii \"" msg "\""`：将警告字符串 msg 以 ascii 格式写入该节
    - `".txt"`：最后再切回

至此，open实现逻辑如下：
- open系统调用函数在`include/fcntl.h`中声明(实际声明在`io/fcntl.h`中)
- open函数弱引用实现在`io/open.c`中，且此函数仅仅实现了简单的函数参数检查，如果实际代码中调用open函数的此实现，则会返回`ENOSYS`且链接时候会报错
- `open`、`__open`、`__libc_open`、`__open_2`这三个函数声明实际指向了一个函数实现
- 真正的open实现并不在此实现，在不同系统中有不同实现。

接下来阅读linux中open函数的具体实现，定义在`sysdeps/unix/sysv/linux/open.c`中(可以编译时候把编译过程保存下来分析)：

```c
int __libc_open (const char *file, int oflag, ...)
{
    int mode = 0;

    // # define __OPEN_NEEDS_MODE(oflag) (((oflag) & O_CREAT) != 0)
    if (__OPEN_NEEDS_MODE (oflag)) {
        va_list arg;
        va_start (arg, oflag);
        mode = va_arg (arg, int);
        va_end (arg);
    }

    return SYSCALL_CANCEL (openat, AT_FDCWD, file, oflag, mode);
}
libc_hidden_def (__libc_open)

weak_alias (__libc_open, __open)
libc_hidden_weak (__open)
weak_alias (__libc_open, open)
```

调用过程分析如下：
1. `SYSCALL_CANCEL`(定义位置:`sysdeps/unix/sysdep.h`)
```c
// SYSCALL_CANCEL(openat, AT_FDCWD, file, oflag, mode)
# define SYSCALL_CANCEL(...)    __SYSCALL_CANCEL_CALL(__VA_ARGS__)
```
SYSCALL_CANCEL 宏主要用于包装那些既是系统调用又是线程取消点的函数调用，它的主要作用是在执行系统调用时候检查线程取消状态，从而确保在调用被取消时候能正确中断系统调用并返回相应的错误，而不会出现资源泄漏或挂起状态。
- 包装系统调用：调用前后插入取消状态检查
    - 调用前检查：检查线程是否已经有取消请求，从而避免不必要的进入系统调用阻塞状态。
    - 调用后检查：如果系统调用返回中断(例如：EINTR)且线程的取消状态为有效状态，则触发取消流程。这样就保证了在取消请求下，系统调用能够及时中断，进而让线程完成取消。
- 封装内核调用与错误处理：该宏会调用内部的取消安全系统调用包装函数(比如：`__syscall_cp`或`__syscall_cp_return`系列宏)，它们负责将系统调用的返回值转换为符合线程取消语义的返回值(例如：将EINTR转换为一个取消状态)，从而上层库函数可以按POSIX规范处理线程取消。

宏展开为：`__SYSCALL_CANCEL_CALL(openat, AT_FDCWD, file, oflag, mode)`

2. `__SYSCALL_CANCEL_CALL`
```c
#define __SYSCALL_CANCEL_CALL(...) \
  __SYSCALL_CANCEL_DISP (__SYSCALL_CANCEL, __VA_ARGS__)
```
宏展开为：`__SYSCALL_CANCEL_DISP(__SYSCALL_CANCEL, openat, AT_FDCWD, file, oflag, mode)`

3. `__SYSCALL_CANCEL_DISP`
```c
#define __SYSCALL_CANCEL_DISP(b,...) \
  __SYSCALL_CANCEL_CONCAT (b,__SYSCALL_CANCEL_NARGS(__VA_ARGS__))(__VA_ARGS__)
```

宏展开为：`__SYSCALL_CANCEL_CONCAT(__SYSCALL_CANCEL, __SYSCALL_CANCEL_NARGS(openat, AT_FDCWD, file, oflag, mode))(openat, AT_FDCWD, file, oflag, mode)`

4. 补充`__SYSCALL_CANCEL_NARGS`定义：
```c
#define __SYSCALL_CANCEL_NARGS(...) \
  __SYSCALL_CANCEL_NARGS_X (__VA_ARGS__,7,6,5,4,3,2,1,0,)

#define __SYSCALL_CANCEL_NARGS_X(a,b,c,d,e,f,g,h,n,...) n
```

5. 补充`__SYSCALL_CANCEL_CONCAT` 
```c
// __SYSCALL_CANCEL_CONCAT(__SYSCALL_CANCEL, __SYSCALL_CANCEL_NARGS(openat, AT_FDCWD, file, oflag, mode))(openat, AT_FDCWD, file, oflag, mode)
#define __SYSCALL_CANCEL_CONCAT(a,b)       __SYSCALL_CANCEL_CONCAT_X (a, b)
```

6. 补充`CANCEL_CONCAT_X`
```c
#define __SYSCALL_CANCEL_CONCAT_X(a,b)     a##b
```

宏展开为：`__SYSCALL_CANCEL4(openat, AT_FDCWD, file, oflag, mode);`

7. `__SYSCALL_CANCEL4`

```c
#define __SYSCALL_CANCEL4(name, a1, a2, a3, a4) \
  __syscall_cancel (__SSC (a1), __SSC (a2), __SSC (a3),			\
		    __SSC(a4), 0, 0, __SYSCALL_CANCEL7_ARG __NR_##name)
```
宏展开为：`__syscall_cancel (__SSC (AT_FDCWD), __SSC (file), __SSC (oflag), __SSC(mode), 0, 0, __SYSCALL_CANCEL7_ARG __NR_openat);`

8. `__syscall_cancel`

函数声明：
```c
typedef long int __syscall_arg_t;

long int __syscall_cancel (__syscall_arg_t arg1, __syscall_arg_t arg2,
            __syscall_arg_t arg3, __syscall_arg_t arg4,
            __syscall_arg_t arg5, __syscall_arg_t arg6,
            __SYSCALL_CANCEL7_ARG_DEF
            __syscall_arg_t nr) attribute_hidden;
```

函数实现：

```c
/* Called by the INTERNAL_SYSCALL_CANCEL macro, check for cancellation and
   returns the syscall value or its negative error code.  */
long int __internal_syscall_cancel (
            __syscall_arg_t a1, __syscall_arg_t a2,
            __syscall_arg_t a3, __syscall_arg_t a4,
            __syscall_arg_t a5, __syscall_arg_t a6,
            __SYSCALL_CANCEL7_ARG_DEF __syscall_arg_t nr)
{
    long int result;
    struct pthread *pd = THREAD_SELF;

    // 如果未启用cancelhandling，则直接调用系统调用，
    // 并在线程终止时以避免在执行清理处理程序时调用 __syscall_do_cancel。
    // 执行清理处理程序时调用 __syscall_do_cancel。
    int ch = atomic_load_relaxed (&pd->cancelhandling);
    if (SINGLE_THREAD_P || !cancel_enabled (ch) || cancel_exiting (ch)) {
        result = INTERNAL_SYSCALL_NCS_CALL (nr, a1, a2, a3, a4, a5, a6 __SYSCALL_CANCEL7_ARCH_ARG7);
        if (INTERNAL_SYSCALL_ERROR_P (result)) {
            return -INTERNAL_SYSCALL_ERRNO (result);
        }
        return result;
    }

    /* Call the arch-specific entry points that contains the globals markers
        to be checked by SIGCANCEL handler.  */
    // 检查是否取消调用
    result = __syscall_cancel_arch (&pd->cancelhandling, nr, a1, a2, a3, a4, a5, a6 __SYSCALL_CANCEL7_ARCH_ARG7);

    /* If the cancellable syscall was interrupted by SIGCANCEL and it has no
        side-effect, cancel the thread if cancellation is enabled.  */
    // 如果可取消的系统调用被 SIGCANCEL 中断且没有副作用，则在取消功能启用的情况下取消该线程。
    ch = atomic_load_relaxed (&pd->cancelhandling);
    /* The behaviour here assumes that EINTR is returned only if there are no
       visible side effects.  POSIX Issue 7 has not yet provided any stronger
       language for close, and in theory the close syscall could return EINTR
       and leave the file descriptor open (conforming and leaks).  It expects
       that no such kernel is used with glibc.  */
    if (result == -EINTR && cancel_enabled_and_canceled (ch))
        __syscall_do_cancel ();

    return result;
}

/**
 * Called by the SYSCALL_CANCEL macro, check for cancellation and return the
 * syscall expected success value (usually 0) or, in case of failure, -1 and
 * sets errno to syscall return value.  */
long int __syscall_cancel (__syscall_arg_t a1, __syscall_arg_t a2,
            __syscall_arg_t a3, __syscall_arg_t a4,
            __syscall_arg_t a5, __syscall_arg_t a6,
            __SYSCALL_CANCEL7_ARG_DEF __syscall_arg_t nr)
{
    int r = __internal_syscall_cancel (a1, a2, a3, a4, a5, a6, __SYSCALL_CANCEL7_ARG nr);
    return __glibc_unlikely (INTERNAL_SYSCALL_ERROR_P (r))
        ? SYSCALL_ERROR_LABEL (INTERNAL_SYSCALL_ERRNO (r))
        : r;
}


/* Called by __syscall_cancel_arch or function above start the thread
   cancellation.  */
_Noreturn void __syscall_do_cancel (void)
{
    struct pthread *self = THREAD_SELF;

    /* Disable thread cancellation to avoid cancellable entrypoints calling
      __syscall_do_cancel recursively.  We atomic load relaxed to check the
      state of cancelhandling, there is no particular ordering requirement
      between the syscall call and the other thread setting our cancelhandling
      with a atomic store acquire.

      POSIX Issue 7 notes that the cancellation occurs asynchronously on the
      target thread, that implies there is no ordering requirements.  It does
      not need a MO release store here.  */
    int oldval = atomic_load_relaxed (&self->cancelhandling);
    while (1) {
        int newval = oldval | CANCELSTATE_BITMASK;
        if (oldval == newval) {
            break;
        }
        if (atomic_compare_exchange_weak_acquire (&self->cancelhandling, &oldval, newval)) {
            break;
        }
    }

    __do_cancel (PTHREAD_CANCELED);
}
```

最后实际调用变为：`INTERNAL_SYSCALL_NCS (((long int) (AT_FDCWD)), 6, ((long int) (file)), ((long int) (oflag)), ((long int) (mode)), 0, 0, __NR_openat);`

9. `INTERNAL_SYSCALL_NCS`
```c
#define INTERNAL_SYSCALL_NCS(number, nr, args...) internal_syscall##nr (number, args)
```

进一步转为：`internal_syscall6 (((long int) (AT_FDCWD)), ((long int) (file)), ((long int) (oflag)), ((long int) (mode)), 0, 0, __NR_openat);`

10. `internal_syscall6`

```c
#define internal_syscall6(number, arg1, arg2, arg3, arg4, arg5, arg6)                  \
        ({                                                                                 \
            unsigned long int resultvar;                                                   \
            TYPEFY (arg6, __arg6)                   = ARGIFY (arg6);                       \
            TYPEFY (arg5, __arg5)                   = ARGIFY (arg5);                       \
            TYPEFY (arg4, __arg4)                   = ARGIFY (arg4);                       \
            TYPEFY (arg3, __arg3)                   = ARGIFY (arg3);                       \
            TYPEFY (arg2, __arg2)                   = ARGIFY (arg2);                       \
            TYPEFY (arg1, __arg1)                   = ARGIFY (arg1);                       \
            register TYPEFY (arg6, _a6) asm ("r9")  = __arg6;                              \
            register TYPEFY (arg5, _a5) asm ("r8")  = __arg5;                              \
            register TYPEFY (arg4, _a4) asm ("r10") = __arg4;                              \
            register TYPEFY (arg3, _a3) asm ("rdx") = __arg3;                              \
            register TYPEFY (arg2, _a2) asm ("rsi") = __arg2;                              \
            register TYPEFY (arg1, _a1) asm ("rdi") = __arg1;                              \
            asm volatile ("syscall\n\t"                                                    \
                          : "=a"(resultvar)                                                \
                          : "0"(number), "r"(_a1), "r"(_a2), "r"(_a3), "r"(_a4), "r"(_a5), \
                            "r"(_a6)                                                       \
                          : "memory", REGISTERS_CLOBBERED_BY_SYSCALL);                     \
            (long int)resultvar;                                                           \
        })
```

进一步转为：
```c
({ \
    unsigned long int resultvar; \
    TYPEFY (__NR_openat, __arg6) = ARGIFY (__NR_openat); \
    TYPEFY (0, __arg5) = ARGIFY (0); \
    TYPEFY (0, __arg4) = ARGIFY (0); \
    TYPEFY (((long int) (mode)), __arg3) = ARGIFY (((long int) (mode))); \
    TYPEFY (((long int) (oflag)), __arg2) = ARGIFY (((long int) (oflag))); \
    TYPEFY (((long int) (file)), __arg1) = ARGIFY (((long int) (file))); \
    register TYPEFY (__NR_openat, _a6) asm ("r9") = __arg6; \
    register TYPEFY (0, _a5) asm ("r8") = __arg5; \
    register TYPEFY (0, _a4) asm ("r10") = __arg4; \
    register TYPEFY (((long int) (mode)), _a3) asm ("rdx") = __arg3; \
    register TYPEFY (((long int) (oflag)), _a2) asm ("rsi") = __arg2; \
    register TYPEFY (((long int) (file)), _a1) asm ("rdi") = __arg1; \
    asm volatile ("syscall\n\t" 
            : "=a"(resultvar)
            : "0"(((long int) (AT_FDCWD))), "r"(_a1), "r"(_a2), "r"(_a3), "r"(_a4), "r"(_a5), "r"(_a6)
            : "memory", REGISTERS_CLOBBERED_BY_SYSCALL);
    (long int)resultvar;
});
```

11. 其它宏

```c
#define TYPEFY(X, name) __typeof__ (ARGIFY (X)) name
#define TYPEFY1(X) __typeof__ ((X) - (X))
#define ARGIFY(X) ((TYPEFY1 (X)) (X))
#define REGISTERS_CLOBBERED_BY_SYSCALL "cc", "r11", "cx"
```

最终转为：
```c
({
    unsigned long int resultvar; \
    __typeof__ (((__typeof__ ((__NR_openat) \
                    - (__NR_openat))) (__NR_openat))) \
            __arg6 = ((__typeof__ ((__NR_openat) \
                    - (__NR_openat))) (__NR_openat)); \
    __typeof__ (((__typeof__ ((0) \
                    - (0))) (0))) \
            __arg5 = ((__typeof__ ((0) \
                    - (0))) (0)); \
    __typeof__ (((__typeof__ ((0) \
                    - (0))) (0))) \
            __arg4 = ((__typeof__ ((0) \
                    - (0))) (0)); \
    __typeof__ (((__typeof__ ((((long int) (mode))) \
                    - (((long int) (mode))))) (((long int) (mode))))) \
            __arg3 = ((__typeof__ ((((long int) (mode))) \
                    - (((long int) (mode))))) (((long int) (mode)))); \
    __typeof__ (((__typeof__ ((((long int) (oflag))) \
                    - (((long int) (oflag))))) (((long int) (oflag))))) \
            __arg2 = ((__typeof__ ((((long int) (oflag))) \
                    - (((long int) (oflag))))) (((long int) (oflag)))); \
    __typeof__ (((__typeof__ ((((long int) (file))) \
                    - (((long int) (file))))) (((long int) (file))))) \
            __arg1 = ((__typeof__ ((((long int) (file))) \
                    - (((long int) (file))))) (((long int) (file)))); \
    register __typeof__ (((__typeof__ ((__NR_openat) - (__NR_openat))) (__NR_openat))) \
            _a6 asm ("r9") = __arg6; \
    register __typeof__ (((__typeof__ ((0) - (0))) (0))) \
            _a5 asm ("r8") = __arg5; \
    register __typeof__ (((__typeof__ ((0) - (0))) (0))) \
            _a4 asm ("r10") = __arg4; \
    register __typeof__ (((__typeof__ ((((long int) (mode))) \
                    - (((long int) (mode))))) (((long int) (mode))))) \
            _a3 asm ("rdx") = __arg3; \
    register __typeof__ (((__typeof__ ((((long int) (oflag))) \
                    - (((long int) (oflag))))) (((long int) (oflag))))) \
            _a2 asm ("rsi") = __arg2; \
    register __typeof__ (((__typeof__ ((((long int) (file))) \
                    - (((long int) (file))))) (((long int) (file))))) \
            _a1 asm ("rdi") = __arg1; \
    asm volatile ("syscall\n\t" 
            : "=a"(resultvar) 
            : "0"(((long int) (AT_FDCWD))), \
                "r"(_a1), \
                "r"(_a2), \
                "r"(_a3), \
                "r"(_a4), \
                "r"(_a5), \
                "r"(_a6) \
            : "memory", "cc", "r11", "cx"); 
    (long int)resultvar;
});
```
- `__typeof__ (((__typeof__ ((__NR_openat) - (__NR_openat))) (__NR_openat))) __arg6 = ...;`：通过`__typeof__`运算符推导`__NR_openat`的类型（通常为unsigned long），并创建一个同类型的变量`__arg6`。此模式用于确保参数类型与系统调用编号的类型一致，避免隐式类型转换错误。
- 后续`__arg5`到`__arg1`的声明类似，分别对应`openat`的`flags`、`mode`、`file`等参数，通过0或变量初始化确保类型正确
- `register ... _a1 asm ("rdi") = __arg1;`：将参数变量注册到特定寄存器（如rdi、rsi等），符合x86_64系统调用约定：
    - rdi：第一个整数参数（file，文件路径）
    - rsi：第二个整数参数（oflag，打开标志）
    - rdx：第三个整数参数（mode，权限模式）
    - r10：第四个整数参数（flags，扩展标志）
    - r8、r9
- `asm volatile ("syscall\n\t" : "=a"(resultvar) : "0"(((long int) (AT_FDCWD))), "r"(_a1), "r"(_a2), "r"(_a3), "r"(_a4), "r"(_a5), "r"(_a6) : "memory", "cc", "r11", "cx");` 内联汇编执行系统调用：
    - syscall是x86_64架构的系统调用指令
    - 接下来是返回参数和输入参数
    - "memory"：告知编译器内存可能被修改（如openat可能改变文件状态）
    - "cc"：条件码寄存器可能被修改
    - "r11", "cx"：部分寄存器可能被使用（r11用于栈帧指针，cx用于循环计数）

至此，open函数的引用层部分代码跟踪完毕！
