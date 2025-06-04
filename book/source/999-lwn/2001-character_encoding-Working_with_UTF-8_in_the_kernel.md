# Working with UTF-8 in the kernel

在现实世界中，文本用多种语言表达，使用各种字符集；这些字符集可以用许多不同的方式进行编码。在内核中，生活一直更简单；文件名和其他字符串数据只是不透明的字节流。在内核必须解释文本的少数情况下，只需要ASCII。但是，向ext4文件系统添加不区分大小写的文件名查找的提议改变了事情；现在一些内核代码必须处理Unicode的全部复杂性。查看一下用于处理编码的API，就能很好地说明这项任务有多复杂。

当然，Unicode标准定义了“码点”；近似地说，每个代码点表示特定语言组中的特定字符。这些码点如何在字节流中表示——编码——是一个单独的问题。处理编码本身就存在挑战，但多年来，UTF-8编码已经成为许多设置中表示代码点的首选方式。UTF-8具有表示整个Unicode空间的优点，同时与ASCII兼容——有效的ASCII字符串也是有效的UTF-8。在内核中实现大小写无关的开发人员决定将其限制为UTF-8编码，大概是希望在不完全发疯的情况下解决问题。

由此产生的API有两层：一组相对简单的高级操作和用于实现这些操作的原语。我们从顶部开始，然后往下走。

## 高级UTF-8 API

在高层次上，所需的操作可以相当简单地描述：验证字符串、规范化字符串和比较两个字符串（可能使用大小写折叠）。不过，这里有一个问题：Unicode标准有多个版本（12.0.0版本于3月初发布），每个版本都不同。标准化和案例折叠规则可以在不同版本之间改变，并且不是所有的代码点都存在于所有版本中。因此，在使用任何其他操作之前，必须为感兴趣的Unicode版本加载“map”：
```
struct unicode_map *utf8_load(const char *version);
```

给定的版本号可以为NULL，在这种情况下，将使用支持的最新版本并发出警告。在ext4实现中，用于任何给定文件系统的Unicode版本存储在超级块中。可以通过从`utf8version_latest()`获取最新版本的名称来显式请求最新版本，该方法不接受任何参数。`utf8_load()`的返回值是一个可以与其他操作一起使用的映射指针，或者是一个出错时的错误指针值。当不再需要返回的指针时，应该使用`utf8_unload()`释放它。

UTF-8字符串在本接口中使用<linux/dache.h>中定义的qstr结构来表示。这揭示了一个明显的假设，即该API的使用将仅限于文件系统代码；目前是这样，但将来可能会改变。

提供的单字符串操作有：
```c
int utf8_validate(const struct unicode_map *um, const struct qstr *str);
int utf8_normalize(const struct unicode_map *um, const struct qstr *str,
        unsigned char *dest, size_t dlen);
int utf8_casefold(const struct unicode_map *um, const struct qstr *str,
        unsigned char *dest, size_t dlen);
```

所有的函数都需要映射指针（um）和要操作的字符串（str）。如果str是有效的UTF-8字符串，`utf_validate()`返回零，否则返回非零。调用`utf8_normalize()`将在dest中存储str的规范化版本并返回结果的长度；`utf8_casefold()`进行大小写折叠和规范化。如果输入字符串无效或结果大于dlen，两个函数都将返回-EINVAL。

比较用：
```c
int utf8_strncmp(const struct unicode_map *um,
        const struct qstr *s1, const struct qstr *s2);
int utf8_strncasecmp(const struct unicode_map *um,
        const struct qstr *s1, const struct qstr *s2);
```

两个函数都将比较s1和s2的标准化版本；`utf8_strncasecmp()`将进行独立于大小写的比较。如果字符串相同，返回值为0，如果字符串不同，返回值为1，错误时返回`-EINVAL`。这些函数只检验相等性；没有“大于”或“小于”测试。

## Moving down(低一级API)

规范化和大小写折叠要求内核获得整个Unicode码点空间的详细知识。有很多规则，有很多表示代码点的方法。好消息是，这些规则以机器可读的形式与Unicode标准本身打包在一起。坏消息是它们占用了几兆字节的空间。

UTF-8补丁通过将提供的文件处理成C头文件中的数据结构来合并这些规则。然后，通过删除将韩文（韩语）代码点分解为其基本组件的信息，可以重新获得相当数量的空间，因为这也是一项可以通过算法完成的任务。但是，仍然有许多数据必须进入内核空间，并且每个版本的Unicode自然是不同的。

想要使用低级API的代码的第一步是获取一个指向该数据库的指针，以查找正在使用的Unicode版本。这是用其中之一完成的：
```c
struct utf8data *utf8nfdi(unsigned int maxage);
struct utf8data *utf8nfdicf(unsigned int maxage);
```
这里，maxage是感兴趣的版本号，使用`UNICODE_AGE()`宏将主版本号、次版本号和修订号以整数形式编码。如果只需要规范化，则应该调用`utf8nfdi()`；如果还需要折叠大小写，则使用`utf8nfdicf()`。返回值将是一个不透明的指针，如果不支持给定的版本，则返回值为NULL。

接下来，应该设置一个游标来跟踪处理感兴趣字符串的进度：
```c
int utf8cursor(struct utf8cursor *cursor, const struct utf8data *data,
        const char *s);
int utf8ncursor(struct utf8cursor *cursor, const struct utf8data *data,
        const char *s, size_t len);
```

游标结构必须由调用者提供，否则是不透明的；data是上面得到的数据库指针。如果字符串的长度（以字节为单位）已知，则应使用`utf8ncursor()`；`utf8cursor()`可以在长度未知但字符串以空结束的情况下使用。这些函数成功时返回零，否则返回非零。

然后通过重复调用：
```c
int utf8byte(struct utf8cursor *u8c);
```
此函数将返回规范化（可能折叠大小写）字符串中的下一个字节，或者返回末尾的零。当然，utf-8编码的码点可以占用多个字节，因此单个字节本身并不表示码点。由于分解，返回字符串可能比传入的字符串长。

下面是`utf8_strncasecmp()`的完整实现，作为这些部分如何组合在一起的示例：
```c
int utf8_strncasecmp(const struct unicode_map *um,
        const struct qstr *s1, const struct qstr *s2)
{
    const struct utf8data *data = utf8nfdicf(um->version);
    struct utf8cursor cur1, cur2;
    int c1, c2;
	
    if (utf8ncursor(&cur1, data, s1->name, s1->len) < 0)
        return -EINVAL;

    if (utf8ncursor(&cur2, data, s2->name, s2->len) < 0)
        return -EINVAL;

    do {
        c1 = utf8byte(&cur1);
        c2 = utf8byte(&cur2);

        if (c1 < 0 || c2 < 0)
            return -EINVAL;
        if (c1 != c2)
            return 1;
    } while (c1);

    return 0;
}
```
在低级API中还有其他用于测试有效性、获取字符串长度等的函数，但上面的函数捕获了它的核心。那些对细节感兴趣的人可以在这个补丁中找到它们。

当考虑到它只是用来比较字符串时，这是相当复杂的；我们现在已经远离Kernighan & Ritchie中发现的简单字符串函数了。但这似乎就是我们现在生活的世界。至少我们得到了一些漂亮的表情符号来表达所有的复杂性👍。

