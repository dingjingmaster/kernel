# 内存地址

现代计算机中，CPU + 一些电路硬件电路使得内存管理更有效、更健壮，因此操作系统不必跟踪物理内存使用。

三类内存地址：
- 逻辑地址(Logical address)：程序中指定指令或操作的地址，每个地址由段和偏移组成。
- 线性地址(Linear address，也称虚拟地址)：
- 物理地址(Physical address)：用于处理存储器芯片中的存储单元，对应处理器沿着地址总线发送到存储器总线的电信号

内存管理器单元(MMU)通过分段处理单元将逻辑地址转为线性地址，然后再通过页单元将线性地址转为物理地址。

```
              +----------+  线性地址    +----------+
逻辑地址 ---> | 分段单元 | ---------->  | 分页单元 | -----------> 物理地址
              +----------+              +----------+
```

在多处理器系统中，所有CPU可以共享统一块内存，这意味着RAM可以被多个CPU同时访问。因为读写必须同时进行，所以在RAM芯片和总线之间插入了内存仲裁器的电路，它的作用是当RAM空闲时候被CPU访问，当RAM正在被某个CPU访问时候禁止其他CPU访问。即使单CPU也许要内存仲裁器，因为DMA和CPU都需要访问RAM。多处理器系统中，仲裁器实现更加复杂。

## 硬件分段

从80286开始，Intel微处理器支持两种方式执行地址转换——实模式和保护模式。

实模式的存在主要是为了保持与旧处理器的兼容性，并使操作系统启动。

## 分段选择器和分段寄存器

逻辑地址分为两部分：一个段标识符和一个偏移量，指定该段内的相对地址。

段标识符是一个称为段选择器的16位字段，而偏移量是一个32位字段。

