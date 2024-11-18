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
