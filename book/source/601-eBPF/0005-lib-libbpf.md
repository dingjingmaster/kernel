# libbpf

libbpf 是 eBPF 开发的参考库。它作为内核树的一部分进行开发和维护，通常与内核本身同步进行。由于在项目中包含整个内核树并不现实，因此只在 https://github.com/libbpf/libbpf 上维护 libbpf 库的镜像。

libbpf 包含用户空间组件和 eBPF 组件。eBPF 组件主要是预处理器语句、前向声明和类型定义，它们使 eBPF 程序的编写更加容易。用户空间组件是一个库，用于加载 eBPF 程序并与加载的资源交互。

