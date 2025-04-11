# 内存布局图

## 传统加载image或zImage内存布局

```
        |                        |
0A0000  +------------------------+ 
        |  BIOS EBDA预留(不要用) | 
09A000  +------------------------+
        |  命令行                |
        |  堆/栈 (实模式)        | 
098000  +------------------------+
        |  Kernel setup (实模式) | 
090200  +------------------------+
        |  Kernel boot sector    |  The kernel legacy boot sector.
090000  +------------------------+
        |  Protected-mode kernel |  The bulk of the kernel image.
010000  +------------------------+
        |  Boot loader           |  <- Boot sector entry point 0000:7C00
001000  +------------------------+
        |  Reserved for MBR/BIOS |
000800  +------------------------+
        |  Typically used by MBR |
000600  +------------------------+
        |  BIOS use only         |
000000  +------------------------+
```

## 现代版本zImage(引导协议>=2.02)

```
       ~                        ~
       |  Protected-mode kernel |
100000 +------------------------+
       |  I/O memory hole       |
0A0000 +------------------------+
       |  Reserved for BIOS     |   Leave as much as possible unused
       ~                        ~
       |  Command line          |   (Can also be below the X+10000 mark)
X+10000+------------------------+
       |  Stack/heap            |   For use by the kernel real-mode code.
X+08000+------------------------+
       |  Kernel setup          |   The kernel real-mode code.
       |  Kernel boot sector    |   The kernel legacy boot sector.
X      +------------------------+
       |  Boot loader           |   <- Boot sector entry point 0000:7C00
001000 +------------------------+
       |  Reserved for MBR/BIOS |
000800 +------------------------+
       |  Typically used by MBR |
000600 +------------------------+
       |  BIOS use only         |
000000 +------------------------+
```

## 4级虚拟内存布局

```
========================================================================================
    Start addr    |   Offset   |     End addr     |  Size   | VM area description
========================================================================================
                  |            |                  |         |
 0000000000000000 |    0       | 00007fffffffefff | ~128 TB | user-space virtual memory,
                  |            |                  |         | different per mm
 00007ffffffff000 | ~128    TB | 00007fffffffffff |    4 kB | ... guard hole
__________________|____________|__________________|_________|___________________________
                  |            |                  |         |
 0000800000000000 | +128    TB | 7fffffffffffffff |   ~8 EB | ... 
                  |            |                  |         | huge, almost 63 bits wide
                  |            |                  |         | hole of non-canonical
                  |            |                  |         | virtual memory addresses
                  |            |                  |         | up to the -8 EB starting
                  |            |                  |         | offset of kernel mappings.
                  |            |                  |         |
                  |            |                  |         | LAM relaxes canonicallity
                  |            |                  |         | check allowing to create
                  |            |                  |         | aliases for userspace
                  |            |                  |         | memory here.
__________________|____________|__________________|_________|___________________________
                                                            |
                                                            | Kernel-space virtual memory,
                                                            | shared between all processes:
__________________|____________|__________________|_________|___________________________
                  |            |                  |         |
 8000000000000000 |   -8    EB | ffff7fffffffffff |   ~8 EB | ... 
                  |            |                  |         | huge, almost 63 bits wide
                  |            |                  |         | hole of non-canonical
                  |            |                  |         | virtual memory addresses
                  |            |                  |         | up to the -128 TB
                  |            |                  |         | starting offset of kernel
                  |            |                  |         | mappings.
                  |            |                  |         |
                  |            |                  |         | LAM_SUP relaxes canonicallity
                  |            |                  |         | check allowing to create
                  |            |                  |         | aliases for kernel memory here.
_______________________________|__________________|_________|___________________________
                  |            |                  |         |
 ffff800000000000 | -128    TB | ffff87ffffffffff |    8 TB | ... guard hole, 
                  |            |                  |         | also reserved for hypervisor
                  |            |                  |         |
 ffff880000000000 | -120    TB | ffff887fffffffff |  0.5 TB | LDT remap for PTI
 ffff888000000000 | -119.5  TB | ffffc87fffffffff |   64 TB | direct mapping of all 
                  |            |                  |         | physical memory 
                  |            |                  |         | (page_offset_base)
 ffffc88000000000 |  -55.5  TB | ffffc8ffffffffff |  0.5 TB | ... unused hole
 ffffc90000000000 |  -55    TB | ffffe8ffffffffff |   32 TB | vmalloc/ioremap space 
                  |            |                  |         | (vmalloc_base)
 ffffe90000000000 |  -23    TB | ffffe9ffffffffff |    1 TB | ... unused hole
 ffffea0000000000 |  -22    TB | ffffeaffffffffff |    1 TB | virtual memory map
                  |            |                  |         | (vmemmap_base)
 ffffeb0000000000 |  -21    TB | ffffebffffffffff |    1 TB | ... unused hole
 ffffec0000000000 |  -20    TB | fffffbffffffffff |   16 TB | KASAN shadow memory
__________________|____________|__________________|_________|___________________________
                                                            |
                                                            | Identical layout to the 
                                                            | 56-bit one from here on:
____________________________________________________________|___________________________
                  |            |                  |         |
 fffffc0000000000 |   -4    TB | fffffdffffffffff |    2 TB | ... unused hole
                  |            |                  |         | vaddr_end for KASLR
 fffffe0000000000 |   -2    TB | fffffe7fffffffff |  0.5 TB | cpu_entry_area mapping
 fffffe8000000000 |   -1.5  TB | fffffeffffffffff |  0.5 TB | ... unused hole
 ffffff0000000000 |   -1    TB | ffffff7fffffffff |  0.5 TB | %esp fixup stacks
 ffffff8000000000 | -512    GB | ffffffeeffffffff |  444 GB | ... unused hole
 ffffffef00000000 |  -68    GB | fffffffeffffffff |   64 GB | EFI region mapping space
 ffffffff00000000 |   -4    GB | ffffffff7fffffff |    2 GB | ... unused hole
 ffffffff80000000 |   -2    GB | ffffffff9fffffff |  512 MB | kernel text mapping, 
                  |            |                  |         | mapped to physical address 0
 ffffffff80000000 |-2048    MB |                  |         |
 ffffffffa0000000 |-1536    MB | fffffffffeffffff | 1520 MB | module mapping space
 ffffffffff000000 |  -16    MB |                  |         |
    FIXADDR_START | ~-11    MB | ffffffffff5fffff | ~0.5 MB | kernel-internal fixmap range,
                  |            |                  |         | variable size and offset
 ffffffffff600000 |  -10    MB | ffffffffff600fff |    4 kB | legacy vsyscall ABI
 ffffffffffe00000 |   -2    MB | ffffffffffffffff |    2 MB | ... unused hole
__________________|____________|__________________|_________|___________________________
```

## 5级虚拟内存布局

```
========================================================================================
    Start addr    |   Offset   |     End addr     |  Size   | VM area description
========================================================================================
                  |            |                  |         |
 0000000000000000 |    0       | 00fffffffffff000 |  ~64 PB | user-space virtual memory,
                  |            |                  |         | different per mm
 00fffffffffff000 |  ~64    PB | 00ffffffffffffff |    4 kB | ... guard hole
__________________|____________|__________________|_________|___________________________
                  |            |                  |         |
 0100000000000000 |  +64    PB | 7fffffffffffffff |   ~8 EB | ... huge, almost 63 bits 
                  |            |                  |         | wide hole of non-canonical
                  |            |                  |         | virtual memory addresses 
                  |            |                  |         | up to the -8EB TB. starting 
                  |            |                  |         | offset of kernel mappings.
                  |            |                  |         |
                  |            |                  |         | LAM relaxes canonicallity 
                  |            |                  |         | check allowing to create
                  |            |                  |         | aliases for userspace 
                  |            |                  |         | memory here.
__________________|____________|__________________|_________|___________________________
                                                            |
                                                            | Kernel-space virtual memory,
                                                            | shared between all processes:
____________________________________________________________|___________________________
 8000000000000000 |   -8    EB | feffffffffffffff |   ~8 EB | ... huge, almost 63 bits 
                  |            |                  |         | wide hole of non-canonical
                  |            |                  |         |  virtual memory addresses 
                  |            |                  |         |  up to the -64 PB.
                  |            |                  |         | starting offset of 
                  |            |                  |         |  kernel mappings.
                  |            |                  |         |
                  |            |                  |         | LAM_SUP relaxes canonicallity 
                  |            |                  |         |  check allowing to create.
                  |            |                  |         | aliases for kernel memory here.
____________________________________________________________|___________________________
                  |            |                  |         |
 ff00000000000000 |  -64    PB | ff0fffffffffffff |    4 PB | ... guard hole, also 
                  |            |                  |         | reserved for hypervisor
 ff10000000000000 |  -60    PB | ff10ffffffffffff | 0.25 PB | LDT remap for PTI
 ff11000000000000 |  -59.75 PB | ff90ffffffffffff |   32 PB | direct mapping of all 
                  |            |                  |         | physical memory 
                  |            |                  |         | (page_offset_base)
 ff91000000000000 |  -27.75 PB | ff9fffffffffffff | 3.75 PB | ... unused hole
 ffa0000000000000 |  -24    PB | ffd1ffffffffffff | 12.5 PB | vmalloc/ioremap space 
                  |            |                  |         | (vmalloc_base)
 ffd2000000000000 |  -11.5  PB | ffd3ffffffffffff |  0.5 PB | ... unused hole
 ffd4000000000000 |  -11    PB | ffd5ffffffffffff |  0.5 PB | virtual memory map 
                  |            |                  |         |  (vmemmap_base)
 ffd6000000000000 |  -10.5  PB | ffdeffffffffffff | 2.25 PB | ... unused hole
 ffdf000000000000 |   -8.25 PB | fffffbffffffffff |   ~8 PB | KASAN shadow memory
__________________|____________|__________________|_________|___________________________
                                                            |
                                                            | Identical layout to the 
                                                            | 47-bit one from here on:
____________________________________________________________|___________________________
                  |            |                  |         |
 fffffc0000000000 |   -4    TB | fffffdffffffffff |    2 TB | ... unused hole
                  |            |                  |         | vaddr_end for KASLR
 fffffe0000000000 |   -2    TB | fffffe7fffffffff |  0.5 TB | cpu_entry_area mapping
 fffffe8000000000 |   -1.5  TB | fffffeffffffffff |  0.5 TB | ... unused hole
 ffffff0000000000 |   -1    TB | ffffff7fffffffff |  0.5 TB | %esp fixup stacks
 ffffff8000000000 | -512    GB | ffffffeeffffffff |  444 GB | ... unused hole
 ffffffef00000000 |  -68    GB | fffffffeffffffff |   64 GB | EFI region mapping space
 ffffffff00000000 |   -4    GB | ffffffff7fffffff |    2 GB | ... unused hole
 ffffffff80000000 |   -2    GB | ffffffff9fffffff |  512 MB | kernel text mapping, 
                  |            |                  |         | mapped to physical address 0
 ffffffff80000000 |-2048    MB |                  |         |
 ffffffffa0000000 |-1536    MB | fffffffffeffffff | 1520 MB | module mapping space
 ffffffffff000000 |  -16    MB |                  |         |
    FIXADDR_START | ~-11    MB | ffffffffff5fffff | ~0.5 MB | kernel-internal fixmap range,
                  |            |                  |         | variable size and offset
 ffffffffff600000 |  -10    MB | ffffffffff600fff |    4 kB | legacy vsyscall ABI
 ffffffffffe00000 |   -2    MB | ffffffffffffffff |    2 MB | ... unused hole
__________________|____________|__________________|_________|___________________________
```

## 进程用户态栈和内核态栈

x86_64系统中，进程的**用户态栈**和**内核态栈**位于不同地址空间，并且它们的存储位置、管理方式和大小都不相同。

### 用户态栈

- 位于：用户态进程虚拟地址空间的高地址部分，通常在 0x7FFFFFFFF000附近（TASK_SIZE）
- 大小：默认8MB，但是可以通过ulimit -s 设置或者`pthread_attr_setstacksize()`调整
- 增长方向：高地址 -> 低地址（栈顶->栈底）
- 用户态栈由RSP管理，每次push都会向低地址方向移动。
- 多线程：每个线程都有自己的用户态栈，由mmap分配，不同于主线程的brk分配方式

```
+-----------------------------------+ 0x7FFFFFFFF00(用户态栈顶)
| 用户栈(默认 8MB)                  |
|                                   |
+-----------------------------------+ 栈底
| 堆区                              |
+-----------------------------------+ 低地址到栈底，由malloc/brk/mmap分配
| 共享库(.so)                       |
+-----------------------------------+ 由动态连接器加载
| 代码段(.text)                     |
+-----------------------------------+ 0x00400000000(典型ELF起始地址)
```

### 内核态栈

- 位置：进程的内核栈存储在内核地址空间中，每个进程的内核栈地址是唯一的，通常位于fixmap区域或vmalloc区域(启用CONFIG_VMAP_STACK)
- 与用户态栈完全分离，用户态进程无法直接访问其内核栈
- 大小：默认16KB(2个物理页，THREAD_SIZE=16384)
- 增长方向：从高地址向低地址增长(栈顶->栈底)
- 内核栈不会换出(swap out)，始终驻留在内存中
- 每个进程有一个独立的内核栈，在进程进入内核态(系统调用或中断)时候，RSP被切换到该进程的内核栈顶

```
高地址
+-----------------------------------+ 0xFFFFFFFFFFFFFFFF
| 内核映射区                        |
+-----------------------------------+
| vmalloc 区域                      |
+-----------------------------------+
| fixmap 区域                       |
+-----------------------------------+
| 进程 A 的内核栈                   | (16KB)
| 进程 B 的内核栈                   | (16KB)
| 进程 C 的内核栈                   | (16KB)
+-----------------------------------+
| 内核代码段                        |
+-----------------------------------+ 0xFFFFFFFF80000000
```
### 用户态栈和内核态栈之间的切换

当用户进程执行**系统调用**或发生**中断**时候：
- CPU进入内核态（Ring 0）
	- CS 段寄存器从Ring 3切换到Ring 0
    - RSP切换到当前进程的内核栈顶
- 执行内核代码
	- 处理系统调用，如`sys_read()`
- 返回用户态
	- 恢复RSP到用户栈，恢复CS段，继续执行用户代码

```
用户态(syscall 发生前)
用户态栈：
+-----------------------------------+
| 用户态数据                        |
| 函数调用栈                        |
+-----------------------------------+  <------ RSP(用户态)

内核态(syscall 发生时)
内核态栈：
+-----------------------------------+
| 内核寄存器保存                    |
| 调度信息                          |
+-----------------------------------+  <------ RSP(内核态)
```

### thread_info

thread_info 是Linux内核中描述线程(进程)低级状态的结构体，存储了与调度、堆栈管理等相关的信息。

在x86_64架构的现代Linux版本(5.x及更高)中，thread_info被嵌入到stack_struct结构体中，每个进程/线程有且只有一个thread_info结构

thread_info的位置：
- Linux 4.x及更早：thread_info位于内核栈底部(共享16KB内核栈，可能被栈溢出破坏)
- Linux 5.x及更高：thread_info存放在task_struct，避免栈溢出影响，提高安全性

> 现代Linux内核通过`CONFIG_VMAP_STACK`允许动态分配更大的内核栈(vmalloc)，减少固定大小的限制，增强安全性和稳定性

```
4.x之前版本

高地址(栈顶)
+-----------------------------------+
| 内核栈                            |
+-----------------------------------+
| struct thread_info                |
+-----------------------------------+

5.x之后版本

高地址(栈顶)
+-----------------------------------+
| 内核栈(16KB)                      |
+-----------------------------------+
低地址(栈底)

+-----------------------------------+
| struct task_struct                | (包含了 thread_info)
+-----------------------------------+
```


