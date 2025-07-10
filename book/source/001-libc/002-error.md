# 错误信息

GNU C 库中的许多函数会检测并报告错误条件，有时您的程序需要检查这些错误条件。例如，当您打开一个输入文件时，应验证文件是否已正确打开，并在库函数调用失败时打印错误消息或采取其他适当措施。
本章描述了错误报告机制的工作原理。您的程序应包含头文件 errno.h 以使用此机制。

大多数库函数会返回一个特殊值来指示它们已失败。该特殊值通常为-1、空指针或为该目的定义的常量（如EOF）。但该返回值仅告知您已发生错误。要确定具体是哪种错误，您需要查看存储在errno变量中的错误代码。该变量在头文件errno.h中声明。

变量 errno 包含系统错误编号。您可以修改 errno 的值。  

```c
volatile int errno;
```
由于 errno 被声明为 volatile，它可能会被信号处理程序异步修改；请参阅[定义信号处理程序]。然而，一个正确编写信号处理程序会保存并恢复 errno 的值，因此您通常无需担心这种可能性，除非在编写信号处理程序时。

程序启动时 errno 的初始值为零。在许多情况下，当库函数遇到错误时，它会将 errno 设置为非零值以指示具体错误条件。每个函数的文档列出了该函数可能遇到的错误条件。并非所有库函数都使用此机制；有些函数会直接返回错误代码。

> 警告：许多库函数即使未遇到任何错误，也可能将 errno 设置为某个无意义的非零值，即使它们直接返回错误代码也是如此。

因此，通过检查 errno 的值来判断是否发生错误通常是不正确的。检查错误的正确方法在每个函数的文档中都有说明。

可移植性说明：ISO C 将 errno 指定为“可修改的左值”而非变量，允许其以宏的形式实现。例如，其展开可能涉及函数调用，如 `*__errno_location()`。事实上，在 GNU/Linux 和 GNU/Hurd 系统中正是如此实现。GNU C 库在每个系统上都会根据具体系统情况采取合适的实现方式。

有一些库函数，如 sqrt 和 atan，在发生错误时会返回一个完全合法的值，同时也会设置 errno。对于这些函数，如果你想检查是否发生了错误，推荐的方法是在调用函数前将 errno 设置为零，然后在调用后检查其值。

所有错误代码都有符号名称；它们是 errno.h 文件中定义的宏。这些名称以字母 ‘E’ 和一个大写字母或数字开头；您应将此类名称视为保留名称。

## 错误码

## 错误信息

该库提供了专门设计的函数和变量，旨在让您的程序能够以惯用的格式轻松报告有关库调用失败的详细错误信息。函数 strerror 和 perror 可为您提供给定错误代码的标准错误信息；变量 `program_invocation_short_name` 可让您方便地获取遇到错误的程序名称。

### strerror 

```c
char * strerror (int errnum);
```

返回错误信息字符串

### strerror_l

```c
char * strerror_l (int errnum, locale_t locale);
```

返回错误信息是翻译过的

### strerror_r

```c
char * strerror_r (int errnum, char *buf, size_t n);
int strerror_r (int errnum, char *buf, size_t n);
```

strerror_r 函数与 strerror 函数类似，但它不返回由 GNU C 库管理的字符串指针，而是可以使用用户提供的缓冲区（从 buf 开始）来存储字符串。

### perror

```c
void perror (const char *message);
```

此函数将错误消息打印到标准错误流 stderr；参见[标准流]。stderr 的方向不会改变。

如果你调用 perror 时传入的错误消息是空指针或空字符串，perror 仅打印与 errno 对应的错误消息，并在末尾添加换行符。

### strerrorname_np

```c
const char * strerrorname_np (int errnum);
```

该函数返回描述错误 errnum 的名称，如果没有已知的常量具有此值，则返回 NULL（例如，EINVAL 对应 “EINVAL”）。返回的字符串在程序剩余的执行过程中不会改变。

该函数是 GNU 扩展，在头文件 string.h 中声明。

### strerrordesc_np

```c
const char * strerrordesc_np (int errnum);
```

该函数返回描述错误 errnum 的消息，如果没有已知的常量具有此值，则返回 NULL（例如，对于 EINVAL，返回“无效参数”）。与 strerror 不同，返回的描述未经过翻译，且返回的字符串在程序剩余执行过程中不会改变。
该函数是 GNU 扩展，在头文件 string.h 中声明。

### program_invocation_name

该变量的值是用于调用当前进程中运行的程序的名称。它与 argv[0] 相同。

### program_invocation_short_name

该变量的值是用于调用当前进程中运行的程序的名称，其中已移除目录名称。（也就是说，它等同于 program_invocation_name 减去最后一个斜杠之前的所有内容，如果有的话。）

### error

```c
void error (int status, int errnum, const char *format, ...);
```

- `error` 函数可用于在程序执行过程中报告一般性问题。
- `format` 参数是一个格式字符串，与printf函数家族中使用的格式字符串类似。格式所需的参数可跟在格式参数之后。

与perror类似，error函数也可将错误代码以文本形式报告。但与perror不同的是，错误值会通过errnum参数显式传递给函数。这消除了上述问题，即错误报告函数必须在引发错误的函数之后立即调用，否则 errno 可能具有不同的值。error 首先打印程序名称。如果应用程序定义了全局变量 `error_print_progname` 并将其指向一个函数，则该函数将被调用以打印程序名称。否则使用全局变量 `program_name` 中的字符串。程序名称后跟一个冒号和空格，随后是格式字符串生成的输出。如果 `errnum` 参数不为零，则格式字符串输出后跟一个冒号和空格，随后是错误代码 errnum 的错误消息。无论如何，输出以换行符结束。
输出将发送到 stderr 流。如果在调用前 stderr 未被定位，则调用后将被窄定位。
除非 status 参数的值不为零，否则函数将返回。在这种情况下，函数将调用 exit，并将 status 值作为其参数，因此不会返回。如果发生错误，全局变量 error_message_count 将增加 1 以跟踪已报告的错误数量。

### error_at_line

```c
void error_at_line (int status, int errnum, const char* fname,
        unsigned int lineno, const char *format, ...);
```

`error_at_line` 函数与 error 函数非常相似。唯一的区别是增加了 fname 和 lineno 两个参数。其他参数的处理方式与 error 函数完全相同，只是在程序名和格式字符串生成的字符串之间插入了额外文本。在程序名之后，会打印一个冒号，接着是 fname 指向的文件名，再一个冒号，最后是 lineno 的值。

当然，这个额外输出是用于定位输入文件中的错误（如编程语言源代码文件等）。
此额外输出当然旨在用于定位输入文件（如编程语言源代码文件等）中的错误。
如果全局变量 `error_one_per_line` 被设置为非零值，`error_at_line` 将避免为同一文件和行打印连续的消息。不直接相邻的重复情况不会被捕获。与 error 函数类似，该函数仅在 status 为零时返回。否则将调用 exit 并传入非零值。如果 error 返回，全局变量 `error_message_count` 将加一以记录已报告的错误数量。

### error_print_progname

```c
void (*error_print_progname) (void);
```

如果 error_print_progname 变量被定义为非零值，则由 error 或 error_at_line 调用的函数将被调用。该函数应打印程序名称或执行类似的有用的操作。
该函数应将输出打印到 stderr 流，并必须能够处理该流的任何方向。
该变量是全局变量，并由所有线程共享。

### error_message_count

错误消息计数变量（error_message_count）在调用error或error_at_line函数时会被递增。该变量是全局变量，并被所有线程共享。

### error_one_per_line

error_one_per_line 变量仅影响 error_at_line 函数。通常，error_at_line 函数会在每次调用时生成输出。如果 error_one_per_line 设置为非零值，error_at_line 函数会记录最后一个报告错误的文件名和行号，并避免直接跟进同一文件和行号的后续消息。该变量是全局变量，并由所有线程共享。

### warn

```c
void warn (const char *format, ...);
void vwarn (const char *format, va_list ap);
void warnx (const char *format, ...);
void vwarnx (const char *format, va_list ap);
```

warn 函数大致相当于以下调用：
```c
error (0, errno, format, param);
```
但该函数不会使用或修改全局变量 error。

### err

```c
void err (int status, const char *format, ...);
void verr (int status, const char *format, va_list ap);
void errx (int status, const char *format, ...);
void verrx (int status, const char *format, va_list ap)
```
