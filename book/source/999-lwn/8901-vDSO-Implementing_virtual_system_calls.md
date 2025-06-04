# Implementing virtual system calls

“虚拟动态共享对象”（vDSO）是内核导出的一个小型共享库，用于加速某些不一定必须在内核空间中运行的系统调用的执行。虽然内核开发人员已经确定了一小部分可以通过vDSO导出的函数，但是没有什么可以阻止开发人员添加自己的函数。如果应用程序需要频繁而快速地从内核获取某些信息，那么vDSO函数可能是一个有用的解决方案。有关vDSO的介绍，请参阅vDSO(7)手册页。

本文展示了用于实现这些函数的编程技术如何主要基于对Linux链接器脚本的巧妙添加，以及如何将相同的技术应用于实现基于一组内核变量快速计算值的函数。它可以看作是本系列关于常规系统调用的补充。根据硬件平台的不同，vDSO库中包含了不同的函数集。这里描述的实现指的是`x86_64`体系结构。

## 虚拟系统调用

当进程调用系统调用时，它执行一条特殊指令，迫使CPU切换到内核模式，将寄存器的内容保存在内核模式堆栈上，并开始执行内核函数。当系统调用得到服务后，内核恢复保存在内核模式堆栈上的寄存器的内容，并执行另一条特殊指令来恢复用户空间进程的执行。

将访问内核空间信息的系统调用放入进程地址空间会使它们更快，因为它们能够从内核地址空间获取所需的值，而无需进行上下文切换。显然，只有“只读”系统调用才是这种模拟的有效候选者，因为用户空间进程不允许写入内核地址空间。模拟系统调用的用户空间函数称为虚拟系统调用。

`x86_64`上的Linux vDSO实现提供了四种虚拟系统调用：`__vdso_clock_gettime()`、`__vdso_gettimeofday()`、`__vdso_time()`和`__vdso_getcpu()`。它们分别对应于标准的`clock_gettime()`、`gettimeofday()`、`time()`和`getcpu()`系统调用。

虚拟系统调用比标准系统调用快多少？这显然取决于硬件平台和处理器类型。在带有Intel 2.8GHz Core i7 CPU的P6T SE华硕主板上，执行标准`gettimeofday()`系统调用所需的平均时间为90.5微秒，相应的虚拟系统调用的平均时间为22.3微秒，这是一个显着的改进，证明了开发vDSO框架所花费的努力是合理的。

## 什么是vDSO

在考虑vDSO时，您应该记住这个术语有两个不同的含义：(1)它是一个动态库，但这个术语也用于指代(2)属于每个用户模式进程的地址空间的内存区域。vDSO内存区域—像大多数其他进程内存区域一样—在每次映射时，其默认位置是随机的。地址空间布局随机化是防御安全漏洞的一种形式。

如果你通过“cat /proc/pid/maps”命令显示进程ID等于pid的进程所拥有的内存区域，你会得到这样一行：
```
7ffffb892000-7ffffb893000 r-xp 00000000 00:00 0          [vdso]
```

它描述了这个特殊地区的属性。vDSO的起始地址`0x7ffffb892000`小于`PAGE_OFFSET`（在x86-64机器上为`0xffff880000000000`），因此，vDSO是用户空间地址空间的一部分。最后一个地址，即0x7ffffb893000，表明vDSO占用一个4KB的页面。r-xp权限标志指定启用读取和执行权限，并且该区域是私有的（非共享的）。最后三个字段表示该区域没有从任何文件映射，因此它没有inode。

存储在vDSO内存区域中的二进制代码具有动态库的格式。如果将代码从vDSO内存区域转储到一个文件中，并对其应用file命令，您将得到：

```
ELF 64-bit LSB shared object, x86-64, version 1 (SYSV), dynamically linked, stripped 
```

所有Linux共享动态库（如glibc）都使用ELF格式。

如果您反汇编包含vDSO内存区域的文件，您将发现前面提到的四个虚拟系统调用的汇编代码。在3.15内核中，只需要2733字节（总共4096字节）来存储ELF头和vDSO中虚拟系统调用的代码。这意味着仍然有附加功能的空间。

由于vDSO是一个完全形成的ELF映像，因此可以对其进行符号查找。这允许在较新的内核版本中添加新符号，并允许C库在运行时检测在不同内核版本下运行时可用的功能。如果你在一个包含vDSO内存区域的文件上运行“readelf -s”，你会得到文件符号表部分的条目显示：

```
Symbol table '.dynsym' contains 11 entries:
   Num:    Value          Size Type    Bind   Vis      Ndx Name
     0: 0000000000000000     0 NOTYPE  LOCAL  DEFAULT  UND
     1: ffffffffff700330     0 SECTION LOCAL  DEFAULT    7
     2: ffffffffff700600   727 FUNC    WEAK   DEFAULT   13 clock_gettime@@LINUX_2.6
     3: 0000000000000000     0 OBJECT  GLOBAL DEFAULT  ABS LINUX_2.6
     4: ffffffffff7008e0   365 FUNC    GLOBAL DEFAULT   13 __vdso_gettimeofday@@LINUX_2.6
     5: ffffffffff700a70    61 FUNC    GLOBAL DEFAULT   13 __vdso_getcpu@@LINUX_2.6
     6: ffffffffff7008e0   365 FUNC    WEAK   DEFAULT   13 gettimeofday@@LINUX_2.6
     7: ffffffffff700a50    22 FUNC    WEAK   DEFAULT   13 time@@LINUX_2.6
     8: ffffffffff700a70    61 FUNC    WEAK   DEFAULT   13 getcpu@@LINUX_2.6
     9: ffffffffff700600   727 FUNC    GLOBAL DEFAULT   13 __vdso_clock_gettime@@LINUX_2.6
    10: ffffffffff700a50    22 FUNC    GLOBAL DEFAULT   13 __vdso_time@@LINUX_2.6
```

在这里，您可以看到vDSO区域中的各种函数。

## 数据在哪里？

直到现在还没有提到虚拟系统调用如何从内核地址空间检索变量。这可能是vDSO子系统中最有趣且文档记录最少的特性。为了具体起见，考虑`__vdso_gettimeofday()`虚拟系统调用。该函数从名为`vsyscall_gtod_data`的变量中获取所需的内核数据。这个变量有两个不同的地址：
- 第一个是一个常规的内核空间地址，其值大于`PAGE_OFFSET`。如果你看看这个系统。映射文件，您会发现该符号有一个类似`ffffffff81c76080的`地址。
- 第二个地址位于一个称为“vvar页”的区域。本页的基址（`VVAR_ADDRESS`）在内核中定义为`0xffffffffff5ff000`，接近64位地址空间的末尾。此页对于用户空间代码是只读的。

显然，两个地址映射到相同的物理地址；也就是说，它们指向相同的页面框架。

变量是在这个页面中使用`DECLARE_VVAR()`宏创建的。例如，`vsyscall_gtod_data`的声明将其放在`vvar`页中的偏移量128处。因此，这个变量的用户空间可见地址是：`0xffffffff5ff000 + 128`。为了允许链接器检测vvar页面中存在的变量，`DECLARE_VVAR()`宏将它们放在内核二进制映像中的`.vvar`特殊部分中（有关更多信息，请参阅Linux二进制文件中的特殊部分）。

内核内运行的代码使用内核空间地址访问`vsyscall_gtod_data`。在用户模式下运行的虚拟系统调用必须使用第二个地址。位于vDSO页面中的变量由用户模式进程使用`__USER_DS`段描述符访问。它们可以读，但不能写。作为进一步的预防措施，Linux将它们声明为const，以便编译器能够检测到任何写入它们的尝试。

vvar变量的值是根据用户空间代码无法访问的其他内核变量的值设置的。当内核修改其中一个内部变量的值时，必须更新`vvar`页中的相关变量。在Linux中，该任务由`timekeeping_update()`函数执行，例如，每当jiffies（系统启动以来经过的滴答数）被修改时，就会调用该函数。

## 添加功能到vDSO页

Linux vDSO实现使内核开发人员可以轻松地向vDSO页面添加新功能。如果查看一下这四个虚拟系统调用的代码，您会注意到其中三个调用从一个名为`vsyscall_gtod_data`的`vvar`变量（类型为`struct vsyscall_gtod_data`）中获取所需的内核数据。第四个，即`__vdso_getcpu()`，不获取任何东西：它通过执行rdtscp指令获取CPU索引。

另一个重要的观察是，传递给`timekeeping_update()`（更新`vsyscall_gtod_data`字段的函数）的参数是一个指针，该指针指向一个名为`timekeeper`的全局内核变量，类型为`struct timekeeper`。当内核函数更新与虚拟系统调用相关的计时器字段时，它可以假设在短时间内这个更改将影响`vsyscall_gtod_data`，从而影响虚拟系统调用返回的值。换句话说，更新`timekeeper`字段的内核函数与虚拟系统调用是松散耦合的。

定义新的vDSO函数的最简单方法是在感兴趣的内部内核变量和添加到vvar页面的变量之间创建类似的耦合。可以对`update_vsycall()`函数进行扩展，以便根据需要将数据移动到vvar页中，您可以预期它会以相对较高的频率被调用。

这里有一些提示，可以帮助您开发新的vDSO功能：
- 在编写vDSO函数时，请记住代码中不能出现外部内核函数或全局内核变量，只能出现自动变量（堆栈）和vvar变量。由于函数在用户模式下运行，因此链接器不知道内核空间标识符。
- 必须告诉处理vDSO函数的Linux链接器脚本，正在添加一个新函数，例如`__vdso_foo()`。为此，您必须在linux/arch/x86/vdso/vdso.lds. s中添加如下几行：
    ```
    foo;
    __vdso_foo;
    ```
    您还必须在linux/arch/x86/vdso/目录下的`__vdso_foo()`定义的末尾添加以下行：
    ```
    int foo (struct fd * fd) __attribute__((weak, alias(“__vdso_foo”)));
    ```
    这样，`foo()`就变成了`__vdso_foo()`的弱别名。
- 不要忘记修改`update_vsycall()`的代码，这是一个由`timekeeping_update()`调用的简单传递函数，它将timekeeper的一些字段复制到`vsyscall_gtod_data`变量中。该函数还可以将您感兴趣的数据复制到vvar页面中的新变量中。
- 如果新函数的原型是：`int __vdso_foo(struct fd *fd)`，那么调用`__vdso_foo()`的测试程序，比如`test_foo.c`，必须链接为：
    ```
    gcc -o test_foo linux/arch/x86/vdso/vdso.so test_foo.c
    ```
    因为`__vdso_foo()`的代码包含在vdso。所以将foo定义为`__vdso_foo`的弱别名后，您还可以在测试程序中调用`foo()`而不是`__vdso_foo()`。

## 结论

用于实现vDSO函数的编程技术基于对Linux链接器脚本的一些巧妙的添加，这些添加允许在Linux内核中定义的内核函数在所有用户模式进程的地址空间中进行链接。可以很容易地实现新的vDSO函数来获取有关当前内核状态的信息（系统中的进程数、空闲页面数等）。如果您有必须从内核中以高频率和低开销获取的信息，那么vDSO机制可能正好提供了您所需要的工具。

