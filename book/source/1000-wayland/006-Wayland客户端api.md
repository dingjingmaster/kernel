# Wayland客户端API

Wayland 协议的开源参考实现分为两个 C 库：libwayland-client 和 libwayland-server。它们的主要职责是处理进程间通信（IPC），从而保证协议对象的编译和信息同步。

客户端使用 libwayland-client 与一个或多个 wayland 服务器通信。创建一个 `wl_display` 对象并管理与服务器的每个开放连接。每个 `wl_display` 对象至少创建一个 `wl_event_queue` 对象，用于保存从服务器接收到的事件，直到可以对其进行处理。通过为每个额外的线程创建一个额外的 `wl_event_queue`，可以支持多线程，每个对象都可以将其事件放置在一个特定的队列中，因此可以为每个创建的对象创建一个不同的线程来处理事件。

虽然提供了一些便利函数，但 libwayland-client 的设计允许调用代码等待事件发生，因此可以使用不同的轮询机制。libwayland-client 提供了一个文件描述符，当该文件描述符可供读取时，调用代码就可以要求 libwayland-client 将其中的可用事件读入 `wl_event_queue` 对象。

该库只提供对 wayland 对象的底层访问。客户端创建的每个对象都由该库创建的 `wl_proxy` 对象表示。该对象包括通过套接字与服务器实际通信的 `id`、一个 `void*` 数据指针（用于指向客户端对该对象的表示），以及一个指向静态 `wl_interface` 对象的指针（该对象由 xml 生成，可识别对象的类，并可用于对消息和事件的反省）。

消息通过调用 `wl_proxy_marshal` 发送。这将通过使用消息 ID 和 `wl_interface` 来识别每个参数的类型，并将其转换为流格式，从而将消息写入套接字。大多数软件都会调用从 Wayland 协议的 xml 描述中生成的类型安全封装器。例如，根据 xml 生成的 C 头文件定义了以下内联函数，用于传输 `wl_surface::attach` 消息： 

```c
static inline void 
wl_surface_attach(struct wl_surface *wl_surface, struct wl_buffer *buffer, int32_t x, int32_t y)
{
    wl_proxy_marshal((struct wl_proxy *) wl_surface, WL_SURFACE_ATTACH, buffer, x, y);
}
```

事件（来自服务器的消息）是通过调用客户端为每个事件存储在 `wl_proxy` 中的 “dispatcher” 回调来处理的。基于字符串的解释器（如 CPython）的语言绑定可能有一个调度器，它使用 `wl_interface` 中的事件名称来确定要调用的函数。默认的派发器使用消息 `id` 编号来索引函数指针数组（称为 `wl_listener`），并使用 `wl_interface` 将数据流中的数据转换为函数的参数。从 xml 生成的 C 头文件定义了一个按类划分的结构，它强制函数指针采用正确的类型，例如 `wl_surface::enter` 事件就在 `wl_surface_listener` 对象中定义了这个指针： 

```c
struct wl_surface_listener 
{
    void (*enter)(void *data, struct wl_surface *, struct wl_output *);
    //...
}
```

## wl_argument — 协议报文参数数据类型

该数据类型表示 Wayland 协议线格式中的所有参数类型。协议实现使用 `wl_argument`，在客户端与合成器之间调度信息。

## wl_array - 动态数组

`wl_array` 是一个动态数组，在释放之前只能增长。它适用于大小可变或事先不知道大小的相对较小的分配。虽然 `wl_array` 的构造并不要求所有元素都具有相同的大小，但 `wl_array_for_each()` 却要求所有元素都具有相同的类型和大小。

### 数组大小

```c
size_t wl_array::size
```

### 分配空间

```c
size_t wl_array::alloc
```

### 数组数据

```c
void* wl_array::data
```

### 初始化数据

```c
void wl_array_init(struct wl_array *array)
```

### 释放数组数据

```c
void wl_array_release(struct wl_array *array)
```

### 添加数据元素

```c
void * wl_array_add(struct wl_array *array, size_t size)
```
- array：Array whose size is to be increased 
- size：Number of bytes to increase the size of the array by 
- Returns: A pointer to the beginning of the newly appended space, or NULL when resizing fails. 

### 复制数组

```c
int wl_array_copy(struct wl_array *array, struct wl_array *source)
```
- array：Destination array to copy to 
- source：Source array to copy from 
- Returns: 0 on success, or -1 on failure 

### 数组遍历

```c
wl_array_for_each(size_t pos, struct wl_array* array)
```

## wl_display — 代表与合成器的连接，并充当 wl_display 单例对象的代理

`wl_display` 对象表示与 Wayland 合成器的客户端连接。它是通过 `wl_display_connect()` 或 `wl_display_connect_too_fd()` 创建的。使用 `wl_display_disconnect()` 可以终止连接。

在合成器端，`wl_display` 也被用作 `wl_display` 单例对象的 `wl_proxy`。

`wl_display` 对象负责处理从合成器发送和接收的所有数据。当 `wl_proxy` 对一个请求进行编译时，它会将其线性表示写入显示屏的写缓冲区。当客户端调用 `wl_display_flush()`时，数据就会被发送到合成器。

传入数据的处理分为两个步骤：排队和分派。在队列步骤中，来自显示 `fd` 的数据会被解释并添加到队列中。在调度步骤中，会调用客户端在相应的 `wl_proxy` 上设置的传入事件处理程序。

一个 `wl_display` 至少有一个事件队列，称为默认队列。客户端可以使用 `wl_display_create_queue()` 创建额外的事件队列，并将 `wl_proxy` 分配给它。在特定代理中发生的事件总是在其分配的队列中排队。客户端可以通过将代理分配到事件队列，并确保只有在假设成立时才调度该队列，从而确保在调用代理事件处理程序时，某个假设（如持有锁或从指定线程运行）为真。

默认队列通过调用 `wl_display_dispatch()` 进行分派。这将分派默认队列中的任何事件，如果显示 `fd` 为空，则尝试从显示 `fd` 中读取。读取的事件将根据代理分配在相应的队列中排队。

用户创建的队列通过 `wl_display_dispatch_queue()` 进行分派。该函数的行为与 `wl_display_dispatch()` 完全相同，但它调度的是指定队列而不是默认队列。

使用事件队列的一个实际例子是 Mesa 为 Wayland 平台实现的 `eglSwapBuffers()`。该函数可能需要阻塞，直到收到帧回调，但分派默认队列可能会导致客户端的事件处理程序重新开始绘制。使用另一个事件队列可以解决这个问题，这样在阻塞期间，只有 EGL 代码处理的事件才会被分派。

这就产生了一个问题，即线程会调度一个非默认队列，从显示 `fd` 读取所有数据。如果应用程序在此之后调用 `poll(2)`，就会阻塞，即使默认队列中可能有排队的事件。在刷新和阻塞之前，应该使用 `wl_display_dispatch_pending()` 或 `wl_display_dispatch_queue_pending()` 来派发这些事件。

### wl_display_create_queue - 为该显示器创建一个新的事件队列

```c
struct wl_event_queue * wl_display_create_queue(struct wl_display *display)
```
- display：The display context object 
- Returns：A new event queue associated with this display or NULL on failure. 

### wl_display_create_queue_with_name - 为该显示创建一个新的事件队列并为其命名

```c
struct wl_event_queue * wl_display_create_queue_with_name(struct wl_display *display, const char *name)
```
- display：The display context object 
- name：A human readable queue name 
- Returns: A new event queue associated with this display or NULL on failure. 

### wl_display_connect_too_fd - 在已打开的 fd 上连接 Wayland 显示器

```c
struct wl_display * wl_display_connect_to_fd(int fd)
```
- fd：The fd to use for the connection 
- Returns：A wl_display object or NULL on failure 

wl_display 拥有 fd 的所有权，并将在销毁显示器时关闭 fd。如果出现故障，也会关闭 fd

### wl_display_connect - 连接到 Wayland 显示器

```c
struct wl_display * wl_display_connect(const char *name)
```
- name：Name of the Wayland display to connect to 
- returns: A wl_display object or NULL on failure 

连接到名为 `name` 的 `Wayland` 显示屏。如果 `name` 为空，其值将被设置了 `WAYLAND_DISPLAY` 的环境变量取代，否则将使用显示 `wayland-0`。

如果设置了 `WAYLAND_SOCKET`，它将被解释为指向已打开套接字的文件描述符编号。在这种情况下，套接字将按原样使用，名称将被忽略。

如果 name 是相对路径，则套接字是相对于 `XDG_RUNTIME_DIR` 目录打开的。

如果 name 是绝对路径，那么 Wayland 服务器监听的套接字位置将使用该路径，而不会在 `XDG_RUNTIME_DIR` 内进行限定。

如果 name 为空，而 `WAYLAND_DISPLAY` 环境变量被设置为绝对路径名，则该路径名会被原样用于套接字，使用方式与 name 中的绝对路径相同。从 Wayland 1.15 版开始，name 和 `WAYLAND_DISPLAY` 均支持绝对路径

### wl_display_disconnect - 关闭与 Wayland 显示器的连接

```c
void wl_display_disconnect(struct wl_display *display)
```
- display：The display context object 

关闭显示连接。在断开连接之前，调用者需要手动销毁 wl_proxy 和 wl_event_queue 对象

### wl_display_get_fd - 获取显示上下文的文件描述符

```c
int wl_display_get_fd(struct wl_display *display)
```
- display：The display context object 
- Returns：Display object file descriptor 

返回与显示相关的文件描述符，以便将其整合到客户端的主循环中

### wl_display_roundtrip_queue - 在服务器处理完所有待处理请求之前一直阻塞

```c
int wl_display_roundtrip_queue(struct wl_display *display, struct wl_event_queue *queue)
```
- display： The display context object 
- queue：The queue on which to run the roundtrip 
- Returns: The number of dispatched events on success or -1 on failure 

该函数通过向显示服务器发送请求并等待回复后返回，从而阻塞直到服务器处理完当前发出的所有请求。

此函数内部使用 `wl_display_dispatch_queue()`。不允许在线程准备读取事件时调用此函数，否则会导致死锁。

注意：此函数可能会调度给定队列上正在接收的其他事件。另请参见：`wl_display_roundtrip()` 

### wl_display_read_events - 从显示文件描述符读取事件

```c
int wl_display_read_events(struct wl_display *display)
```
- display：The display context object 
- Returns: 0 on success or -1 on error. In case of error errno will be set accordingly 

调用该函数后，显示文件描述符上的可用数据将被读取，读取事件将在相应的事件队列中排队。

在调用此函数之前，需要调用 `wl_display_prepare_read_queue()` 或 `wl_display_prepare_read()`，具体取决于从哪个线程调用。详情请参阅 `wl_display_prepare_read_queue()`。

如果在其他线程已准备好读取（使用 `wl_display_prepare_read_queue()` 或 `wl_display_prepare_read()`）的情况下调用此函数，该函数将处于休眠状态，直到所有其他准备好的线程都被取消（使用 `wl_display_cancel_read()`）或自己进入此函数。然后，调用此函数的最后一个线程将读取事件并在其相应的事件队列中排队，最后唤醒所有其他 `wl_display_read_events()` 调用，使它们返回。

如果一个线程在所有其他准备读取的线程都调用了 `wl_display_cancel_read()` 或 `wl_display_read_events()` 之后取消了读取准备，那么所有读取线程都将在没有读取任何数据的情况下返回。

要分派可能已经排队的事件，请调用 `wl_display_dispatch_pending()` 或 `wl_display_dispatch_queue_pending()`。

另请参见：`wl_display_prepare_read()`, `wl_display_cancel_read()`, `wl_display_dispatch_pending()`, `wl_display_dispatch()`

### wl_display_prepare_read_queue - 准备将事件从显示器的文件描述符读取到队列中

```c
int wl_display_prepare_read_queue(struct wl_display *display, struct wl_event_queue *queue)
```
- display：The display context object 
- queue：The event queue to use 
- Returns: 0 on success or -1 if event queue was not empty 

在使用 `wl_display_read_events()` 从文件描述符读取数据之前，必须调用此函数（或 `wl_display_prepare_read()`）。调用 `wl_display_prepare_read_queue()` 会宣布调用线程的读取意图，并确保在该线程准备好读取并调用 `wl_display_read_events()` 之前，不会有其他线程从文件描述符中读取数据。只有当事件队列为空时，此操作才会成功，否则将返回 `-1` 并将 `errno` 设置为 `EAGAIN`。

如果一个线程成功调用了 `wl_display_prepare_read_queue()`，它必须在准备就绪时调用 `wl_display_read_events()`，或者通过调用 `wl_display_cancel_read()` 取消读取意图。

在对显示 fd 进行轮询之前使用该函数，或以无竞争方式将 fd 集成到工具包事件循环中。正确的用法应该是（省略大部分错误检查）：
```c
while (wl_display_prepare_read_queue(display, queue) != 0) {
    wl_display_dispatch_queue_pending(display, queue);
}
wl_display_flush(display);

ret = poll(fds, nfds, -1);
if (has_error(ret)) {
    wl_display_cancel_read(display);
}
else {
    wl_display_read_events(display);
}

wl_display_dispatch_queue_pending(display, queue);
```

在这里，我们调用了 `wl_display_prepare_read_queue()`，以确保在从该调用返回到最终调用 `wl_display_read_events()` 期间，不会有其他线程从 `fd` 读取事件并将事件排入我们的队列。如果调用 `wl_display_prepare_read_queue()` 失败，我们会分派待处理的事件并再次尝试，直到成功为止。

`wl_display_prepare_read_queue()` 函数不会获取对显示器 fd 的独占访问权限。它只是注册调用此函数的线程有意从 fd 读取数据。当所有已注册的读取器都调用 `wl_display_read_events()` 时，只有一个（随机的）读取器会最终读取并队列事件，而其他读取器则处于休眠状态。这样就避免了竞赛，而且还能从更多线程读取数据。

另请参见：`wl_display_cancel_read()`, `wl_display_read_events()`, `wl_display_prepare_read()`

### wl_display_prepare_read - 准备从显示器的文件描述符中读取事件

```c
int wl_display_prepare_read(struct wl_display *display)
```
- display：The display context object 
- Returns：0 on success or -1 if event queue was not empty 

该函数的功能与 `wl_display_prepare_read_queue()` 相同，但将默认队列作为队列传入

### wl_display_cancel_read - 取消对显示器 fd 的读取意图

```c
void wl_display_cancel_read(struct wl_display *display)
```
- display：The display context object 

线程成功调用 `wl_display_prepare_read()` 后，必须调用 `wl_display_read_events()` 或 `wl_display_cancel_read()`。如果线程不遵守这一规则，就会导致死锁

### wl_display_dispatch_queue - 在事件队列中调度事件

```c
int wl_display_dispatch_queue(struct wl_display *display, struct wl_event_queue *queue)
```
- display：The display context object 
- queue：The event queue to dispatch 
- Returns：The number of dispatched events on success or -1 on failure 

在给定的事件队列中调度事件。

如果给定的事件队列为空，该函数将阻塞，直到有事件要从显示 fd 读取。事件被读取并在相应的事件队列中排队。最后，调度给定事件队列中的事件。如果失败，将返回 -1 并适当设置 errno。

在多线程环境中，调用此函数前请勿使用 `poll()`（或类似函数）手动等待，否则可能会导致死锁。如果需要从外部依赖 `poll()`（或类似函数），请参阅 `wl_display_prepare_read_queue()`，了解如何做到这一点。

只要在正确的线程上派发正确的队列，该函数就是线程安全的。它也兼容多线程事件读取准备 API（请参阅 `wl_display_prepare_read_queue()`），并在内部使用同等功能。当线程正在准备读取事件时，不允许调用此函数，否则会导致死锁。

该函数可用作辅助函数，以简化读取和分派事件的过程。

注意：自 Wayland 1.5 版起，显示器有了一个额外的队列来处理自己的事件（即 `delete_id`）。无论我们将哪个队列作为参数传递给该函数，该队列始终都会被分派。这意味着，即使没有为给定队列派发任何事件，该函数也可以返回非 0 值。另请参见：`wl_display_dispatch()`, `wl_display_dispatch_pending()`, `wl_display_dispatch_queue_pending()`, `wl_display_prepare_read_queue()`

### wl_display_dispatch_queue_pending - 在事件队列中调度待处理事件

```c
int wl_display_dispatch_queue_pending(struct wl_display *display, struct wl_event_queue *queue)
```
- display：The display context object 
- queue：The event queue to dispatch 
- Returns:The number of dispatched events on success or -1 on failure 

调度分配给给定事件队列的对象的所有传入事件。如果失败，将返回 -1 并适当设置 errno。如果没有事件队列，则立即返回。

Since：1.0.2

### wl_display_dispatch - 处理接收到的事件

```c
int wl_display_dispatch(struct wl_display *display)
```
- display：The display context object
- Returns: The number of dispatched events on success or -1 on failure

在默认事件队列中调度事件。

如果默认事件队列为空，该函数将阻塞，直到有事件要从显示 fd 读取。事件被读取并在相应的事件队列中排队。最后，对默认事件队列中的事件进行分派。如果失败，将返回 -1 并适当设置 errno。

在多线程环境中，调用此函数前请勿使用 `poll()`（或类似函数）手动等待，否则可能会导致死锁。如果需要从外部依赖 `poll()`（或类似函数），请参阅 `wl_display_prepare_read_queue()`，了解如何做到这一点。

只要在正确的线程上派发正确的队列，该函数就是线程安全的。它也兼容多线程事件读取准备 API（请参阅 `wl_display_prepare_read_queue()`），并在内部使用同等功能。当线程正在准备读取事件时，不允许调用此函数，否则会导致死锁。

注意：无法检查队列中是否有事件。要在不阻塞的情况下分派默认队列事件，请参阅 `wl_display_dispatch_pending()`。另请参见：`wl_display_dispatch_pending()`, `wl_display_dispatch_queue()`, `wl_display_read_events()`

### wl_display_dispatch_pending - 在不读取显示器 fd 的情况下调度默认队列事件

```c
int wl_display_dispatch_pending(struct wl_display *display)
```
- display：The display context object 
- Returns：The number of dispatched events or -1 on failure 

该函数在主事件队列上派发事件。它不会尝试读取显示 fd，如果主队列为空，则直接返回 0，即不会阻塞。

另请参见：`wl_display_dispatch()`, `wl_display_dispatch_queue()`, `wl_display_flush()`

### wl_display_get_error - 读取显示屏上发生的最后一次错误

```c
int wl_display_get_error(struct wl_display *display)
```
- Returns：The last error that occurred on display or 0 if no error occurred 

返回显示屏上出现的最后一个错误。这可能是服务器发送的错误，也可能是本地客户端造成的错误

### wl_display_get_protocol_error - 读取有关协议错误的信息

```c
uint32_t wl_display_get_protocol_error(struct wl_display *display, const struct wl_interface **interface, uint32_t *id)
```
- display：The Wayland display 
- interface：if not NULL, stores the interface where the error occurred, or NULL, if unknown. 
- id：if not NULL, stores the object id that generated the error, or 0, if the object id is unknown. There's no guarantee the object is still valid; the client must know if it deleted the object. 
- Returns：The error code as defined in the interface specification. 

```c
int err = wl_display_get_error(display);

if (err == EPROTO) {
    code = wl_display_get_protocol_error(display, &interface, &id);
    handle_error(code, interface, id);
}
```

### wl_display_flush - 将显示屏上的所有缓冲请求发送到服务器

```c
int wl_display_flush(struct wl_display *display)
```
- display：The display context object 
- Returns：The number of bytes sent on success or -1 on failure 

将客户端的所有缓冲数据发送到服务器。客户端应始终在阻塞显示 fd 的输入之前调用此函数。成功时，将返回发送到服务器的字节数。如果失败，则返回 -1 并适当设置 errno。

`wl_display_flush()` 从不阻塞。它会尽可能多地写入数据，但如果无法写入所有数据，则会将 errno 设置为 EAGAIN 并返回-1。在这种情况下，请在显示文件描述符上使用轮询功能，以等待它再次成为可写文件。

### wl_display_set_max_buffer_size - 调整客户端连接缓冲区的最大大小

```c
void wl_display_set_max_buffer_size(struct wl_display *display, size_t max_buffer_size)
```
- display：The display context object 
- max_buffer_size：The maximum size of the connection buffers 

客户端缓冲区默认是无限制的。此函数将限制连接缓冲区的大小。

如果 `max_buffer_size` 值为 0，则要求缓冲区不受限制。

连接缓冲区的实际大小是 2 的幂次，因此请求的 `max_buffer_size` 会四舍五入为最接近的 2 的幂次值。

如果缓冲区的当前内容无法满足新的大小限制，降低最大大小可能不会立即生效

Since: 1.22.90

## wl_event_queue - wl_proxy 对象事件的队列

事件队列允许以线程安全的方式处理显示屏上的事件。详情请参见 wl_display。

### wl_event_queue_destroy - 销毁一个事件队列

```c
void wl_event_queue_destroy(struct wl_event_queue *queue)
```
- queue：The event queue to be destroyed 

销毁给定的事件队列。该队列中的任何待处理事件都将被丢弃。

用于创建队列的 `wl_display` 对象不应被销毁，直到用该函数销毁了用它创建的所有事件队列

## wl_interface - 协议对象接口

`wl_interface` 描述了 Wayland 协议规范中定义的协议对象的 API。协议实现在其 marshalling 机制中使用 `wl_interface` 对客户端请求进行编码。

`wl_interface` 的名称是相应协议接口的名称，而版本则代表该接口的版本。成员 `method_count` 和 `event_count` 分别代表相应 `wl_message` 成员中方法（请求）和事件的数量。

例如，考虑一个标记为版本 1 的协议接口 foo，它有两个请求和一个事件。
```xml
<interface name="foo" version="1">
  <request name="a"></request>
  <request name="b"></request>
  <event name="c"></event>
</interface>
```

给定两个 `wl_message` 数组 `foo_requests` 和 `foo_events` 后，`foo` 的 `wl_interface` 可能是这样的：

```c
struct wl_interface foo_interface = {
    "foo", 1,
    2, foo_requests,
    1, foo_events
};
```

注意：协议的服务器端可能会定义接口实现类型，在其名称中包含接口一词。注意不要将这些服务器端结构体与名称也以 interface 结尾的 `wl_interface` 变量混淆。例如，服务器可定义 `struct wl_foo_interface` 类型，而客户端可定义 `struct wl_interface wl_fo_interface`。另请参见： `wl_message` 另请参见： `wl_proxy` 

## wl_list - 双向链表

结构 `wl_list` 实例本身代表双链表的表头，必须使用 `wl_list_init()` 进行初始化。当为空时，列表头的 `next` 和 `prev` 成员指向列表头本身，否则 `next` 指向列表中的第一个元素，而 `prev` 指向列表中的最后一个元素。

使用 `struct wl_list` 类型来表示列表头和列表中元素之间的链接。使用 `wl_list_empty()` 可以在 `O(1)` 内判断列表是否为空。

列表中的所有元素必须具有相同的类型。元素类型必须有一个 `struct wl_list` 成员，通常按惯例命名为 `link`。在插入之前，没有必要初始化元素的链接--如果下一个操作是 `wl_list_insert()`，那么就没有必要在单个列表元素的 `struct wl_list` 成员上调用 `wl_list_init()`。然而，一个常见的习惯做法是在移除元素之前初始化其链接--在 `wl_list_remove()` 之前调用 `wl_list_init()` 可以确保安全。

考虑一个列表引用 `struct wl_list foo_list`，一个元素类型为 `struct element`，一个元素的链接成员为 `struct wl_list link`。

以下代码初始化了一个列表，并向其中添加了三个元素。

下面的代码初始化了一个 list，并向其中添加了三个元素。

```c
struct wl_list foo_list;

struct element {
    int foo;
    struct wl_list link;
};
struct element e1, e2, e3;

wl_list_init(&foo_list);
wl_list_insert(&foo_list, &e1.link);   // e1 is the first element
wl_list_insert(&foo_list, &e2.link);   // e2 is now the first element
wl_list_insert(&e2.link, &e3.link); // insert e3 after e2
```
The list now looks like [e2, e3, e1].

`wl_list` API 提供了一些迭代宏。例如，按升序迭代一个列表：
```c
struct element *e;
wl_list_for_each(e, foo_list, link) {
    do_something_with_element(e);
}
```
### prev - 上一个列表元素

```c
struct wl_list* wl_list::prev
```

### next - 下一个列表元素

```c
struct wl_list* wl_list::next
```

### wl_list_init - 初始化列表

```c
void wl_list_init(struct wl_list *list)
```

### wl_list_insert - 在 list 所代表的元素之后向 list 插入一个元素

```c
void wl_list_insert(struct wl_list *list, struct wl_list *elm)
```
当 list 是对 list 本身（头部）的引用时，将 elm 的包含结构设置为 list 中的第一个元素

### wl_list_remove - 从列表中删除一个元素

```c
void wl_list_remove(struct wl_list *elm)
```

### wl_list_length - 确定列表的长度

```c
int wl_list_length(const struct wl_list *list)
```

### wl_list_empty - 确定列表是否为空

```c
int wl_list_empty(const struct wl_list *list)
```

### wl_list_insert_list - 将一个列表中的所有元素插入另一个列表中，位于 list 所代表的元素之后

```c
void wl_list_insert_list(struct wl_list *list, struct wl_list *other)
```

### wl_list_for_each - 遍历列表

```c
struct message {
    char *contents;
    wl_list link;
};

struct wl_list *message_list;
// Assume message_list now "contains" many messages

struct message *m;
wl_list_for_each(m, message_list, link) {
    do_something_with_message(m);
}
```
### wl_list_for_each_safe - 遍历一个列表，安全地防止删除列表元素

### wl_list_for_each_reverse - 对列表进行反向遍历

### wl_list_for_each_reverse_safe - 对列表进行反向遍历，防止删除列表元素

## wl_message - 协议报文签名

`wl_message` 描述了符合 Wayland 协议线格式的实际协议消息（如请求或事件）的签名。协议实现在其解码机制中使用 `wl_message`，用于解码合成器与其客户端之间的信息。从某种意义上说，`wl_message` 对于协议信息来说就像类对于对象一样。

`wl_message` 的名称就是相应协议消息的名称。

签名是一个有序的符号列表，代表报文参数的数据类型，也可选择代表协议版本和无效性指标。签名中的前导整数表示协议信息的版本。数据类型符号前的? 表示后面的参数类型是可归零的。虽然将非空值参数设置为 NULL 会违反协议发送信息，但客户端中的事件处理程序仍可能在非空值对象参数设置为 NULL 的情况下被调用。当客户端销毁了用作其参数的对象，而引用该对象的事件在服务器知道其销毁之前就已发送时，就可能发生这种情况。由于这种竞赛无法避免，因此客户机在对事件处理程序进行编程时，一般应确保能从容地处理已声明为 NULL 的不可空对象参数。

当消息中没有参数时，签名为空字符串

符号：
- i: int
- u: uint
- f: fixed
- s: string
- o: object
- n: new_id
- a: array
- h: fd
- ?: following argument (o or s) is nullable 

虽然对原始参数进行解屏蔽非常简单，但在对包含对象或 new_id 参数的消息进行解屏蔽时，协议实现通常必须确定对象的类型。wl_message 的类型是一个 wl_interface 引用数组，对应于签名中的 o 和 n 个参数，NULL 占位符表示非对象类型的参数。

请看协议事件 `wl_display` `delete_id`，它只有一个 uint 参数。`wl_message` 为：
```
{ "delete_id", "u", [NULL] }
```

在这里，消息名称是 “delete_id”，签名是 “u”，参数类型是[NULL]，表示 uint 参数没有对应的 wl_interface，因为它是一个原始参数。
与此相反，考虑一个从第 2 版开始就存在的支持协议请求栏的 wl_foo 接口，它有两个参数：一个 uint 和一个 wl_baz_interface 类型的对象，后者可能是 NULL。这样的 wl_message 可能是
```
{ "bar", "2u?o", [NULL, &wl_baz_interface] }
```

这里的报文名称是 “bar”，签名是 “2u?o”。请注意，“2 ”表示协议版本，“u ”表示第一个参数类型是 uint，“?o ”表示第二个参数是一个可能为 NULL 的对象。最后，参数类型数组表示第一个参数不对应 wl_interface，而第二个参数对应 wl_baz_interface 类型。

另请参见：wl_argument 另请参见：wl_interface 另请参见：wl_interface

## wl_object - 协议对象

wl_object 是一个不透明结构，用于标识 wl_proxy 或 wl_resource 底层的协议对象。

## wl_proxy - 代表客户端的协议对象

`wl_proxy` 充当客户端对合成器中存在的对象的代理。代理负责将客户端通过 `wl_proxy_marshal()`发出的请求转换为 Wayland 的线格式。来自合成器的事件也由代理处理，代理会调用用 `wl_proxy_add_listener()`设置的处理程序。

注意：除了函数 `wl_proxy_set_queue()`，客户端代码通常不会使用访问 `wl_proxy` 的函数。客户端通常应使用扫描仪生成的高级接口与合成器对象交互。

### wl_proxy_create - 用给定接口创建代理对象

```c
struct wl_proxy * wl_proxy_create(struct wl_proxy *factory, const struct wl_interface *interface)
```

该函数使用提供的接口创建一个新的代理对象。代理对象将有一个从客户端 id 空间分配的 id。该 id 应通过使用 `wl_proxy_marshal()`发送适当请求在合成器端创建。

代理将继承工厂对象的显示和事件队列。

### wl_proxy_destroy - 销毁代理对象

```c
void wl_proxy_destroy(struct wl_proxy *proxy)
```

### wl_proxy_add_listener - 设置代理的监听器

```c
int wl_proxy_add_listener(struct wl_proxy *proxy, void(**implementation)(void), void *data)
```

将代理的监听器设置为实现，将用户数据设置为数据。如果已经设置了监听器，则此函数将失效，并且不会有任何更改。

implementation 是一个函数指针向量。对于操作码 n，implementation[n] 应指向给定对象的 n 处理程序

### wl_proxy_get_listener - 获取代理的监听器

```c
const void * wl_proxy_get_listener(struct wl_proxy *proxy)
```

获取代理的监听器地址，即通过 `wl_proxy_add_listener` 设置的监听器地址。

该函数适用于在同一接口上有多个监听器的客户端，以便识别要执行的代码。

### wl_proxy_add_dispatcher - 设置代理的监听器（带派发器）

```c
int wl_proxy_add_dispatcher(struct wl_proxy *proxy, wl_dispatcher_func_t dispatcher, const void *implementation, void *data)
```

设置代理的监听器，使其使用 `dispatcher_func` 作为派发器，使用 `dispatcher_data` 作为派发器的特定实现，并将用户数据设为 data。如果已经设置了监听器，则此函数将失效，不会有任何改变。

`dispatcher_data` 的具体细节取决于所使用的调度程序。该函数主要用于语言绑定，而非用户代码

### wl_proxy_marshal_array_constructor - 准备发送给合成器的请求

```c
struct wl_proxy * wl_proxy_marshal_array_constructor(struct wl_proxy *proxy, uint32_t opcode, union wl_argument *args, const struct wl_interface *interface)
```

该函数将给定操作码、接口和 wl_argument 数组的请求转换成线格式，并将其写入连接缓冲区。

对于 new-id 参数，该函数将分配一个新的 wl_proxy，并将 ID 发送给服务器。成功时将返回新的 wl_proxy，错误时则返回 NULL，并相应设置 errno。新创建的代理将继承父代理的版本

### wl_proxy_marshal_array_constructor_versioned - 准备发送给合成器的请求

```c
struct wl_proxy * wl_proxy_marshal_array_constructor_versioned(struct wl_proxy *proxy, uint32_t opcode, union wl_argument *args, const struct wl_interface *interface, uint32_t version)
```

将操作码和额外参数给出的请求转换为电线格式，并将其写入连接缓冲区。该版本接收一个联合类型为 wl_argument 的数组。

对于 new-id 参数，该函数将分配一个新的 wl_proxy，并将 ID 发送给服务器。成功时将返回新的 wl_proxy，错误时则返回 NULL，并相应设置 errno。新创建的代理将具有指定的版本

### wl_proxy_marshal_flags - 准备发送给合成器的请求

```c
struct wl_proxy * wl_proxy_marshal_flags(struct wl_proxy *proxy, uint32_t opcode, const struct wl_interface *interface, uint32_t version, uint32_t flags,...)
```

将由操作码和额外参数给出的请求转换为电线格式，并将其写入连接缓冲区。

对于 new-id 参数，该函数将分配一个新的 wl_proxy，并将 ID 发送给服务器。成功时将返回新的 wl_proxy，错误时则返回 NULL，并相应设置 errno。新创建的代理将具有指定的版本。

可以通过 WL_MARSHAL_FLAG_DESTROY 标志来确保代理与 marshalling 同时原子销毁，以防止在 marshal 和销毁操作之间丢弃显示锁时发生竞赛

### wl_proxy_marshal_array_flags - 准备发送给合成器的请求

```c
struct wl_proxy * wl_proxy_marshal_array_flags(struct wl_proxy *proxy, uint32_t opcode, const struct wl_interface *interface, uint32_t version, uint32_t flags, union wl_argument *args)
```

将操作码和额外参数给出的请求转换为电线格式，并将其写入连接缓冲区。该版本接收一个联合类型为 wl_argument 的数组。

对于 new-id 参数，该函数将分配一个新的 wl_proxy，并将 ID 发送给服务器。成功时将返回新的 wl_proxy，错误时则返回 NULL，并相应设置 errno。新创建的代理将具有指定的版本。

可以传递 WL_MARSHAL_FLAG_DESTROY 标志，以确保代理与 marshalling 同时原子销毁，从而防止在 marshal 和销毁操作之间丢弃显示锁时发生竞赛

### wl_proxy_marshal - 准备发送给合成器的请求

```c
void wl_proxy_marshal(struct wl_proxy *proxy, uint32_t opcode,...)
```

该函数类似于 wl_proxy_marshal_constructor()，但它不会为 new-id 参数创建代理

### wl_proxy_marshal_constructor - 准备发送给合成器的请求

```c
struct wl_proxy * wl_proxy_marshal_constructor(struct wl_proxy *proxy, uint32_t opcode, const struct wl_interface *interface,...)
```

 该函数将给定操作码、接口和额外参数的请求转换为导线格式，并将其写入连接缓冲区。额外参数的类型必须与接口中与操作码相关的方法的参数类型一致。

对于 new-id 参数，该函数将分配一个新的 wl_proxy，并将 ID 发送给服务器。成功时将返回新的 wl_proxy，错误时则返回 NULL，并相应设置 errno。新创建的代理将继承父代理的版本

### wl_proxy_marshal_constructor_versioned - 准备发送给合成器的请求

```c
struct wl_proxy * wl_proxy_marshal_constructor_versioned(struct wl_proxy *proxy, uint32_t opcode, const struct wl_interface *interface, uint32_t version,...)
```

将由操作码和额外参数给出的请求转换为电线格式，并将其写入连接缓冲区。

对于 new-id 参数，该函数将分配一个新的 wl_proxy，并将 ID 发送给服务器。成功时将返回新的 wl_proxy，错误时则返回 NULL，并相应设置 errno。新创建的代理将具有指定的版本

### wl_proxy_marshal_array - 准备发送给合成器的请求

```c
void wl_proxy_marshal_array(struct wl_proxy *proxy, uint32_t opcode, union wl_argument *args)
```

该函数类似于 wl_proxy_marshal_array_constructor()，但它不会为 new-id 参数创建代理

### wl_proxy_set_user_data - 设置与代理相关联的用户数据

```c
void wl_proxy_set_user_data(struct wl_proxy *proxy, void *user_data)
```
设置与代理相关的用户数据。当收到该代理的事件时，将向其监听器提供 user_data

### wl_proxy_get_user_data - 获取与代理相关联的用户数据

```c
void * wl_proxy_get_user_data(struct wl_proxy *proxy)
```

### wl_proxy_get_version - 获取代理对象的协议对象版本

```c
uint32_t wl_proxy_get_version(struct wl_proxy *proxy)
```

获取代理对象的协议对象版本，如果代理是通过未版本控制的 API 创建的，则返回 0。

返回值为 0 意味着没有可用的版本信息，因此调用者必须对对象的真实版本做出安全假设。

### wl_proxy_get_id - 获取代理对象的 ID

```c
uint32_t wl_proxy_get_id(struct wl_proxy *proxy)
```

### wl_proxy_set_tag - 设置代理对象的标记

```c
void wl_proxy_set_tag(struct wl_proxy *proxy, const char *const *tag)
```

工具包或应用程序可以在代理上设置一个唯一标签，以识别对象是由其自身管理还是由外部部分管理。

要创建标签，推荐的方法是定义一个静态分配的常量字符数组，其中包含一些描述性字符串。标签将是指向数组开头的非常数指针。

例如，在某个子系统管理的曲面上定义并设置标签： static const char *my_tag = “my tag”; wl_proxy_set_tag((struct wl_proxy *) surface, &my_tag); 然后，在以 wl_surface 为参数的回调中，检查是否是同一子系统管理的曲面。const char * const *tag; tag = wl_proxy_get_tag((struct wl_proxy *) surface); if (tag != &my_tag) return; ... 为调试目的，标签应适合包含在调试日志条目中，例如 const char * const *tag; tag = wl_proxy_get_tag((struct wl_proxy *) surface); printf(“Got a surface with the tag %p (%s)\n”, tag, (tag && *tag) ? *tag ： “");

Since: 1.17.90 

### wl_proxy_get_tag - 获取代理对象的标记

```c
const char *const  * wl_proxy_get_tag(struct wl_proxy *proxy)
```

Since: 1.17.90

### wl_proxy_get_class - 获取代理对象的接口名称（类）

```c
const char * wl_proxy_get_class(struct wl_proxy *proxy)
```

### wl_proxy_get_display - 获取代理对象的显示内容

```c
struct wl_display * wl_proxy_get_display(struct wl_proxy *proxy)
```

Since: 1.23

### wl_proxy_set_queue - 将代理分配到事件队列中

```c
void wl_proxy_set_queue(struct wl_proxy *proxy, struct wl_event_queue *queue)
```

将代理分配到事件队列。从现在开始，来自代理的事件将在队列中排队。如果队列为空，则显示屏的默认队列将设置为代理。

为了保证队列更改生效前已排队的所有事件都能得到妥善处理，需要在设置新事件队列后分派代理的旧事件队列。

这一点对于多线程设置尤为重要，因为在调用此函数期间，可能会有事件从不同的线程队列到代理的旧队列。

为确保新创建的代理的所有事件都在特定队列中分发，如果事件在多个线程上读取和分发，则有必要使用代理封装器。详情请参见 `wl_proxy_create_wrapper()`。

### wl_event_queue_get_name - 获取事件队列的名称

```c
const char * wl_event_queue_get_name(const struct wl_event_queue *queue)
```
返回事件队列的可读名称

如果未设置名称，则可能为空

### wl_proxy_create_wrapper - 创建一个代理包装器，使队列分配线程安全

```c
void * wl_proxy_create_wrapper(void *proxy)
```

代理封装器是 `struct wl_proxy` 实例的一种，在发送请求时可代替原始代理使用。代理封装器没有实现或派发器，对象上接收到的事件仍会在原始代理上发出。尝试设置实现或调度程序不会有任何效果，但会记录警告。

设置代理包装器的代理队列将使使用代理包装器创建的新对象使用所设置的代理队列。即使没有实现或调度程序，也可以更改代理队列。这将影响通过代理封装器发送的请求所创建的新对象的默认队列。

代理封装器只能使用 `wl_proxy_wrapper_destroy()` 进行销毁。

代理包装器必须在代理创建之前被销毁。

如果用户在多个线程上读取和派发事件，那么在向对象发送请求时，如果希望新创建的代理使用的代理队列与发送请求时使用的代理队列不同，就必须使用代理包装器，因为创建新代理然后设置队列不是线程安全的。

例如，使用自己的代理队列运行的模块如果需要进行显示往返，则必须在发送 wl_display.sync 请求之前封装 wl_display 代理对象。例如：
```c
struct wl_event_queue *queue = ...;
struct wl_display *wrapped_display;
struct wl_callback *callback;

wrapped_display = wl_proxy_create_wrapper(display);
wl_proxy_set_queue((struct wl_proxy *) wrapped_display, queue);
callback = wl_display_sync(wrapped_display);
wl_proxy_wrapper_destroy(wrapped_display);
wl_callback_add_listener(callback, ...);
```

### wl_proxy_wrapper_destroy - 销毁代理包装器

```c
void wl_proxy_wrapper_destroy(void *proxy_wrapper)
```

### WL_MARSHAL_FLAG_DESTROY - 在 marshalling 后销毁代理

## 函数

### wl_event_queue_destroy

```c
void wl_event_queue_destroy(struct wl_event_queue *queue)
```

### wl_proxy_marshal_flags

```c
struct wl_proxy * wl_proxy_marshal_flags(struct wl_proxy *proxy, uint32_t opcode, const struct wl_interface *interface, uint32_t version, uint32_t flags,...)
```

### wl_proxy_marshal_array_flags

```c
struct wl_proxy * wl_proxy_marshal_array_flags(struct wl_proxy *proxy, uint32_t opcode, const struct wl_interface *interface, uint32_t version, uint32_t flags, union wl_argument *args)
```

### wl_proxy_marshal

```c
void wl_proxy_marshal(struct wl_proxy *p, uint32_t opcode,...)
```

### wl_proxy_marshal_array

```c
void wl_proxy_marshal_array(struct wl_proxy *p, uint32_t opcode, union wl_argument *args)
```

### wl_proxy_create

```c
struct wl_proxy * wl_proxy_create(struct wl_proxy *factory, const struct wl_interface *interface)
```

### wl_proxy_create_wrapper

```c
void * wl_proxy_create_wrapper(void *proxy)
```

### wl_proxy_wrapper_destroy

```c
void wl_proxy_wrapper_destroy(void *proxy_wrapper)
```

### wl_proxy_marshal_constructor

```c
struct wl_proxy * wl_proxy_marshal_constructor(struct wl_proxy *proxy, uint32_t opcode, const struct wl_interface *interface,...)
```

### wl_proxy_marshal_constructor_versioned

```c
struct wl_proxy * wl_proxy_marshal_constructor_versioned(struct wl_proxy *proxy, uint32_t opcode, const struct wl_interface *interface, uint32_t version,...)
```

### wl_proxy_marshal_array_constructor

```c
struct wl_proxy * wl_proxy_marshal_array_constructor(struct wl_proxy *proxy, uint32_t opcode, union wl_argument *args, const struct wl_interface *interface)
```

### wl_proxy_marshal_array_constructor_versioned

```c
struct wl_proxy * wl_proxy_marshal_array_constructor_versioned(struct wl_proxy *proxy, uint32_t opcode, union wl_argument *args, const struct wl_interface *interface, uint32_t version)
```

### wl_proxy_destroy

```c
void wl_proxy_destroy(struct wl_proxy *proxy)
```

### wl_proxy_add_listener

```c
int wl_proxy_add_listener(struct wl_proxy *proxy, void(**implementation)(void), void *data)
```

### wl_proxy_get_listener

```c
const void * wl_proxy_get_listener(struct wl_proxy *proxy)
```

### wl_proxy_add_dispatcher

```c
int wl_proxy_add_dispatcher(struct wl_proxy *proxy, wl_dispatcher_func_t dispatcher_func, const void *dispatcher_data, void *data)
```

### wl_proxy_set_user_data

```c
void wl_proxy_set_user_data(struct wl_proxy *proxy, void *user_data)
```

### wl_proxy_get_user_data

```c
void * wl_proxy_get_user_data(struct wl_proxy *proxy)
```

### wl_proxy_get_version

```c
uint32_t wl_proxy_get_version(struct wl_proxy *proxy)
```

### wl_proxy_get_id

```c
uint32_t wl_proxy_get_id(struct wl_proxy *proxy)
```

### wl_proxy_set_tag

```c
void wl_proxy_set_tag(struct wl_proxy *proxy, const char *const *tag)
```

### wl_proxy_get_tag

```c
const char *const  * wl_proxy_get_tag(struct wl_proxy *proxy)
```

### wl_proxy_get_class

```c
const char * wl_proxy_get_class(struct wl_proxy *proxy)
```

### wl_proxy_get_display

```c
struct wl_display * wl_proxy_get_display(struct wl_proxy *proxy)
```

### wl_proxy_set_queue

```c
void wl_proxy_set_queue(struct wl_proxy *proxy, struct wl_event_queue *queue)
```

### wl_proxy_get_queue - Get a proxy's event queue.

```c
struct wl_event_queue * wl_proxy_get_queue(const struct wl_proxy *proxy)
```

### wl_event_queue_get_name

```c
const char * wl_event_queue_get_name(const struct wl_event_queue *queue)
```

### wl_display_connect

```c
struct wl_display * wl_display_connect(const char *name)
```

### wl_display_connect_to_fd

```c
struct wl_display * wl_display_connect_to_fd(int fd)
```

### wl_display_disconnect

```c
void wl_display_disconnect(struct wl_display *display)
```

### wl_display_get_fd

```c
int wl_display_get_fd(struct wl_display *display)
```

### wl_display_dispatch

```c
int wl_display_dispatch(struct wl_display *display)
```

### wl_display_dispatch_queue

```c
int wl_display_dispatch_queue(struct wl_display *display, struct wl_event_queue *queue)
```

### wl_display_dispatch_queue_pending

```c
int wl_display_dispatch_queue_pending(struct wl_display *display, struct wl_event_queue *queue)
```

### wl_display_dispatch_pending

```c
int wl_display_dispatch_pending(struct wl_display *display)
```

### wl_display_get_error

```c
int wl_display_get_error(struct wl_display *display)
```

### wl_display_get_protocol_error

```c
uint32_t wl_display_get_protocol_error(struct wl_display *display, const struct wl_interface **interface, uint32_t *id)
```

### wl_display_flush

```c
int wl_display_flush(struct wl_display *display)
```

### wl_display_roundtrip_queue

```c
int wl_display_roundtrip_queue(struct wl_display *display, struct wl_event_queue *queue)
```

### wl_display_roundtrip

```c
int wl_display_roundtrip(struct wl_display *display)
```

### wl_display_create_queue

```c
struct wl_event_queue * wl_display_create_queue(struct wl_display *display)
```

### wl_display_create_queue_with_name

```c
struct wl_event_queue * wl_display_create_queue_with_name(struct wl_display *display, const char *name)
```

### wl_display_prepare_read_queue

```c
int wl_display_prepare_read_queue(struct wl_display *display, struct wl_event_queue *queue)
```

### wl_display_prepare_read

```c
int wl_display_prepare_read(struct wl_display *display)
```

### wl_display_cancel_read

```c
void wl_display_cancel_read(struct wl_display *display)
```

### wl_display_read_events

```c
int wl_display_read_events(struct wl_display *display)
```

### wl_log_set_handler_client

```c
void wl_log_set_handler_client(wl_log_func_t handler)
```

### wl_display_set_max_buffer_size

```c
void wl_display_set_max_buffer_size(struct wl_display *display, size_t max_buffer_size)
```

### wl_proxy_get_queue - Get a proxy's event queue.

```c
struct wl_event_queue * wl_proxy_get_queue(const struct wl_proxy *proxy)
```

### wl_log_set_handler_client

```c
void wl_log_set_handler_client(wl_log_func_t handler)
```

### WL_EXPORT - Visibility attribute.

### WL_DEPRECATED - Deprecated attribute.

### WL_PRINTF - Printf-style argument attribute. 

### wl_container_of - 读取给定成员名称的包含结构的指针

```c
struct example_container 
{
    struct wl_listener destroy_listener;
    // other members...
};

void example_container_destroy(struct wl_listener *listener, void *data)
{
    struct example_container *ctr;

    ctr = wl_container_of(listener, ctr, destroy_listener);
    // destroy ctr...
}
```

### wl_iterator_result - 迭代器函数的返回值

### wl_fixed_t - Fixed-point number.

```c
typedef int32_t wl_fixed_t
```

### wl_dispatcher_func_t - Dispatcher function type alias.

```c
typedef int(* wl_dispatcher_func_t) (const void *user_data, void *target, uint32_t opcode, const struct wl_message *msg, union wl_argument *args))(const void *user_data, void *target, uint32_t opcode, const struct wl_message *msg, union wl_argument *args)
```

调度程序是一种在客户端代码中处理回调的函数。对于直接使用 C 库的程序来说，这是通过使用 libffi 调用函数指针来实现的。当绑定到 C 语言以外的其他语言时，调度程序提供了一种方法来抽象函数调用过程，使其对其他函数调用系统更加友好。

调度程序需要五个参数： 第一个参数是与目标对象相关的调度器特定实现。第二个参数是调用回调的对象（wl_proxy 或 wl_resource）。第三和第四个参数是与回调对应的操作码和 wl_message。最后一个参数是通过有线协议从另一个进程接收的参数数组。

### wl_log_func_t - 日志函数类型别名

```c
typedef void(* wl_log_func_t) (const char *fmt, va_list args))(const char *fmt, va_list args)
```

Wayland 协议的 C 语言实现抽象了日志记录的细节。用户可以通过 wl_log_set_handler_client 和 wl_log_set_handler_server 自定义日志记录行为，并使用符合 wl_log_func_t 类型的函数。

wl_log_func_t 必须符合 vprintf 的期望，并需要两个参数：一个要写入的字符串和一个相应的变量参数列表。虽然要写入的字符串可能包含格式说明符并使用变量参数列表中的值，但任何 wl_log_func_t 的行为都取决于实现。

注意：请不要将其与 wl_protocol_logger_func_t 混淆，后者是用于请求和事件的特定服务器端日志记录器。
