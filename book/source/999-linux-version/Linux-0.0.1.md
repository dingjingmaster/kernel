# Linux 0.0.1

## 代码结构

## 代码各模块说明

### Makefile

分别生成 boot、system、build 三个二进制，最后用 build 整合 boot 和 system 生成镜像 Image。

- boot 由 boot/head.s 生成
- system 由 boot/head.s init/main.c kernel/kernel.c mm/mm.c fs/fs.c lib/lib.a 生成

使用 `make run` 可以在装有 `qemu-system-i386` 的机器上加载运行 Image 镜像。

```
#
# Makefile for linux.
# If you don't have '-mstring-insns' in your gcc (and nobody but me has :-)
# remove them from the CFLAGS defines.
#

AS86	=as86 -0
CC86	=cc86 -0
LD86	=ld86 -0

AS		=as --32 
LD		=ld -m  elf_i386 
LDFLAGS	=-M -Ttext 0 -e startup_32
CC		=gcc
CFLAGS	=-Wall -O -std=gnu89 -fstrength-reduce -fomit-frame-pointer -fno-stack-protector -fno-builtin -g -m32
CPP		=gcc -E -nostdinc -Iinclude

ARCHIVES=kernel/kernel.o mm/mm.o fs/fs.o
LIBS	=lib/lib.a

.c.s:
	$(CC) $(CFLAGS) \
	-nostdinc -Iinclude -S -o $*.s $<
.s.o:
	$(AS) --32 -o $*.o $<
.c.o:
	$(CC) $(CFLAGS) \
	-nostdinc -Iinclude -c -o $*.o $<

all:	Image

Image: boot/boot tools/system tools/build
	objcopy  -O binary -R .note -R .comment tools/system tools/system.bin
	tools/build boot/boot tools/system.bin > Image
#	sync

tools/build: tools/build.c
	$(CC) $(CFLAGS) \
	-o tools/build tools/build.c
	#chmem +65000 tools/build

boot/head.o: boot/head.s

tools/system:	boot/head.o init/main.o \
		$(ARCHIVES) $(LIBS)
	$(LD) $(LDFLAGS) boot/head.o init/main.o \
	$(ARCHIVES) \
	$(LIBS) \
	-o tools/system > System.map
	
kernel/kernel.o:
	(cd kernel; make)

mm/mm.o:
	(cd mm; make)

fs/fs.o:
	(cd fs; make)

lib/lib.a:
	(cd lib; make)

boot/boot:	boot/boot.s tools/system
	(echo -n "SYSSIZE = (";stat -c%s tools/system \
		| tr '\012' ' '; echo "+ 15 ) / 16") > tmp.s	    # 计算系统大小，给boot.s中 SYSSIZE 变量赋值
	cat boot/boot.s >> tmp.s 
	$(AS86) -o boot/boot.o tmp.s
	rm -f tmp.s
	$(LD86) -s -o boot/boot boot/boot.o
	
run:
	qemu-system-i386 -drive format=raw,file=Image,index=0,if=floppy -boot a -hdb hd_oldlinux.img -m 8 -machine pc-i440fx-2.5

run-curses:
	qemu-system-i386 -display curses -drive format=raw,file=Image,index=0,if=floppy -boot a -hdb hd_oldlinux.img -m 8 -machine pc-i440fx-2.5
	
dump:
	objdump -D --disassembler-options=intel tools/system > System.dum

clean:
	rm -f Image System.map tmp_make boot/boot core
	rm -f init/*.o boot/*.o tools/system tools/build tools/system.bin
	(cd mm;make clean)
	(cd fs;make clean)
	(cd kernel;make clean)
	(cd lib;make clean)

backup: clean
	(cd .. ; tar cf - linux | compress16 - > backup.Z)
#	sync

dep:
	sed '/\#\#\# Dependencies/q' < Makefile > tmp_make
	(for i in init/*.c;do echo -n "init/";$(CPP) -M $$i;done) >> tmp_make
	cp tmp_make Makefile
	(cd fs; make dep)
	(cd kernel; make dep)
	(cd mm; make dep)

### Dependencies:
init/main.o : init/main.c include/unistd.h include/sys/stat.h \
  include/sys/types.h include/sys/times.h include/sys/utsname.h \
  include/utime.h include/time.h include/linux/tty.h include/termios.h \
  include/linux/sched.h include/linux/head.h include/linux/fs.h \
  include/linux/mm.h include/asm/system.h include/asm/io.h include/stddef.h \
  include/stdarg.h include/fcntl.h 

```

### boot/boot.s

工作流程如下：
1. 把自己boot代码复制到 0x90000
2. 输出 Loading System... 字符串
3. 读取系统，放到 0x10000，关闭磁盘
4. 读取光标位置，禁用中断
5. 把系统移动到 0x00000
6. 加载idt和gdt(gdt可用，但是idt还没设置)
7. 开启A20
8. 配置两个中断控制器，并屏蔽中断控制器
9. 开启保护模式
10. 跳转到读取的系统部分——boot/heade.s + init/main.o + kernel/kernel.o + mm/mm.o fs/fs.o lib/lib.o

### boot/head.s

1. 设置段寄存器：DS、ES、FS、GS、加载栈段寄存器
2. 初始化 IDT、GDT
3. 重新加载段寄存器：DS、ES、FS、GS、加载栈段寄存器
4. 检测A20是否开启，没开启则开启
5. 初始化分页机制
6. 调用 main 函数

### init/main.c

1. 获取BIOS时间，把启动时间设置为BIOS时间
2. 初始化tty相关设备：初始化RS232芯片(初始化状态、添加中断)、添加键盘中断
3. 添加 trap 中断
4. 初始化任务控制模块：设置TSS描述符、设置LDT描述符、初始化任务链表、添加定时器中断、添加系统调用中断
5. 初始化磁盘模块：初始化磁盘读写操作链表、初始化磁盘中断
6. 打开中断
7. 切换到用户模式
8. fork 子进程并在子进程中打开`/dev/tty0`通过`execve`调用 `/bin/sh` 进程

至此，用户可以通过打开的 `/bin/sh` 与系统进行交互

### tools/build

build 命令用于整合 boot 和 system

```
#include <stdio.h>  /* fprintf */
#include <stdlib.h> /* contains exit */
#include <sys/types.h>  /* unistd.h needs this */
#include <unistd.h> /* contains read/write */
#include <fcntl.h>
#include <string.h>

#define MINIX_HEADER 32
#define GCC_HEADER 1024

void die(char * str)
{
    fprintf(stderr,"%s\n",str);
    exit(1);
}

void usage(void)
{
    die("Usage: build boot system [> image]");
}

/**
 * @brief
 *  打开并读取 boot 和 system.bin 
 */
int main(int argc, char ** argv)
{
    int i,c,id;
    char buf[1024];

    if (argc != 3) {
        usage();
    }

    for (i = 0; i < sizeof(buf); i++) {
        buf[i] = 0;
    }

    if ((id = open(argv[1],O_RDONLY, 0)) < 0) {
        die("Unable to open 'boot'");
    }

    if (read(id, buf, MINIX_HEADER) != MINIX_HEADER) {
        die("Unable to read header of 'boot'");
    }

    if (((long *) buf)[0] != 0x04100301) {
        die("Non-Minix header of 'boot'");
    }

    if (((long *) buf)[1] != MINIX_HEADER) {
        die("Non-Minix header of 'boot'");
    }

    if (((long *) buf)[3] != 0) {
        die("Illegal data segment in 'boot'");
    }

    if (((long *) buf)[4] != 0) {
        die("Illegal bss in 'boot'");
    }

    if (((long *) buf)[5] != 0) {
        die("Non-Minix header of 'boot'");
    }

    if (((long *) buf)[7] != 0) {
        die("Illegal symbol table in 'boot'");
    }

    i = read(id, buf, sizeof buf);
    fprintf(stderr,"Boot sector %d bytes.\n",i);

    if (i > 510) {
        die("Boot block may not exceed 510 bytes");
    }

    buf[510] = 0x55;
    buf[511] = 0xAA;
    i = write(1, buf, 512);
    if (i != 512) {
        die("Write call failed");
    }
    close (id);

    if ((id = open(argv[2], O_RDONLY, 0)) < 0) {
        die("Unable to open 'system'");
    }

    for (i = 0 ; (c = read(id, buf, sizeof buf)) > 0; i += c) {
        if (write(1, buf, c) != c) {
            die("Write call failed");
        }
    }

    /* only needed by qemu. ( qemu may not read last sector if
     * size is < 512 bytes ) 
     */
    memset(buf, 0, 512);
    //write(1,buf,512); 

    close(id);

    fprintf(stderr, "System %d bytes.\n", i);

    return (0);
}
```

## 其它重点模块说明

### 进程调度模块

进程调用模块主要数据结构
```c
typedef int (*fn_ptr)();

/**
 * @brief 此结构体是Linux内核中用来描述x86架构上的FPU(浮点运算单元)
 *  和MMX/AVX/SSE等扩展状态的结构体。
 *
 *  它主要用于保存和恢复与浮点计算和SIMD指令相关的CPU寄存器状态。
 *
 * 在上下文切换或信号处理时候，操作系统需要保存线程或进程的CPU寄存器状态，
 * 以便在后续恢复运行时候能正确执行。
 * 这些状态包括了FPU/MX/SSE寄存器，而 struct i387_struct 是用来存储这些状态的
 */
struct i387_struct
{
    long    cwd;            // 控制字
    long    swd;            // 状态字
    long    twd;            // 标记字
    long    fip;            // 指令指针
    long    fcs;            // 代码段
    long    foo;            // 操作数偏移
    long    fos;            // 操作数段选择器
    long    st_space[20];   // FPU寄存器的状态 
                            /* 8*10 bytes for each FP-reg = 80 bytes */
};

/**
 * @brief 用于描述x86架构的任务状态段(Task State Segment, TSS)。
 * TSS 是x86硬件提供的一种机制，用于存储任务的硬件状态信息，
 * 帮助CPU在任务切换和异常处理的时候快速保存和恢复状态
 *
 * 1. 每个CPU对应一个TSS：在Linux中，每个CPU通常会有一个独立的TSS，
 *    用于处理该CPU的异常堆栈切换
 * 2. 在现代 x86_64 中，sp0字段被设置为内核线程在内核栈顶地址，
 *    供用户态向内核态的切换时使用
 * 3. 当发生中断或系统调用时，CPU根据TSS切换到内核栈(sp0)，
 *    然后进入内核的中断处理程序
 * 4. 如果发生了 Double Fault(例如栈已出)，TSS中定义的错误处理堆栈会被使用，
 *    以确保错误处理程序有足够的栈空间
 *
 * @note 现代操作系统已经用软件实现任务切换，
 *   但是在某些关键场景还有用(中断、双重错误处理)
 */
struct tss_struct 
{
    long    back_link;      // 16 high bits zero
    long    esp0;           // 特权级 0 的堆栈指针
    long    ss0;            // 16 high bits zero */
    long    esp1;           // 特权级 1 的堆栈指针
    long    ss1;            /* 16 high bits zero */
    long    esp2;           // 特权级 2 的堆栈指针
    long    ss2;            /* 16 high bits zero */
    long    cr3;            // 页表地址
    long    eip;            // 指令指针
    long    eflags;         // 标志寄存器
    long    eax,ecx,edx,ebx;
    long    esp;
    long    ebp;
    long    esi;
    long    edi;
    long    es;             /* 16 high bits zero */
    long    cs;             /* 16 high bits zero */
    long    ss;             /* 16 high bits zero */
    long    ds;             /* 16 high bits zero */
    long    fs;             /* 16 high bits zero */
    long    gs;             /* 16 high bits zero */
    long    ldt;            // 局部描述符选择子 16 high bits zero
    long    trace_bitmap;   // 位图偏移量 bits: trace 0, bitmap 16-31 */
    struct i387_struct i387;
};

/**
 * @brief 是内核中描述进程的核心数据结构，
 *   包含与进程管理、调度、内存管理、文件系统相关的几乎所有信息。
 * 每个进程都有一个对应的 task_struct 实例，它是Linux进程管理的基础
 */
struct task_struct 
{
/* these are hardcoded - don't touch */
    long state;                                         // 进程状态
                                                        // -1 unrunnable
                                                        // 0  runnable
                                                        // >0 stopped
    long counter;                                       // 任务的动态时间片，
                                                        // 用于表示当前任务还能运行的剩余时间片
    long priority;                                      // 优先级
    long signal;                                        // 用来管理与信号有关的数据和操作。
                                                        // 它指向一个
                                                        //  struct signal_struct的指针
    fn_ptr sig_restorer;                                // 此字段是一个与用户态信号处理机
                                                        //  制相关的指针字段，
                                                        //  用于指向用户态的信号恢复函数
                                                        //  用于帮助用户空间的信号处理程序
                                                        //  在完成信号处理后
                                                        //  正确返回到信号触发前的执行状态
    fn_ptr sig_fn[32];                                  // 信号处理函数的指针
/* various fields */
    int exit_code;                                      // 故名思义
    unsigned long end_code,end_data,brk,start_stack;
    long pid,father,pgrp,session,leader;                // 
    unsigned short uid,euid,suid;                       // 顾名思义
    unsigned short gid,egid,sgid;                       // 顾名思义
    long alarm;                                         // 用来管理与进程相关的定时器机制。
                                                        // alarm用来实现
                                                        //  与alarm系统调用相关的功能，
                                                        // 它表示当前任务设置的
                                                        //  定时器在某个时间点触发
                                                        // 此字段在现代Linux内核中已经被废弃
    long utime,stime,cutime,cstime,start_time;          // stime: 进程在用户态和内核态运行时间
    unsigned short used_math;                           // 用于标识一个任务是否曾经使用过
                                                        //  浮点运算单元(FPU)
                                                        //  或SIMD扩展(如MMX、SSE、AVX等)。
                                                        // 这是内核中 任务上下文切换 
                                                        //  和 FPU状态管理 的一部分
/* file system info */
    int tty;                                            // -1 if no tty, so it must be signed
    unsigned short umask;                               // 故名思义
    struct m_inode * pwd;                               // 进程工作目录
    struct m_inode * root;                              // 表示与进程相关的根文件系统
                                                        //  (root filesystem)的目录信息。
                                                        // 它用于描述一个任务的文件系统视图
                                                        //  尤其是在支持chroot或容器环境时候
                                                        //  这一字段尤为重要。
    unsigned long close_on_exec;
    struct file * filp[NR_OPEN];                        // 进程打开的文件
/* ldt for this task 0 - zero 1 - cs 2 - ds&ss */
    struct desc_struct ldt[3];                          //
/* tss for this task */
    struct tss_struct tss;
};
```

### 进程调用过程

要分析进程调用过程，先回到系统调用过程中的 `sched_init` 函数中，开始梳理其程序实现逻辑。
1. `set_tss_desc`在`GDT`中添加`TSS`入口
2. `set_ldt_desc`在`GDT`中添加`LDT`入口
3. 依次初始化所有任务描述结构体(`struct task_struct`)
4. 将任务寄存器(Task Register, TR)加载为指定的任务状态段(TSS)。任务寄存器用于管理任务切换时的上下文切换数据
5. 设置 8253/8254 定时器，使其开始生成固定频率的信号
6. IDT中设置定时器的中断例程
7. 设置 8259 中断控制器，启用时钟中断对应的 IRQ0 中断
8. 中断向量表中设置系统调用中断服务例程

当定时器触发中断服务 `timer_interrupt` 执行逻辑如下：
1. 保存现场，保存：DS、ES、FS、EDX、ECX、EBX、EAX
2. DS、ES 指向 0x10 内存段；FS 指向 0x17 内存段
3. 全局变量 jiffies(记录系统启动时间)
4. 通知中断控制器、中断已经处理结束，允许PIC处理下一个中断请求
5. 获取当前CS段中特权位，根据特权位判断当前中断或系统调用的来源
6. 把当前特权级位传给 `do_timer`，并调用`do_timer`处理进程管理。
7. 如果特权级别是内核，则当前进程内核运行时间+1，否则当前进程用户运行时间+1
8. 当前进程时间片减1，如果减1后值为0，则继续执行此任务(进程)，否则判断当前进程如果是内核级别则结束`do_timer`处理过程；否则执行 `schedule()`
9. 在`schedule()`中先遍历所有的进程，把配置了alarm唤醒 && 可中断的进程状态修改为运行状态
10. 寻找所有进程中时间片最多的进程，并切换到这个进程。如果所有进程的时间片都耗尽，则根据进程优先级重设进程时间片，然后切换到第一个任务(注意：第一个任务被称为'idle'任务，当没有其它任务可以运行时候，则会运行此任务，此任务不可被杀、不可睡眠，第一个任务的状态不起作用，因为它永远可以运行)

