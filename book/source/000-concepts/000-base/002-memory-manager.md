# 内存管理

## 虚拟内存

虚拟内存充当应用程序内核与内存管理单元(MMU)之间的逻辑层。

虚拟内存具有很多优点：
- 允许操作系统同时执行多个进程
- 允许运行所需内存大于物理内存的程序
- 可以执行代码仅在内存中部分加载的程序
- 每个进程都可以访问,允许访问物理内存的一个子集
- 多个进程可以共享同一个程序内存镜像或者同一个动态库内存镜像
- 程序可以是重定位的，它可以在物理内存的任何位置
- 程序员可以编写与机器无关代码的程序，因为不必直到物理内存是如何组织的

虚拟内存子系统的主要组成部分是虚拟地址空间。进程可以使用的内存范围与物理内存地址完全不同。当进程使用虚拟地址时候，内核和MMU协同查找实际进程请求的的物理地址位置。

当今CPU包含自动将虚拟地址转为物理地址的硬件电路。因此，可访问的RAM被划分为页面帧(通常是4KB或8KB)，并引入一组页表（Page Tables）来表示虚拟地址与物理地址的对应关系。这些电路使内存分配更简单，可以使用一系列物理地址非连续的页帧来满足程序运行中连续虚拟地址的需求。

## RAM使用

所有Unix操作系统都将RAM分为两部分，小的一部分用来加载内核镜像(比如：内核代码、内核静态数据结构)；剩余RAM部分由虚拟内存系统处理，并有三种可能使用的方式：
1. 满足内核对缓存区、描述符、其他动态内核数据结构的使用
2. 满足通用内存区域和文件映射内存使用需求
3. 通过缓存，从磁盘或其他缓存设备获得更好的性能

每个内存请求都必须被满足，但是RAM是有限的，因此必须要在多个内存请求之间做一些平衡，尤其是所剩内存很少的情况下。这里就需要根据经验编写更合适的内存管理/分配算法。

虚拟内存必须要解决的一个主要问题就是内存碎片化。

## 内核存储分配器

内核内存分配器(KMA, Kernel Memory Allocator)作为内存管理子系统，用于满足系统中所有使用内存的需求。

一个好的KMA需满足以下特性：
- 必须块
- 内存浪费少
- 减少内存碎片化
- 能与其他内存管理子系统合作