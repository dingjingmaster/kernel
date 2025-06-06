# 概述

通常编译的最后一步就是链接过程，即将编译生成的对象(obj文件)和归档文件(动态库、静态库)进行整合，生成可执行文件或共享库，执行这一步的命令就是ld(链接器)。

ld接受以AT&T的链接编辑器命令语言语法的超集编写的链接器命令语言文件，以提供对链接过程的显式和完全控制。

此版本的ld使用通用的BFD库对对象文件进行操作。这允许ld以许多不同的格式读取、组合和写入目标文件，例如COFF或a.out。不同的格式可以链接在一起以产生任何可用的目标文件类型。更多信息请参见BFD。

除了灵活性之外，GNU链接器在提供诊断信息方面比其他链接器更有帮助。许多链接器在遇到错误时立即放弃执行；只要有可能，ld就会继续执行，允许您识别其他错误（或者，在某些情况下，尽管有错误，也可以获得输出文件）。

