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


