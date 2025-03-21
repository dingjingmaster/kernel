# 调用

GNU链接器旨在涵盖广泛的情况，并尽可能与其他链接器兼容。因此，您有很多选择来控制它的行为。

## 命令行选项

链接器支持大量的命令行选项，但在实际操作中，它们很少用于任何特定的上下文中。例如，ld的一个常用用法是在一个受支持的标准Unix系统上链接标准Unix对象文件。在这样的系统中，链接文件hello.o：

```shell
ld -o output /lib/crt0.o hello.o -lc
```

这条指令告诉链接器，使用 /lib/crt0.o、htllo.o 以及库libc.a生成一个名为output的文件。

一些命令行选项可以在命令行中的任何位置指定。但是，引用文件的选项，如‘-l’或‘-T’，会在命令行中出现该选项时读取文件，因此需要注意目标文件和其他文件选项之间的依赖关系。使用不同的参数重复非文件选项没有进一步的效果，可能会覆盖先前出现的该选项（命令行上更左边的那些）。

必要参数指要链接在一起的目标文件或归档文件。它们可以跟在命令行选项后面、前面，也可以与命令行选项混合在一起，除非对象文件参数不能放在选项和它的参数之间。

链接器至少包含一个对象文件作为输入，否则链接器不会生成任何文件，并报错：'no input files'

如果链接器不能识别目标文件的格式，它将假定它是一个链接器脚本。这种方式指定的脚本会作为链接器主链接脚本的补充，使用'-T'选项则会完全替换默认链接器脚本。

对于名称为单个字母的选项，选项参数必须紧跟选项字母而不加空格，或者作为单独的参数紧接在需要它们的选项后面。

对于名称由多个字母组成的选项，可以在选项名称前加一个或两个破折号；例如，‘-trace-symbol’和‘--trace-symbol’是等价的。注意，这条规则有一个例外。以小写“o”开头的多个字母选项前面只能有两个破折号。这是为了减少与‘-o’选项的混淆。例如，‘-magic’将输出文件名设置为‘magic’，而‘--magic’将设置输出文件的NMAGIC标志。

多字母选项的参数必须用等号与选项名分开，或者作为单独的参数紧接在需要它们的选项后面。例如，‘--trace-symbol foo’和‘--trace-symbol=foo’是等价的。接受多个字母选项名称的唯一缩写。

注意：如果链接器是通过编译器驱动程序（例如‘gcc’）间接调用的，那么所有的链接器命令行选项都应该以‘-Wl’作为前缀，（或任何适合特定编译器驱动程序的前缀），像这样：

```shell
gcc -Wl,--start-group foo.o bar.o -Wl,--end-group
```

这一点很重要，因为某些情况编译器驱动程序可能会静默地删除链接器选项，从而导致错误的链接。当通过驱动程序传递需要值的选项时，也可能出现混淆，因为在选项和参数之间使用空格作为分隔符，并导致驱动程序仅将选项传递给链接器并将参数传递给编译器。在这种情况下，最简单的是使用单字母和多字母选项的连接形式，例如：
```shell
gcc foo.o bar.o -Wl,-eENTRY -Wl,-Map=a.map
```

以下是链接器可接受的命令行选项

### 链接器命令行选项

#### @file 

从文件中读取命令行选项。选项read被插入到原来的@file选项的位置。如果文件不存在，或者无法读取，那么该选项将被按字面处理，而不会被删除。

file中的选项以空格分隔。通过将整个选项用单引号或双引号括起来，可以将空白字符包含在选项中。任何字符（包括反斜杠）都可以通过在要包含的字符前加上反斜杠来包含。文件本身可能包含额外的@file选项；任何这样的选项都将被递归地处理。

#### -a keyword

HP/UX兼容性支持此选项。关键字参数必须是字符串‘archive’， ‘shared’或‘default’之一。‘-aarchive’在功能上等同于‘-Bstatic’，其他两个关键字在功能上等同于‘-Bdynamic’。此选项可以使用任意次数。

#### --audit AUDITLIB

将AUDITLIB添加到动态节的DT_AUDIT项中。不检查AUDITLIB是否存在，也不会使用库中指定的DT_SONAME。如果多次指定，DT_AUDIT将包含一个冒号分隔的审计接口列表。如果链接器在搜索共享库时发现具有审计项的对象，它将在输出文件中添加相应的DT_DEPAUDIT项。这个选项只有在支持rtld-audit接口的ELF平台上才有意义。

#### -b input-format(--format=input-format)

ld可以配置为支持一种以上的目标文件。如果您的ld是以这种方式配置的，那么您可以使用‘-b’选项在命令行中为跟随该选项的输入对象文件指定二进制格式。即使将ld配置为支持其他对象格式，您通常也不需要指定它，因为应该将ld配置为期望每台机器上最常用的格式作为默认输入格式。input-format为文本字符串，BFD库支持的特定格式的名称。（您可以使用‘objdump -i’列出可用的二进制格式）。

如果要用不寻常的二进制格式链接文件，则可能需要使用此选项。您还可以使用‘-b’显式切换格式（当链接不同格式的目标文件时），通过在特定格式的每组目标文件之前包含‘-b input-format’。

默认格式取自环境变量gnuttarget。参见环境变量。还可以使用TARGET命令从脚本中定义输入格式。

#### -c MRI-commandfile(--mri-script=MRI-commandfile)

为了与MRI产生的链接器兼容，ld接受用另一种受限制的命令语言编写的脚本文件，这些脚本文件在MRI兼容脚本文件中进行了描述。引入带有选项‘-c’的MRI脚本文件；使用‘-T’选项运行用通用old脚本语言编写的链接器脚本。如果MRI-cmdfile不存在，ld将在‘-L’选项指定的目录中查找它。

#### -d -dc -dp

这三个选项是等价的；为了与其他链接器兼容，支持多个表单。即使指定了可重定位的输出文件（使用‘-r’），它们也会为通用符号分配空间。脚本命令`FORCE_COMMON_ALLOCATION`具有相同的效果。

#### --depaudit AUDITLIB(-P AUDITLIB)

将AUDITLIB添加到动态节的`DT_DEPAUDIT`项中。不检查`AUDITLIB`是否存在，也不会使用库中指定的`DT_SONAME`。如果多次指定，`DT_DEPAUDIT`将包含一个冒号分隔的审计接口列表。这个选项只有在支持rtld-audit接口的ELF平台上才有意义。提供-P选项是为了Solaris兼容性。

#### --enable-linker-version

启用`LINK_VERSION`链接器脚本指令，在输出部分数据中描述。如果在一个链接器脚本中使用了这个指令，并且这个选项已经启用，那么一个包含链接器版本的字符串将被插入到当前点。

注意-该选项在链接器命令行上的位置非常重要。它只会影响命令行中紧随其后的链接器脚本，或者内置于链接器中的链接器脚本。

#### --disable-linker-version

 禁用`LINK_VERSION`链接器脚本指令。不插入版本信息字符串(默认选项)。

#### --enable-non-contiguous-regions

此选项可避免在输入部分与匹配的输出部分不匹配时生成错误。链接器试图将输入部分分配给匹配输出部分的子序列，只有在没有足够大的输出部分时才会生成错误。当几个非连续的内存区域可用并且输入部分不需要特定的内存区域时，这是有用的。输入部分的求值顺序不会改变，例如：

```
MEMORY {
    MEM1 (rwx) : ORIGIN = 0x1000, LENGTH = 0x14
    MEM2 (rwx) : ORIGIN = 0x1000, LENGTH = 0x40
    MEM3 (rwx) : ORIGIN = 0x2000, LENGTH = 0x40
}
SECTIONS {
    mem1 : { *(.data.*); } > MEM1
    mem2 : { *(.data.*); } > MEM2
    mem3 : { *(.data.*); } > MEM3
}

with input sections:
.data.1: size 8
.data.2: size 0x10
.data.3: size 4

results in .data.1 affected to mem1, and .data.2 and .data.3
affected to mem2, even though .data.3 would fit in mem3.
```
此选项与INSERT语句不兼容，因为它改变了输入节映射到输出节的方式。

#### --enable-non-contiguous-regions-warnings

当`——enable-non-continuous-regions`允许在部分映射中可能出现意外匹配时，此选项启用警告。


#### -e entry(--entry=entry)

使用entry作为程序开始执行的显式符号，而不是默认的入口点。如果没有名为entry的符号，链接器将尝试将entry解析为一个数字，并使用该数字作为入口地址(该数字将以10为基数进行解释；你可以使用前导‘0x’来表示基数16，或者使用前导‘0’来表示基数8)。

#### --exclude-libs lib,lib,...

指定不应从中自动导出符号的存档库列表。库名可以用逗号或冒号分隔。指定`——exclude-libs ALL`将所有归档库中的符号从自动导出中排除。此选项仅适用于链接器的i386 PE目标端口和ELF目标端口。对于i386 PE，在.def文件中显式列出的符号仍然被导出，不管这个选项是什么。对于ELF目标端口，受此选项影响的符号将被视为隐藏。

#### --exclude-modules-for-implib module,module,...

指定目标文件或存档成员的列表，其中不应自动导出符号，但应将其批量复制到链接期间生成的导入库中。模块名可以用逗号或冒号分隔，并且必须与ld打开文件时使用的文件名完全匹配；对于归档成员，这只是成员名，但对于对象文件，列出的名称必须包括并精确匹配用于指定链接器命令行上的输入文件的任何路径。此选项仅适用于链接器的i386 PE目标端口。在.def文件中显式列出的符号仍然会导出，无论是否使用此选项。

#### -E(--export-dynamic) --no-export-dynamic

在创建动态链接的可执行文件时，使用-E选项或--export-dynamic选项会导致链接器将所有符号添加到动态符号表中。动态符号表是在运行时从动态对象中可见的符号集。

如果您不使用这些选项中的任何一个（或使用--no-export-dynamic选项来恢复默认行为），动态符号表通常只包含那些被链接中提到的某个动态对象引用的符号。

如果您使用dlopen来加载需要引用程序定义的符号的动态对象，而不是其他动态对象，那么在链接程序本身时可能需要使用此选项。

如果输出格式支持，还可以使用动态列表来控制应该将哪些符号添加到动态符号表中。参见‘--dynamic-list’的描述。

注意，这个选项是针对ELF目标端口的。PE目标支持从DLL或EXE导出所有符号的类似功能；参见下面‘--export-all-symbols’的描述。

#### --export-dynamic-symbol=glob

当创建动态链接的可执行文件时，与glob匹配的符号将被添加到动态符号表中。在创建共享库时，对匹配glob的符号的引用不会绑定到共享库中的定义。当创建共享库且未指定‘-Bsymbolic’或‘--dynamic-list’时，此选项不适用。这个选项只有在支持共享库的ELF平台上才有意义。

#### --export-dynamic-symbol-list=file

为文件中的每个模式指定一个‘--export-dynamic-symbol’。文件格式与版本节点相同，没有作用域和节点名。

#### -EB

链接大端对象。这会影响默认的输出格式。

#### -EL

链接小端对象。
 
#### -f name(--auxiliary=name)

当创建一个ELF共享对象时，将内部`DT_AUXILIARY`字段设置为指定的名称。这告诉动态链接器，共享对象的符号表应该用作共享对象名称的符号表上的辅助过滤器。

如果稍后将程序链接到此筛选器对象，那么在运行该程序时，动态链接器将看到`DT_AUXILIARY`字段。如果动态链接器解析了过滤器对象中的任何符号，它将首先检查共享对象名称中是否有定义。如果有，将使用它来代替过滤器对象中的定义。共享对象名称不需要存在。因此，共享对象名称可用于提供某些功能的替代实现，可能用于调试或特定于机器的性能。

此选项可以指定多次。`DT_AUXILIARY`项将按照它们在命令行中出现的顺序创建。

#### -F name(--filter=name)

在创建ELF共享对象时，将内部`DT_FILTER`字段设置为指定的名称。这告诉动态链接器，正在创建的共享对象的符号表应该用作共享对象名称的符号表上的过滤器。

如果稍后将程序链接到此筛选器对象，那么，当您运行程序时，动态链接器将看到`DT_FILTER`字段。动态链接器将像往常一样根据过滤器对象的符号表解析符号，但它实际上会链接到共享对象名称中找到的定义。因此，过滤器对象可用于选择由对象名称提供的符号子集。

一些旧的链接器在整个编译工具链中使用`-F`选项来指定输入和输出对象文件的对象文件格式。GNU链接器为此目的使用其他机制：`-b`、`--format`、`--oformat`选项、链接器脚本中的`TARGET`命令和`gnuttarget`环境变量。当不创建一个ELF共享对象时，GNU链接器将忽略`-F`选项。

#### -fini=name

当创建一个ELF可执行或共享对象时，通过将`DT_FINI`设置为函数的地址，在可执行或共享对象被卸载时调用NAME。默认情况下，链接器使用`_fini`作为要调用的函数。

#### -g

忽略。为其它工具提供兼容性

#### -G value(--gpsize=value)

使用GP寄存器设置要优化的对象的最大大小为size。这只对对象文件格式有意义，例如MIPS ELF，它支持将大小对象放入不同的节中。对于其他对象文件格式，这将被忽略。

#### -h name(-soname=name)

在创建ELF共享对象时，将内部`DT_SONAME`字段设置为指定的名称。当可执行文件与具有`DT_SONAME`字段的共享对象链接时，当可执行文件运行时，动态链接器将尝试加载由`DT_SONAME`字段指定的共享对象，而不是使用给定给链接器的文件名。

#### -i

执行增量链接（与选项‘-r’相同）。

#### -init=name

当创建一个ELF可执行对象或共享对象时，通过将`DT_INIT`设置为函数的地址，在加载可执行对象或共享对象时调用`NAME`。默认情况下，链接器使用`_init`作为要调用的函数。

#### -l namespace(--library=namespace)

将由namespec指定的归档文件或目标文件添加到要链接的文件列表中。此选项可以使用任意次数。如果namespec的格式是：filename， ld将在库路径中搜索名为filename的文件，否则将在库路径中搜索名为libnamespec.a的文件。

在支持共享库的系统上，ld也可以搜索libnamespec.a以外的文件。具体来说，在ELF和SunOS系统上，ld将在目录中搜索名为libnamespec的库。所以在搜索libnamespec.a之前。（按照惯例，.so扩展名表示共享库。）注意，此行为不适用于：filename，它总是指定一个名为filename的文件。

链接器只会在命令行中指定的位置搜索存档文件一次。如果存档文件定义了一个符号，而该符号在命令行存档文件之前出现的某个对象中是未定义的，则链接器将包括来自存档文件的适当文件。但是，稍后在命令行上出现的对象中的未定义符号不会导致链接器再次搜索存档。

您可以在命令行中多次列出相同的归档文件。

这种类型的存档搜索是Unix链接器的标准。但是，如果您在AIX上使用ld，请注意它与AIX链接器的行为不同。

#### -L searchdir(--library-path=searchdir)

将路径searchdir添加到ld将搜索存档库和ld控制脚本的路径列表中。您可以多次使用此选项。目录按照在命令行中指定的顺序进行搜索。在命令行中指定的目录将在默认目录之前搜索。-L选项无论出现的顺序都生效。-L选项不影响搜索链接器脚本的方式，除非指定了-T选项。

如果searchdir以=或$SYSROOT开头，那么这个前缀将被SYSROOT前缀取代，由‘--SYSROOT ’选项控制，或者在配置链接器时指定。

搜索的默认路径集（没有使用‘-L’指定）取决于ld使用的仿真模式，在某些情况下还取决于它是如何配置的。

也可以使用`SEARCH_DIR`命令在链接脚本中指定路径。以这种方式指定的目录将在命令行中出现链接器脚本的位置进行搜索。

#### -m emulation

模拟仿真链接器。您可以使用‘--verbose’或‘-V’选项列出可用的模拟。

如果未使用‘-m’选项，则从ldemulate环境变量（如果定义了该变量）获取仿真。

否则，默认模拟取决于链接器的配置方式。

#### --remap-inputs=pattern=filename(--remap-inputs-file=file)

这些选项允许在链接器尝试打开输入文件之前更改它们的名称。选项`--remmap -inputs=foo.o=bar.o`将导致任何加载名为foo.o的文件都变为尝试加载一个名为bar.o的文件。在第一个文件名中允许使用通配符模式，因此`--remap-inputs=foo*.o=bar.o`将重命名任何与`foo*.o`匹配的输入文件为`bar.o`。

选项`--remap-inputs-file=filename`的另一种形式允许从文件中读取重新标记。文件中的每一行都可以包含一个重新映射。空行将被忽略。从散列字符（'#'）到行尾的任何内容都被认为是注释，也会被忽略。映射模式可以用空格或等号（'='）字符与文件名分开。

这些选项可以被指定多次。它们的内容会累积。重新标识将按照它们在命令行中出现的顺序进行处理，如果它们来自文件，则按照它们在文件中出现的顺序进行处理。如果匹配成功，则不再执行该文件名的进一步检查。

如果替换文件名是`/dev/null`或只是`NUL`，那么重新映射实际上会导致输入文件被忽略。这是一种方便的方法，可以从复杂的构建环境中删除输入文件。

请注意，此选项与位置相关，并且仅影响命令行中紧随其后的文件名。因此:
```shell
ld foo.o --remap-inputs=foo.o=bar.o
```

将不起作用，而：
```shell
ld --remap-inputs=foo.o=bar.o foo.o
```
将把输入文件foo.o重命名为bar.o

> 注意：这些选项也会影响链接器脚本中INPUT语句引用的文件。但是，由于链接器脚本是在读取整个命令行之后处理的，所以命令行上的重新映射选项的位置并不重要。

如果启用了verbose选项，那么将报告任何匹配的映射，尽管在重新映射的文件名出现之前需要再次在命令行上启用verbose选项。

如果启用了 `-Map` 或 `--print-map`选项，则映射输出中将包含重新映射列表

#### -M(--print-map)

将链接映射打印到标准输出。链接映射提供了有关链接的信息，包括以下内容：
- 其中对象文件被映射到内存。
- 如何分配常见的符号。
- 链接中包含的所有存档成员，并提及导致存档成员被引入的符号。
- 赋给符号的值。如果符号的值是通过引用同一符号的前一个值的表达式来计算的，那么在链接映射中可能不会显示正确的结果。这是因为链接器丢弃中间结果，只保留表达式的最终值。在这种情况下，链接器将显示用方括号括起来的最终值。例如，一个链接器脚本包含：
    ```
    foo = 1
    foo = foo * 4
    foo = foo + 8
    ```
    如果使用-M选项，将在链接映射中产生以下输出：
    ```
    0x00000001                foo = 0x1
    [0x0000000c]              foo = (foo * 0x4)
    [0x0000000c]              foo = (foo + 0x8)
    ```
- GNU属性是如何合并的：当链接器合并输入`.note.gnu.property`节的时候将变成一个输出`.note.gnu.property`节，一些属性被删除或更新。这些操作将在链接映射中报告。例如:
    ```
    Removed property 0xc0000002 to merge foo.o (0x1) and bar.o (not found)
    ```
    这表明在合并foo.o中的属性时，将从输出中删除属性0xc0000002，其属性0xc0000002值为0x1，bar.o中没有属性0xc0000002。
    ```
    Updated property 0xc0010001 (0x1) to merge foo.o (0x1) and bar.o (0x1)
    ```
    这表明在合并foo.o中的属性时，输出中的属性0xc0010001值被更新为0x1。其0xc0010001属性值为0x1，bar.o，中其0xc0010001属性值为0x1。
- 在一些ELF目标上，由`--relax`插入的修复程序列表
    ```
    foo.o: Adjusting branch at 0x00000008 towards "far" in section .text
    ```
    这表明foo.o中的分支位于0x00000008，针对text部分中的符号“far”，已被取代。
#### --print-map-discarded --no-print-map-discarded

打印（或不打印）链接图中丢弃和垃圾收集部分的列表。默认开启。

#### --print-map-locals --no-print-map-locals

打印（或不打印）链接映射中的本地符号。局部符号将在其名称之前打印文本‘(Local)’，并将在给定部分中的所有全局符号之后列出。临时局部符号（通常以‘.L’开头）将不包含在输出中。默认为关闭。

#### -n --nmagic

关闭部分的页面对齐，并禁用针对共享库的链接。如果输出格式支持Unix风格的幻数，则将输出标记为NMAGIC。

#### -N --omagic

将文本和数据部分设置为可读和可写。此外，不要对数据段进行页面对齐，并禁用针对共享库的链接。如果输出格式支持Unix风格的幻数，则将输出标记为OMAGIC。注意：尽管PE-COFF目标允许可写的文本部分，但它不符合Microsoft发布的格式规范。

#### --no-omagic

这个选项消除了-N选项的大部分效果。它将文本部分设置为只读，并强制数据段与页面对齐。注意-此选项不启用针对共享库的链接。为此使用-Bdynamic。

#### -o output(--output=output)

使用output作为ld生成的程序的名称；如果未指定此选项，则默认使用a.out名称。脚本命令OUTPUT也可以指定输出文件名。

注意-链接器将在开始写入输出文件之前删除它。即使链接由于错误而无法完成，它也会这样做。

注意：链接器将检查以确保输出文件名与任何输入文件名不匹配，但仅此而已。特别是，如果输出文件可能覆盖源文件或其他一些重要文件，它不会报错。因此，在构建系统中，建议使用-o选项作为链接器命令行上的最后一个选项。例如：
```shell
ld -o $(EXE) $(OBJS)
ld $(OBJS) -o $(EXE)
```
如果由于某种原因没有定义‘EXE’变量，则链接器命令的第一个版本可能最终会删除一个目标文件（‘OBJS’列表中的第一个文件），而链接器命令的第二个版本将生成错误消息而不删除任何内容。

#### --dependency-file=depfile

写一个依赖文件到depfile。该文件包含一个适用于make的规则，该规则描述了输出文件以及为生成输出文件而读取的所有输入文件。输出类似于带有‘-M -MP’的编译器的输出（参见使用GNU编译器集合中的选项控制预处理器）。请注意，没有像编译器的‘-MM’这样的选项来排除“系统文件”（这在链接器中不是一个指定良好的概念，不像编译器中的“系统头文件”）。因此，“--dependency-file”的输出总是特定于产生它的安装的确切状态，不应该在没有仔细编辑的情况下复制到分布式的makefiles中。

#### -O level

如果level是一个大于零的数值，则会优化输出。这可能需要更长的时间，因此可能应该只对最终的二进制文件启用。目前这个选项只影响ELF共享库的生成。链接器的未来版本可能会更多地使用此选项。目前，对于该选项的不同非零值，链接器的行为也没有区别。同样，这可能会随着未来的版本而改变。

#### -plugin name

在链接过程中涉及一个插件。name参数是插件的绝对文件名。通常这个参数是由编译器自动添加的，当使用链接时间优化时，但用户也可以添加自己的插件，如果他们愿意的话。

注意，编译器生成插件的位置与ar、nm和ranlib程序搜索插件的位置是不同的。为了使这些命令能够使用基于编译器的插件，必须首先将其复制到`${libdir}/bfd-plugins`目录中。所有基于gcc的链接器插件都是向后兼容的，因此只需复制最新的插件就足够了。

#### --push-state

--push-state允许保留控制输入文件处理的标志的当前状态，以便它们都可以通过一个相应的--pop-state选项恢复。

这些选项包括：-Bdynamic、-Bstatic、-dn、-dy、-call_shared、-non_shared、-static、-N、-N、--whole-archive、--no-whole-archive、-r、-Ur、--copy-dt-needed-entries、--no-copy-dt-needed-entries、--as-needed、--no-as-needed和-a。

此选项的一个目标是pkg-config的规范。当与--libs选项一起使用时，所有可能需要的库都被列出，然后可能一直被链接。最好返回如下内容：
```
-Wl,--push-state,--as-needed -libone -libtwo -Wl,--pop-state
```

#### --pop-state

撤消-push-state的作用，恢复控制输入文件处理的标志的先前值。

#### -q(--emit-relocs)

将重定位节和内容保留在完全链接的可执行文件中。链接后分析和优化工具可能需要这些信息，以便对可执行文件进行正确的修改。这将导致更大的可执行文件。

此选项目前仅在ELF平台上受支持。

#### --force-dynamic

强制输出文件具有动态节。该选项特定于VxWorks目标。

#### -r(--relocatable)

生成可重新定位的输出，即，生成一个输出文件，该文件反过来可以作为ld的输入。这通常称为部分链接。作为一个副作用，在支持标准Unix幻数的环境中，该选项还将输出文件的幻数设置为OMAGIC。如果未指定此选项，则生成一个绝对文件。当链接c++程序时，此选项不会解析对构造函数的引用；要做到这一点，使用‘-Ur’。

当输入文件的格式与输出文件不同时，只有在该输入文件不包含任何重定位时才支持部分链接。不同的输出格式可以有进一步的限制；例如，一些基于a.out的格式根本不支持与其他格式的输入文件的部分链接。

当可重定位输出包含需要链接时间优化（LTO）的内容和不需要LTO的内容时，将创建一个`.gnu_object_only`节来包含一个可重定位的对象文件，就好像‘-r’应用于所有不需要LTO的可重定位输入一样。当处理带有`.gnu_object_only`节的可重定位输入时，链接器将提取`.gnu_object_only`节作为单独的输入。

请注意，由于‘-r’将来自不同输入文件的某些部分组合在一起，因此可能会对最终可执行库或共享库中的代码大小和位置产生负面影响。

这个选项的作用与‘-i’相同。

#### -R filename(--just-symbols=filename)

从文件名中读取符号名称及其地址，但不重新定位它或将其包含在输出中。这允许您的输出文件符号地引用在其他程序中定义的内存的绝对位置。您可以多次使用此选项。
为了与其他ELF链接器兼容，如果-R选项后面跟着目录名，而不是文件名，则将其视为-rpath选项。

#### --rosegment、--no-rosegment

尝试确保只创建一个只读的非代码段。只有在与-z separate-code选项结合使用时才有用。生成的二进制文件应该比单独使用-z分隔代码时要小。如果没有这个选项，或者如果指定了--no-rosegment， -z separate-code选项将创建两个只读段，一个在代码段之前，一个在代码段之后。

这些选项的名称具有误导性，但选择它们是为了使链接器与LLD和GOLD链接器兼容。

这些选项只有ELF目标支持。

#### -s(--strip-all)

从输出文件中省略所有符号信息。

#### -S(--strip-debug)

从输出文件中省略调试器符号信息（但不是所有符号）。

#### --strip-discarded、--no-strip-discarded

省略（或不省略）在丢弃的节中定义的全局符号。默认开启。

#### -plugin-save-temps

永久存储插件“临时”中间文件。

#### -t(--trace)

在ld处理输入文件时打印它们的名称。如果‘-t’被输入两次，那么档案中的成员也会被打印出来。‘-t’输出对于生成链接中涉及的所有目标文件和脚本的列表很有用，例如，在为链接器错误报告打包文件时。

#### -T scriptfile(--script=scriptfile)

使用scriptfile作为链接器脚本。这个脚本替换了ld的默认链接器脚本（而不是添加到它中），除非脚本包含INSERT，所以commandfile必须指定描述输出文件所需的一切。参见链接器脚本。

如果scriptfile在当前目录中不存在，ld将在前面的‘-L’选项指定的目录中查找它。

出现在-T选项之前的命令行选项会影响脚本，但出现在-T选项之后的命令行选项不会。

如果多个‘-T’选项正在增加当前脚本，则会累积多个‘-T’选项，否则将使用最后一个非增加的-T选项。

#### -dT scriptfile(--default-script=scriptfile)

使用scriptfile作为默认的链接器脚本。参见链接器脚本。

该选项类似于`--script`选项，不同之处在于，脚本的处理将延迟到处理完命令行其余部分之后。这允许放置在命令行`--default-script`选项之后的选项影响链接器脚本的行为，当用户不能直接控制链接器命令行时，这一点很重要。（例如，因为命令行是由另一个工具构建的，例如‘gcc’）。

#### -u symbol(--undefined=symbol)

强制符号作为未定义符号输入到输出文件中。例如，这样做可能会触发从标准库中链接其他模块。‘-u’可以与不同的选项参数一起重复，以输入其他未定义的符号。这个选项相当于EXTERN链接器脚本命令。

如果使用该选项强制将其他模块拉入链接，并且如果符号保持未定义是错误的，则应该使用选项--require-defined。

#### --require-defined=symbol

要求在输出文件中定义该符号。该选项与--undefined相同，不同之处在于如果symbol未在输出文件中定义，则链接器将发出错误并退出。通过同时使用EXTERN、ASSERT和DEFINED，在链接器脚本中也可以达到同样的效果。此选项可以多次使用以需要额外的符号。

#### -Ur

对于不使用构造函数或析构函数的程序，或基于ELF的系统，此选项相当于-r：它生成可重定位的输出，即：对于其他二进制文件，-Ur选项类似于-r，但它也解析对构造函数和析构函数的引用。
对于那些-r和-Ur行为不同的系统，在本身与-Ur链接的文件上使用-Ur不起作用；构造函数表构建完成后，就不能再添加它了。仅对最后一个部分链接使用-Ur，对其他部分使用-r。

#### --orphan-handling=MODE

控制孤立部分的处理方式。孤儿节是在链接器脚本中没有特别提到的部分。

MODE可以是以下任意值：
- place: 按照孤立部分中描述的策略，将孤立部分放置到合适的输出部分中。选项“--unique”也会影响section的放置方式。
- discard: 所有孤儿节都将被丢弃，将它们放置在‘/DISCARD/’节中。
- warn: 链接器将把孤儿节放置在适当的位置，并发出警告。
- error: 如果发现任何孤立节，链接器将退出并显示错误。

> 默认不设置此配置项

#### --unique[=SECTION]

为每个匹配section的输入节创建一个单独的输出节，或者如果缺少可选通配符section参数，则为每个孤立输入节创建一个单独的输出节。孤儿节是在链接器脚本中没有特别提到的部分。您可以在命令行中多次使用此选项；它可以防止在链接器脚本中合并具有相同名称的输入节，从而覆盖输出节的赋值。

#### -v(--version、-V)

显示ld的版本号。-V选项还列出支持的模拟。另请参见命令行选项中--enable-link -version的描述，它可用于将链接器版本字符串插入到二进制文件中。

#### -x(--discard-all)

删除所有本地符号

#### -X(--discard-locals)

删除所有临时本地符号。(这些符号以系统特定的本地标签前缀开头，通常是'.L‘表示ELF系统，'L'表示传统的a.out系统)。

#### -y symbol(--trace-symbol=symbol)

打印出现符号的每个链接文件的名称。这个选项可以被给定任意次数。在许多系统中，必须在前面加上下划线。

当您的链接中有一个未定义的符号，但不知道引用来自何处时，此选项非常有用。

#### -Y path

将path添加到默认库搜索路径中。这个选项是为了Solaris兼容性而存在的。

#### -z keyword

可识别的关键字有：
- 'call-nop=prefix-addr'、'call-nop=suffix-nop'、'call-nop=prefix-byte'、'call-nop=suffix-byte'：在通过GOT槽转换对本地定义函数foo的间接调用时，指定1字节的NOP填充。call-nop=prefix-addr产生0x67调用foo。call-nop=suffix-nop产生0x90调用foo。call-nop=prefix-byte生成字节调用foo。call-nop=suffix-byte生成call foo字节。支持i386和x86_64。
- 'cet-report=none'、'cet-report=warning'、'cet-report=error'：在input `.note.gnu`中指定如何报告丢失的`GNU_PROPERTY_X86_FEATURE_1_IBT`和`GNU_PROPERTY_X86_FEATURE_1_SHSTK`属性。属性部分。`Cet-report=none`是默认值，它将使链接器不报告输入文件中丢失的属性。`Cet-report=warning`将使链接器对输入文件中缺少的属性发出警告。`Cet-report=error`将使链接器对输入文件中缺少的属性发出错误。注意，ibt将关闭丢失的`GNU_PROPERTY_X86_FEATURE_1_IBT`属性报告，shstk将关闭丢失的`GNU_PROPERTY_X86_FEATURE_1_SHSTK`属性报告。支持Linux/i386和Linux/x86_64。
- 'combreloc'、'nocombreloc'：组合多个动态重定位节和排序以改进动态符号查找缓存。如果‘nocombreloc’，不要这样做。
- 'common'、'nocommon'：在可重定位链接期间生成`STT_COMMON`类型的公共符号。如果`nocommon`使用`STT_OBJECT`类型。
- 'common-page-size=value'：设置最常用的页面大小为value。如果系统使用这种大小的页面，将对内存映像布局进行优化，以最小化内存页面
- 'defs'：报告常规对象文件中未解析的符号引用。即使链接器正在创建非符号共享库，也会执行此操作。这个选项与'-z undefs' 相反。
- 'dynamic-undefined-weak'、'nodynamic-undefined-weak'：在构建动态对象时，如果未定义的弱符号是从常规对象文件引用的，并且没有通过符号可见性或版本控制强制在本地引用，则将其设置为动态。如果是“nodynamic-undefined-weak”，不要将它们设置为动态的。如果两个选项都没有给出，目标可能默认使用其中一个选项，或者动态选择其他未定义的弱符号。并非所有目标都支持这些选项。
- 'execstack'：将对象标记为需要可执行堆栈。
- 'global'：此选项仅在构建共享对象时才有意义。它使此共享对象定义的符号可用于随后加载的库的符号解析。
- 'globalaudit'：此选项仅在构建动态可执行文件时才有意义。该选项通过在`DT_FLAGS_1`动态标记中设置`DF_1_GLOBAUDIT`位，将可执行文件标记为需要全局审计。全局审计要求对应用程序加载的所有动态对象运行通过`--deaudit`或`-P`命令行选项定义的任何审计库
- 'ibtplt'：生成Intel间接分支跟踪（IBT）使能PLT条目。支持Linux/i386和Linux/x86_64。
- 'ibt'：在`.note.gnu`中生成`GNU_PROPERTY_X86_FEATURE_1_IBT`。属性部分，以指示与IBT的兼容性。这也意味着ibtplt。支持Linux/i386和Linux/x86_64。
- 'indirect-extern-access'、'noindirect-extern-access'：在`.note.gnu`中生成`GNU_PROPERTY_1_NEEDED_INDIRECT_EXTERN_ACCESS`。属性节以指示目标文件需要规范函数指针，并且不能与副本重定位一起使用。此选项还意味着没有外部保护数据和没有copyreloc。支持i386和x86-64。
- 'initfirst'：此选项仅在构建共享对象时才有意义。它标记对象，以便它的运行时初始化将在同时带入进程的任何其他对象的运行时初始化之前发生。类似地，对象的运行时结束将发生在任何其他对象的运行时结束之后。
- 'interpose'：指定动态加载器应修改其符号搜索顺序，以便此共享库中的符号可以插入所有其他没有这样标记的共享库。
- 'unique'、'nounique'：在生成共享库或其他可动态加载的ELF对象时，将其标记为（默认情况下）只应该加载一次，并且只在主名称空间中（使用dlmopen时）。这主要用于标记基本库，如libc， libpthread等，这些库通常不能正常工作，除非它们是自身的唯一实例。这种行为可以被dlmopen调用者覆盖，并且不适用于某些加载机制（例如审计库）。
- 'lam-u48'：在`.note.gnu`中生成`GNU_PROPERTY_X86_FEATURE_1_LAM_U48`。属性节以指示与Intel `LAM_U48`的兼容性。支持Linux/x86_64。
- 'lam-u57'：在`.note.gnu`中生成`GNU_PROPERTY_X86_FEATURE_1_LAM_U57`。属性节以指示与Intel `LAM_U57`的兼容性。支持Linux/x86_64。
- 'lam-u48-report=none'、'lam-u48-report=warning'、'lam-u48-report=error'：在input `.note.gnu`中指定如何报告丢失的`GNU_PROPERTY_X86_FEATURE_1_LAM_U48`属性。`Lam-u48-report=none`，这是默认值，将使链接器不报告输入文件中丢失的属性。`Lam-u48-report=warning`将使链接器对输入文件中缺少的属性发出警告。`Lam-u48-report=error`将使链接器对输入文件中缺少的属性发出错误。支持Linux/x86_64。
- 'lam-u57-report=none'、'lam-u57-report=warning'、'lam-u57-report=error'：在input `.note.gnu`中指定如何报告丢失的`GNU_PROPERTY_X86_FEATURE_1_LAM_U57`属性。`Lam-u57-report=none`，默认值，将使链接器不报告输入文件中丢失的属性。`Lam-u57-report=warning`将使链接器对输入文件中缺少的属性发出警告。`Lam-u57-report=error`将使链接器对输入文件中丢失的属性发出错误。支持Linux/x86_64。
- 'lam-report=none'、'lam-report=warning'、'lam-report=error'：在input `.note.gnu`中指定如何报告丢失的`GNU_PROPERTY_X86_FEATURE_1_LAM_U48`和`GNU_PROPERTY_X86_FEATURE_1_LAM_U57`属性。`Lam-report=none`是默认值，它将使链接器不报告输入文件中丢失的属性。`Lam-report=warning`将使链接器对输入文件中缺少的属性发出警告。`Lam-report=error`将使链接器对输入文件中缺少的属性发出错误。支持Linux/x86_64。
- 'lazy'：在生成可执行库或共享库时，标记它以告诉动态链接器将函数调用解析推迟到调用函数时（惰性绑定），而不是在加载时。惰性绑定是默认的。
- 'loadfltr'：指定在运行时立即处理对象的筛选器。
- 'max-page-size=value'：将支持的最大内存页大小设置为value。
- 'mark-plt'、'nomark-plt'：用动态标记标记PLT表项，`DT_X86_64_PLT`，`DT_X86_64_PLTSZ`和`DT_X86_64_PLTENT`。由于该选项在`R_X86_64_JUMP_SLOT`重定位的`r_addend`字段中存储非零值，因此产生的可执行文件和共享库与动态链接器不兼容，例如那些在旧版本的glibc中没有更改在`R_X86_64_GLOB_DAT`和`R_X86_64_JUMP_SLOT`重定位中忽略`r_addend`的可执行文件和共享库，它们不会忽略`R_X86_64_JUMP_SLOT`重定位的`r_addend`字段。支持x86_64。
- 'muldefs'：允许多个定义
- 'nocopyreloc'：禁用链接器生成的`.dynbss`变量，以取代共享库中定义的变量。可能导致动态文本重定位。
- 'nodefaultlib'：指定动态加载器搜索此对象的依赖项时应忽略任何默认库搜索路径。
- 'nodelete'：指定不应在运行时卸载对象
- 'nodlopen'：指定对象不可由dlopen访问。
- 'nodump'：指定对象不可以使用dldump转储
- 'noexecstack'：将对象标记为不需要可执行堆栈。
- 'noextern-protected-data'：在构建共享库时，不要将受保护的数据符号视为外部的。此选项覆盖链接器后端默认值。它可用于解决编译器生成的受保护数据符号的错误重定位。其他模块对受保护数据符号的更新对生成的共享库是不可见的。支持i386和x86-64。
- 'noreloc-overflow'：禁用重新定位溢出检查。如果在运行时没有动态重定位溢出，则可使用此选项禁用重定位溢出检查。支持x86_64。
- 'memory-seal'、'nomemory-seal'：指示可执行库或共享库应该对所有PT_LOAD段进行密封，以避免进一步的操作（例如更改保护标志，段大小或删除映射）。这是一个需要系统支持的安全加固。这会在`.note.gnu`中生成`GNU_PROPERTY_MEMORY_SEAL`。属性节
- 'now'：在生成可执行库或共享库时，标记它以告诉动态链接器在程序启动时或通过dlopen加载共享库时解析所有符号，而不是将函数调用解析延迟到函数第一次调用时。
- 'origin'：指定对象在路径中需要‘$ORIGIN’处理
- 'pack-relative-relocs'、'nopack-relative-relocs'：在位置无关的可执行和共享库中生成紧凑的相对重定位。它将`DT_RELR`、`DT_RELRSZ`和`DT_RELRENT`表项添加到动态`section`中。在构建依赖于位置的可执行输出和可重定位输出时，它将被忽略。`Nopack-relative-relocs`是默认值，它禁用紧凑的相对重定位。当链接到GNU C库时，对共享C库的`GLIBC_ABI_DT_RELR`符号版本依赖被添加到输出中。支持i386和x86-64。
- 'relro'、'norelro'：在对象中创建一个`ELF PT_GNU_RELRO`段头。这指定了一个内存段，如果支持，在重定位后应该使其只读。指定“common-page size”小于系统页面大小将使此保护失效。如果‘ norelro ’，不要创建一个ELF `PT_GNU_RELRO`段。
- 'report-relative-reloc'：报告由链接器生成的动态相对重定位。支持Linux/i386和Linux/x86_64。
- 'sectionheader'、'nosectionheader'：生成节头。如果使用‘nosectionheader’，则不要生成节头。Sectionheader是默认的。
- 'separate-code'、'noseparate-code'：在对象中创建单独的代码PT_LOAD段头。这指定了一个内存段，该段应该只包含指令，并且必须位于与任何其他数据完全分离的页面中。如果使用‘noseparate-code’，不要创建单独的PT_LOAD段。
- 'shstk'：在`.note.gnu`中生成`GNU_PROPERTY_X86_FEATURE_1_SHSTK`。属性部分以指示与Intel Shadow Stack的兼容性。支持Linux/i386和Linux/x86_64。
- 'stack-size=value'：指定一个ELF PT_GNU_STACK段的堆栈大小。指定零将覆盖任何默认的非零大小的PT_GNU_STACK段创建。
- 'start-stop-gc'、'nostart-stop-gc'：当‘--gc-sections’生效时，如果SECNAME可表示为C标识符，并且链接器合成了`__start_SECNAME`或`__stop_SECNAME`，则从保留节到`__start_SECNAME`或`__stop_SECNAME`的引用将导致所有名为SECNAME的输入节也被保留。‘-z start-stop-gc’禁用这种效果，允许对部分进行垃圾收集，就好像没有定义特殊的合成符号一样。‘-z start-stop-gc’对对象文件或链接器脚本中`__start_SECNAME`或`__stop_SECNAME`的定义没有影响。这样的定义将防止链接器分别提供合成的`__start_SECNAME`或`__stop_SECNAME`，因此垃圾收集对这些引用进行特殊处理。
- 'start-stop-visibility=value'：为合成的`__start_SECNAME`和`__stop_SECNAME`符号指定ELF符号可见性（参见输入部分示例）。值必须恰好是‘default’、‘internal’、‘hidden’或‘protected’。如果没有给出‘-z start-stop-visibility’选项，则使用‘protected’以与历史实践兼容。然而，强烈建议在新程序和共享库中使用‘-z start-stop-visibility=hidden’，这样这些符号就不会在共享对象之间导出，这通常不是我们想要的。
- 'text'、'notext'、'textoff'：如果设置了`DT_TEXTREL`，即，如果位置无关或共享对象在只读段中有动态重定位，则报告错误。如果‘notext’或‘textoff’，不要报告错误。
- 'undefs'：在创建可执行文件或创建共享库时，不要报告来自常规对象文件的未解析符号引用。这个选项与‘-z defs’相反。
- 'unique-symbol'、'nounique-symbol'：避免在符号字符串表中出现重复的局部符号名。添加'.number'如果使用‘unique-symbol’，则为重复的本地符号名称。唯一符号是默认值。
- 'x86-64-baseline'、'x86-64-v2'、'x86-64-v3'、'x86-64-v4'：指定`.note.gnu.property`段中所需的x86-64 ISA级别。`x86-64-baseline`生成`GNU_PROPERTY_X86_ISA_1_BASELINE`。`x86-64-v2`生成`GNU_PROPERTY_X86_ISA_1_V2`。`x86-64-v3`生成`GNU_PROPERTY_X86_ISA_1_V3`。`x86-64-v4`生成`GNU_PROPERTY_X86_ISA_1_V4`。支持Linux/i386和Linux/x86_64。
- 'isa-level-report=none'、'isa-level-report=all'、'isa-level-report=needed'、'isa-level-report=used'：指定如何在输入可重定位文件中报告x86-64 ISA级别。`isa-level-report=none`，这是默认值，将使链接器不在输入文件中报告x86-64 ISA级别。`isa-level-report=all`将在输入文件中生成所需和使用的x86-64 ISA级别的链接器报告。`isa-level-report=needed`将使链接器报告输入文件中所需的x86-64 ISA级别。`isa-level-report=used`将使链接器报告输入文件中使用的x86-64 ISA级别。支持Linux/i386和Linux/x86_64。
为了Solaris兼容性，忽略其他关键字。
