# 物理内存

Linux 适用于多种体系结构，因此需要一个与体系结构无关的抽象概念来表示物理内存。本章将介绍运行系统中用于管理物理内存的结构。

## NUMA和UMA架构

### UMA(一致性内存访问)

所有CPU共享同一物理内存，适用于单路处理器系统。

### NUMA(非一致性内存访问)

在多核和多插槽机器中，内存可被排列成内存组，根据与处理器的 “距离” 不同，访问内存的成本也不同。例如，可能会为每个 CPU 分配一个内存组，或者为外围设备附近的 DMA 分配一个最合适的内存组。

在 Linux 系统下，每个内存组都被称为一个节点，其概念用 `pglist_data` 结构来表示，即使是 UMA 架构也是如此。该结构总是通过其 `typedef pg_data_t` 来引用。特定节点的 `pg_data_t` 结构可以通过 `NODE_DATA(nid)` 宏引用，其中 `nid` 是该节点的 ID。

对于 NUMA 架构，节点结构由架构特定代码在启动过程中提前分配。通常，这些结构是在它们所代表的内存库上本地分配的。对于 UMA 架构，只使用一个名为 `contig_page_data` 的静态 `pg_data_t` 结构。节点将在 “节点” 一节中进一步讨论。

## 物理内存分区

Linux启动时候，会将物理内存划分为不同的区域(Zone)，以便不同类型的硬件需求。这些范围通常由访问物理内存的架构限制决定。节点内与特定区域相对应的内存范围由结构区（zone）描述，`zone_t` 类型。每个区域都有以下类型之一。
- `ZONE_DMA` 和 `ZONE_DMA32` 在历史上曾代表适合外围设备 `DMA` 的内存，但这些外围设备无法访问所有可寻址内存。多年来，有了更好、更强大的接口来获取具有 `DMA` 特定要求的内存（使用通用设备的动态 `DMA` 映射），但 `ZONE_DMA` 和 `ZONE_DMA32` 仍代表着对其访问方式有限制的内存范围。根据体系结构的不同，可以在构建时使用 `CONFIG_ZONE_DMA` 和 `CONFIG_ZONE_DMA32` 配置选项禁用这些区域类型中的任何一种，甚至同时禁用这两种类型。某些 64 位平台可能需要这两种区域，因为它们支持具有不同 DMA 寻址限制的外设。
- `ZONE_NORMAL` 用于内核可一直访问的正常内存。如果 DMA 设备支持向所有可寻址内存传输数据，则可以在该区域的页面上执行 `DMA` 操作。`ZONE_NORMAL` 始终处于启用状态。
- `ZONE_HIGHMEM` 是物理内存中未被内核页表中永久映射覆盖的部分。内核只能通过临时映射访问该区域的内存。该区域仅适用于某些 32 位架构，并通过 `CONFIG_HIGHMEM` 启用。
- `ZONE_MOVABLE` 与 `ZONE_NORMAL` 一样，用于正常访问的内存。不同的是，`ZONE_MOVABLE` 中大多数页面的内容都是可移动的。也就是说，虽然这些页面的虚拟地址不会改变，但其内容可能会在不同的物理页面之间移动。`ZONE_MOVABLE` 通常是在内存热插拔时填充的，但也可以在启动时使用 `kernelcore`、`movablecore` 和 `movable_node` 内核命令行参数之一填充。更多详情，请参阅页面迁移和内存热插拔。
- `ZONE_DEVICE` 代表驻留在 `PMEM` 和 `GPU` 等设备上的内存。它具有与 `RAM` 区域类型不同的特性，可为设备驱动程序识别的物理地址范围提供结构页和内存映射服务。`ZONE_DEVICE` 通过配置选项 `CONFIG_ZONE_DEVICE` 启用。

需要注意的是，许多内核操作只能在 `ZONE_NORMAL` 区进行，因此该区的性能最为关键。区段将在 “区段” 章节中进一步讨论。

节点和区域扩展之间的关系由固件报告的物理内存映射、内存寻址的架构限制以及内核命令行中的某些参数决定。

> 注意：一般来说，Linux内核启动时候会将物理内存划分为不同的区域，一般有：`ZONE_DMA`(低地址<16MB，供DMA设备使用)、`ZONE_NORMAL`(常规地址, 直接映射到内核地址空间)、`ZONE_HIGHMEM`(高地址，不直接映射，需通过kmap访问，仅32位系统使用)

例如，在拥有 2 Gbytes 内存的 x86 UMA 机器上使用 32 位内核时，整个内存将位于节点 0 上，并且将有三个区域： `ZONE_DMA`、`ZONE_NORMAL` 和 `ZONE_HIGHMEM`：

```
0                                                            2G
+-------------------------------------------------------------+
|                            node 0                           |
+-------------------------------------------------------------+

0         16M                    896M                        2G
+----------+-----------------------+--------------------------+
| ZONE_DMA |      ZONE_NORMAL      |       ZONE_HIGHMEM       |
+----------+-----------------------+--------------------------+
```

如果内核禁用了 `ZONE_DMA`，启用了 `ZONE_DMA32`，并使用 `movablecore=80%` 参数在两节点均分 `16 Gbytes` 内存的 `arm64` 机器上启动，节点 0 上会有 `ZONE_DMA32`、`ZONE_NORMAL` 和 `ZONE_MOVABLE`，节点 1 上会有 `ZONE_NORMAL` 和 `ZONE_MOVABLE`：

```
1G                                9G                         17G
+--------------------------------+ +--------------------------+
|              node 0            | |          node 1          |
+--------------------------------+ +--------------------------+

1G       3G        4200M          9G          9320M          17G
+---------+----------+-----------+ +------------+-------------+
|  DMA32  |  NORMAL  |  MOVABLE  | |   NORMAL   |   MOVABLE   |
+---------+----------+-----------+ +------------+-------------+
```

内存组可能属于交错节点。在下面的示例中，一台 `x86` 机器的 `16 Gbytes` RAM 分属 4 个内存库，偶数库属于节点 0，奇数库属于节点 1：

```
0              4G              8G             12G            16G
+-------------+ +-------------+ +-------------+ +-------------+
|    node 0   | |    node 1   | |    node 0   | |    node 1   |
+-------------+ +-------------+ +-------------+ +-------------+

0   16M      4G
+-----+-------+ +-------------+ +-------------+ +-------------+
| DMA | DMA32 | |    NORMAL   | |    NORMAL   | |    NORMAL   |
+-----+-------+ +-------------+ +-------------+ +-------------+
```
在这种情况下，节点 0 的跨度为 0 至 12 Gbytes，节点 1 的跨度为 4 至 16 Gbytes。

## 节点

如前所述，内存中的每个节点都由 `pg_data_t` 来描述，`pg_data_t` 是 `struct pglist_data` 的类型定义。在分配页面时，Linux 默认使用节点本地分配策略，从最靠近运行 CPU 的节点分配内存。由于进程往往运行在同一个 CPU 上，因此很可能会使用当前节点的内存。如 NUMA 内存策略所述，用户可以控制分配策略。

大多数 NUMA 架构都会维护一个指向节点结构的指针数组。在启动过程中，当架构特定代码解析固件报告的物理内存映射时，实际结构会被提前分配。节点初始化的大部分工作发生在启动过程稍后，由 `free_area_init()` 函数完成，稍后将在 “初始化” 一节中介绍。

除节点结构外，内核还维护一个名为 `node_states` 的 `nodemask_t` 位掩码数组。数组中的每个位掩码都代表一组具有特定属性的节点，这些属性由 `node_states` 枚举定义：
- `N_POSSIBLE`：节点可能在某个时刻联机。
- `N_ONLINE`：节点处于联机状态。
- `N_NORMAL_MEMORY`：节点具有正常内存。
- `N_HIGH_MEMORY`：节点具有普通内存或大内存。禁用 `CONFIG_HIGHMEM` 时，别名为 `N_NORMAL_MEMORY`。
- `N_MEMORY`：节点具有内存（普通、大容量、可移动）。
- `N_CPU`：节点有一个或多个 CPU

对于具有上述属性的每个节点，都会设置 `node_states[<property>]` 位掩码中与节点 ID 相对应的位。

例如，对于具有正常内存和 CPU 的节点 2，将在：
```c
node_states[N_POSSIBLE]
node_states[N_ONLINE]
node_states[N_NORMAL_MEMORY]
node_states[N_HIGH_MEMORY]
node_states[N_MEMORY]
node_states[N_CPU]
```

有关节点掩码的各种操作，请参阅 `include/linux/nodemask.h`。

其中，节点掩码用于提供节点遍历宏，即 `for_each_node()` 和 `for_each_online_node()`。

例如，为每个在线节点调用一个函数 foo()：
```c
for_each_online_node(nid) {
    pg_data_t *pgdat = NODE_DATA(nid);
    foo(pgdat);
}
```

## Node 结构体

在 `include/linux/mmzone.h` 中声明了节点结构 `struct pglist_data`：

### 常用参数

- `node_zones`：该节点的区域。并非所有区域都已填充，但它是完整的列表。该节点的节点区域列表和其他节点的节点区域列表都会引用该列表。
- `mode_zonelists`：所有节点中所有区域的列表。该列表定义了优先分配的区域顺序。`node_zonelists` 由 `mm/page_alloc.c` 中的 `build_zonelists()` 在初始化核心内存管理结构时设置。
- `nr_zones`：此节点中已填充区域的数量。
- `node_mem_map`：对于使用 `FLATMEM` 内存模型的 `UMA` 系统，0 的节点 `node_mem_map` 是代表每个物理帧的结构页数组。
- `node_page_ext`：对于使用 `FLATMEM` 内存模型的 `UMA` 系统，0 的节点 `node_page_ext` 是结构页的扩展数组。仅在启用 `CONFIG_PAGE_EXTENSION` 的内核中可用。
- `node_start_pfn`：此节点中起始页框的页框编号。
- `node_present_pages`：此节点中存在的物理页面总数。
- `node_spanned_pages`：物理页面范围的总大小，包括空洞。
- `node_size_lock`：用于保护定义节点扩展的字段的锁。只有在启用 `CONFIG_MEMORY_HOTPLUG` 或 `CONFIG_DEFERRED_STRUCT_PAGE_INIT` 配置选项中的至少一个选项时才会定义。`pgdat_resize_lock()` 和 `pgdat_resize_unlock()` 用于操作 `node_size_lock`，而无需检查 `CONFIG_MEMORY_HOTPLUG` 或 `CONFIG_DEFERRED_STRUCT_PAGE_INIT`。
- `node_id`：节点 ID（NID），从 0 开始。
- `totalreserve_pages`：这是每个节点保留的不可用于用户空间分配的页面。
- `first_deferred_pfn`：如果大型计算机上的内存初始化是延迟的，那么这是第一个需要初始化的 `PFN`。仅在启用 `CONFIG_DEFERRED_STRUCT_PAGE_INIT` 时定义
- `deferred_split_queue`：每个节点的巨大页面队列，这些页面的分割被延迟。仅在 `CONFIG_TRANSPARENT_HUGEPAGE` 启用时定义。
- `__lruvec`：保存 `LRU` 列表及相关参数的每节点 `lruvec`。仅在禁用内存 `cgroup` 时使用。不应直接访问它，而应使用 `mem_cgroup_lruvec()` 查找 lruvecs。

### 回收控制

- `kswapd`：kswapd 内核线程的每个节点实例。
- `kswapd_wait`、`pfmemalloc_wait`、`reclaim_wait`：用于同步内存回收任务的工作队列
- `nr_writeback_throttled`：等待清理脏页面的任务数。
- `nr_reclaim_start`：在回收被节流等待回写时写入的页面数。
- `kswapd_order`：控制 kswapd 尝试回收的顺序
- `kswapd_highest_zoneidx`：要被 kswapd 回收的最高区域索引
- `kswapd_failures`：kswapd 无法回收页面的运行次数
- `min_unmapped_pages`：无法回收的未映射文件后备页的最小数量。由 `vm.min_unmapped_ratio` sysctl 决定。仅在启用 `CONFIG_NUMA` 时定义。
- `min_slab_pages`：无法回收的 SLAB 页面的最小数量。由 `vm.min_slab_ratio` sysctl 决定。仅在启用 `CONFIG_NUMA` 时定义
- `flags`：控制回收行为的标志。

### Compaction control

- `kcompactd_max_order`：kcompactd 应尽量达到的页面顺序。
- `kcompactd_highest_zoneidx`：kcompactd 压缩的最高区域索引。
- `kcompactd_wait`：用于同步内存压缩任务的工作队列。
- `kcompactd`：kcompactd 内核线程的每个节点实例。
- `proactive_compact_trigger`：决定是否启用主动压缩。由 `vm.compaction_proactiveness` sysctl 控制。

### 统计

- `per_cpu_nodestats`：节点的每 CPU 虚拟机统计数据
- `vm_stat`：节点的虚拟机统计数据。

## Zones

## Pages

## Folios

## Initialization

## 内存层次划分(整理)

Linux内核中，物理内存管理通常遵循如下层级划分(由大到小)：
1. 整体物理内存记录：内核启动后，最初由memblock框架记录整个系统的物理内存布局。这一阶段还未进行细致管理，只是把所有可用和保留内存区域记录下来。
2. NUMA节点(Node)：在支持NUMA的系统中，物理内存首先按照硬件结构划分为多个NUMA节点。每个NUMA节点通常对应一个CPU插槽或一组处理器。代表了内存和处理器之间访问速度不同的物理区域（对于非NUMA系统，比如大多数SMP系统，整个物理内存就作为一个单一的节点来处理）。
3. 内存区域(Zone)：在每个NUMA节点内部，内存进一步被划分成多个Zone，以适应不同的使用场景和硬件约束。常见的Zone包括：
    - `ZONE_DMA`：针对某些设备(如：ISA DMA设备)需要低地址内存的要求
    - `ZONE_NORMAL`：内核直接映射的内存区域，供内核大部分操作使用
    - `ZONE_HIGHMEM`：在32位系统中，当物理内存超过内核直接映射范围时候，超出部分归入此区。
    - `ZONE_MOVALBE`：上述说过....
4. 页(Page)及Buffy分配管理器：在Zone内，内存被进一步划分为固定大小的页面(通常为4KB)，而Buffy分配器则在各个Zone内管理这些页面的分配和回收，满足动态内存需求。

层次如下：
整体物理内存记录 --> NUMA节点 --> 每个NUMA节点分为不同内存区(Zone) --> 页(Page)

## 物理内存管理相关代码

1. `init/main.c` --> `start_kernel`函数中 --> `setup_arch`函数中 --> `early_reserve_memory`函数：

```c
/**
 * 用于在内核启动的早期阶段预留（reserve）特定的内存区域。
 * 这一过程确保了这些内存区域不会被内核或其他系统组件错误地使用，
 * 从而保证关键硬件设备、固件或引导加载程序所需的内存区域得以保留和正常工作。
 */
static void __init early_reserve_memory (void)
{
    /**
     * Reserve the memory occupied by the kernel between _text and
     * __end_of_kernel_reserve symbols. Any kernel sections after the
     * __end_of_kernel_reserve symbol must be explicitly reserved with a
     * separate memblock_reserve() or they will be discarded.
     *
     * 保留 _text 和 __end_of_kernel_reserve 符号之间内核占用的内存。
     * 在__end_of_kernel_reserve符号之后的内核部分必须通过单独的memblock_reserve()明确保留，否则将被丢弃。
     *
     * _text:内核代码段的起始地址，包含：
     *  1. 内核的入口点，是CPU复位后执行的第一条指令
     *  2. 整个内核的代码段(.text)。编译后的内核函数和指令
     * _text：之后的区域主要包含：
     *  1. .text：内核代码本体，包含所有内核函数
     *  2. .rodata：只读数据，比如：常量、内核符号表、syscall表等
     *  3. .init.text/.init.data：初始化期间使用的数据和代码，
     *      例如：start_kernel()及其调用的初始化函数(这些段在内核启动完成后会被释放)
     * __end_of_kernel_reserve：
     *  1. 该符号通常表示内核保留区域的结束，其具体位置依赖于内核的链接脚本和体系架构
     *  2. 在vmlinux.lds.S中，它一般用于标记内核静态影响的结尾，后续的物理内存可用于动态分配(如：memblock管理的区域)
     */
    memblock_reserve (__pa_symbol (_text),
                      (unsigned long)__end_of_kernel_reserve - (unsigned long)_text);

    /*
     * The first 4Kb of memory is a BIOS owned area, but generally it is
     * not listed as such in the E820 table.
     *
     * Reserve the first 64K of memory since some BIOSes are known to
     * corrupt low memory. After the real mode trampoline is allocated the
     * rest of the memory below 640k is reserved.
     *
     * In addition, make sure page 0 is always reserved because on
     * systems with L1TF its contents can be leaked to user processes.
     *
     * 内存的前 4KB 是 BIOS 拥有的区域，但通常不会在 E820 表中列出。
     *
     * 保留前 64K 内存，因为某些 BIOS 会损坏低内存。实际模式分配后，
     *  640K 以下的其余内存将被保留。
     *
     * 此外，确保第 0 页始终被保留，因为在使用 L1TF 的系统中，其内容可能会泄露给用户进程。
     */
    memblock_reserve (0, SZ_64K);

    // 保留 ramdisk 展开内存
    early_reserve_initrd ();

    // 内核启动早期阶段与系统初始化相关的特定内存区域标记为保留区域，
    //  确保这些区域内存中保存的setup数据不会被后续内存分配所覆盖或使用。
    // - 保留关键的启动数据区域：BIOS或引导加载器将一些重要配置信息、参数
    //      以及其他setup数据存放在特定内存区域，
    //      被memblock设置位保留，避免被通用内存分配器误用
    // - 适配x86架构的特殊需求：x86平台某些特殊内存布局要求，
    //      比如低内存(0 ~ 1MB)中某些区域必须保持原样。
    //      该函数会根据平台要求，保留相应内存范围，
    //      以保证与旧有BIOS数据区、EBDA(扩展BIOS数据区)等相关数据不会被破坏
    // - 为后续内核初始化提供保障：在内核进一步建立完善内存管理机制
    //      （比如：页描述符数组和buddy分配器）之前，确保这些setup数据保持不变。
    memblock_x86_reserve_range_setup_data ();

    // 保留BIOS和固件使用的内存
    reserve_bios_regions ();

    /**
     * 在Intel Sandy Bridge(SNB)平台上调整和保留特定的物理内存区域，
     * 以防止内核错误使用某些可能导致系统不稳定的内存范围。
     * 其主要功能包括：
     *  1. 检测并保留问题内存区域
     *  2. 防止GPU或硬件冲突
     *  3. 调用 memblock_remove进行调整
     *
     * SNB 是Intel 2011年推出的第二代Core处理器微架构，属于x86-64指令集架构的CPU平台。
     *  它是Nehaiem(第一代 Core i系列)的继任者，主要用于桌面、笔记本和服务器市场。
     *  1. 32nm制程工艺
     *  2. 集成GPU(核显)
     *  3. 改进微架构
     *  4. 新增内存控制器
     *  5. 引入PCIe 2.0控制器：处理器内部集成了 PCIe 2.0控制器，
     *      减少了对外部芯片组的依赖，提高了I/O速度
     */
    trim_snb_memory ();
}
```

2. 根据CPU、是否启用五级页表，设置IOMEM结束地址：
```c
// boot_cpu_data.x86_phys_bits = arch/x86/kernel/setup.c
// boot_cpu_data.x86_phys_bits = MAX_PHYSMEM_BITS;
// MAX_PHYSMEM_BITS		2^n: max size of physical address space
// arch/x86/include/asm/sparsemem.h => #define MAX_PHYSMEM_BITS (pgtable_l5_enabled() ? 52 : 46)
// 传统4级页表：PGD、PUD、PMD、PTE
iomem_resource.end = (1ULL << boot_cpu_data.x86_phys_bits) - 1;
```

3. 通过E820向BIOS获取物理内存信息、并进行整合(`e820__memory_setup()`)：

```c
// e820__memory_setup ();
/*
 * Calls e820__memory_setup_default() in essence to pick up the firmware/bootloader
 * E820 map - with an optional platform quirk available for virtual platforms
 * to override this method of boot environment processing:
 *
 * e820__memory_setup 函数是 Linux 内核启动过程中用于处理 BIOS 提供的 E820 内存映射信息的核心函数，
 * 其功能主要包括以下三部分：
 * 1. 内存数据保存: 该函数将 BIOS 通过 E820 接口检测到的内存地址范围数据（存储在 boot_params.e820_table 中）
 *    拷贝到全局的 e820_table 数据结构中，以便后续内存管理模块使用。
 * 2. 内存布局处理: 函数会对原始的 E820 表进行筛选和合并操作(筛选重叠区域：排除逻辑上不可能存在的重叠内存段;
 *    合并相邻同类型区域：将连续的可用（E820_RAM）或保留（E820_RESERVED）内存合并为连续区间)
 * 3. 将整理后的内存信息以标准格式输出到内核日志
 */
void __init e820__memory_setup (void)
{
    char* who;

    /* This is a firmware interface ABI - make sure we don't break it: */
    BUILD_BUG_ON (sizeof (struct boot_e820_entry) != 20);

    // e820.c e820__memory_setup_default()
    // 关键步骤
    who = x86_init.resources.memory_setup ();

    memcpy (e820_table_kexec, e820_table, sizeof (*e820_table_kexec));
    memcpy (e820_table_firmware, e820_table, sizeof (*e820_table_firmware));

    pr_info ("BIOS-provided physical RAM map:\n");
    e820__print_table (who);
}

/**
 * @brief
 *  Pass the firmware (bootloader) E820 map to the kernel and process it:
 *
 *  将固件（引导加载程序）E820 映射传递给内核并进行处理：
 */
char* __init e820__memory_setup_default (void)
{
    char* who = "BIOS-e820";

    /**
     * Try to copy the BIOS-supplied E820-map.
     *
     * Otherwise fake a memory map; one section from 0k->640k,
     * the next section from 1mb->appropriate_mem_k
     *
     * 尝试复制 BIOS 提供的 E820-map，失败则回退到 E-88、E-801
     * 否则，伪造一个内存映射；一部分从 0k->640k 开始，下一部分从 1mb->appropriation_mem_k 开始
     */
    if (append_e820_table (boot_params.e820_table, boot_params.e820_entries) < 0) {
        u64 mem_size;

        // 比较其他方法的结果，选择内存更大的方法：
        /* Compare results from other methods and take the one that gives more RAM: */
        if (boot_params.alt_mem_k < boot_params.screen_info.ext_mem_k) {
            mem_size = boot_params.screen_info.ext_mem_k;   // E-88内存大小
            who      = "BIOS-88";
        } else {
            mem_size = boot_params.alt_mem_k;
            who      = "BIOS-e801";
        }

        e820_table->nr_entries = 0;
        e820__range_add (0, LOWMEMSIZE (), E820_TYPE_RAM);
        e820__range_add (HIGH_MEMORY, mem_size << 10, E820_TYPE_RAM);
    }

    /* We just appended a lot of ranges, sanitize the table: */
    /**
     * @brief 整合从 BIOS 获取到的物理内存信息：
     *  1. 该函数会过滤掉重叠或矛盾的内存区域描述符，并移除无效的类型标识（如未定义的类型值）。
     *      例如，若 BIOS 返回的某段内存同时被标记为 usable 和 reserved，该函数会将其修正为合法类型。
     *  2. 通过检查内存区域的基地址和长度是否合法（如基地址是否对齐、长度是否超过物理内存总量），
     *      避免内核因错误数据崩溃
     *  3. 将相邻的同类内存区域（如多个 E820_RAM 区域）合并为连续可用内存块，简化后续内存分配逻辑
     *  4. 若 BIOS-e820 失败，内核会回退使用 BIOS-88 或 BIOS-e801 的内存数据，
     *      此时 e820__update_table 仍会对其结果进行标准化处理
     *
     * 对从BIOS获取到的所有内存相关数据进行排序、排序过程中进行了合并、处理标志
     */
    e820__update_table (e820_table);

    return who;
}

int __init e820__update_table (struct e820_table* table)
{
    struct e820_entry* entries        = table->entries;
    u32                max_nr_entries = ARRAY_SIZE (table->entries);
    enum e820_type     current_type, last_type;
    unsigned long long last_addr;
    u32                new_nr_entries, overlap_entries;
    u32                i, chg_idx, chg_nr;

    /* If there's only one memory region, don't bother: */
    if (table->nr_entries < 2) {
        return -1;
    }

    BUG_ON (table->nr_entries > max_nr_entries);

    /* Bail out if we find any unreasonable addresses in the map: */
    for (i = 0; i < table->nr_entries; i++) {
        if (entries[i].addr + entries[i].size < entries[i].addr) {
            return -1;
        }
    }

    /**
     * Create pointers for initial change-point information (for sorting):
     * 为初始变化信息创建指针（用于排序）：
     */
    for (i = 0; i < 2 * table->nr_entries; i++) {
        change_point[i] = &change_point_list[i];
    }

    /**
     * Record all known change-points (starting and ending addresses),
     * omitting empty memory regions:
     *
     * 记录所有已知变化点（起始地址和终止地址）、省略空内存区域：
     */
    chg_idx = 0;
    for (i = 0; i < table->nr_entries; i++) {
        if (entries[i].size != 0) {
            change_point[chg_idx]->addr    = entries[i].addr;
            change_point[chg_idx++]->entry = &entries[i];
            change_point[chg_idx]->addr    = entries[i].addr + entries[i].size;
            change_point[chg_idx++]->entry = &entries[i];
        }
    }
    chg_nr = chg_idx;

    /* Sort change-point list by memory addresses (low -> high): */
    sort (change_point, chg_nr, sizeof (*change_point), cpcompare, NULL);

    /* Create a new memory map, removing overlaps: */
    overlap_entries = 0; /* Number of entries in the overlap table */
    new_nr_entries  = 0; /* Index for creating new map entries */
    last_type       = 0; /* Start with undefined memory type */
    last_addr       = 0; /* Start with 0 as last starting address */

    /**
     * Loop through change-points, determining effect on the new map:
     *
     * 循环查看更改点，确定对新地图的影响：
     */
    for (chg_idx = 0; chg_idx < chg_nr; chg_idx++) {
        /* Keep track of all overlapping entries */
        if (change_point[chg_idx]->addr == change_point[chg_idx]->entry->addr) {
            /* Add map entry to overlap list (> 1 entry implies an overlap) */
            overlap_list[overlap_entries++] = change_point[chg_idx]->entry;
        }
        else {
            /* Remove entry from list (order independent, so swap with last): */
            for (i = 0; i < overlap_entries; i++) {
                if (overlap_list[i] == change_point[chg_idx]->entry) {
                    overlap_list[i] = overlap_list[overlap_entries - 1];
                }
            }
            overlap_entries--;
        }
        /*
         * If there are overlapping entries, decide which
         * "type" to use (larger value takes precedence --
         * 1=usable, 2,3,4,4+=unusable)
         *
         * 如果有重叠的条目，则决定使用哪个 使用哪种 “类型”（较大的值优先  --  1=可用，2,3,4,4+=不可用）
         */
        current_type = 0;
        for (i = 0; i < overlap_entries; i++) {
            if (overlap_list[i]->type > current_type) {
                current_type = overlap_list[i]->type;
            }
        }

        /* Continue building up new map based on this information: */
        if (current_type != last_type || e820_nomerge (current_type)) {
            if (last_type) {
                new_entries[new_nr_entries].size = change_point[chg_idx]->addr - last_addr;
                /* Move forward only if the new size was non-zero: */
                if (new_entries[new_nr_entries].size != 0) {
                    /* No more space left for new entries? */
                    if (++new_nr_entries >= max_nr_entries) {
                        break;
                    }
                }
            }
            if (current_type) {
                new_entries[new_nr_entries].addr = change_point[chg_idx]->addr;
                new_entries[new_nr_entries].type = current_type;
                last_addr                        = change_point[chg_idx]->addr;
            }
            last_type = current_type;
        }
    }

    /* Copy the new entries into the original location: */
    memcpy (entries, new_entries, new_nr_entries * sizeof (*entries));
    table->nr_entries = new_nr_entries;

    return 0;
}
```


