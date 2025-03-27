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

`sysdeps/unix/sysv/linux/open.c`

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
1. `SYSCALL_CANCEL`
```c
// SYSCALL_CANCEL(openat, AT_FDCWD, file, oflag, mode)
# define SYSCALL_CANCEL(...)    __SYSCALL_CANCEL_CALL(__VA_ARGS__)
```
2. `__SYSCALL_CANCEL_CALL`
```c
// SYSCALL_CANCEL_CALL(openat, AT_FDCWD, file, oflag, mode)
#define __SYSCALL_CANCEL_CALL(...) \
  __SYSCALL_CANCEL_DISP (__SYSCALL_CANCEL, __VA_ARGS__)
```
3. `__SYSCALL_CANCEL_DISP`
```c
#define __SYSCALL_CANCEL_DISP(b,...) \
  __SYSCALL_CANCEL_CONCAT (b,__SYSCALL_CANCEL_NARGS(__VA_ARGS__))(__VA_ARGS__)
```
4. `__SYSCALL_CANCEL_CONCAT` 
```c
#define __SYSCALL_CANCEL_CONCAT(a,b)       __SYSCALL_CANCEL_CONCAT_X (a, b)
```
5. `CANCEL_CONCAT_X`
```c
__SYSCALL_CANCEL_CONCAT_X(a,b)     a##b
```
