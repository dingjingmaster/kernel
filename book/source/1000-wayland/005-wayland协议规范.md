# Wayland协议规范

## wl_display - 核心全局对象

核心全局对象。这是一个特殊的单例对象。它用于实现 Wayland 协议的内部功能。

### 由 wl_display 提供的请求

#### wl_display::sync

```
wl_display::sync // 同步请求
```
- callback：新 wl_callback 的 id - 同步请求的回调对象

同步请求要求服务器在返回的 `wl_callback` 对象上发出 “done” 事件。由于请求是按顺序处理的，而事件也是按顺序交付的，因此这可以作为一个屏障，以确保之前的所有请求和由此产生的事件都已处理完毕。

此请求返回的对象将在回调触发后被合成器销毁，因此客户端在此之后不得再尝试使用该对象。

回调中传递的 `callback_data` 未定义，应予以忽略。

#### wl_display::get_registry

```
wl_display::get_registry // 获取全局注册表对象
```
- registry：新 wl_registry 的 id - 全局注册表对象

该请求会创建一个注册表对象，允许客户端列出并绑定合成器中可用的全局对象。
需要注意的是，响应 get_registry 请求时消耗的服务器端资源只能在客户端断开连接时释放，而不能在客户端代理被销毁时释放。因此，客户端应尽可能少地调用 get_registry，以避免浪费内存。

### 由 wl_display 提供的事件

#### wl_display::error

```
wl_display::error - fatal error event
```
- object_id：发生错误的对象id
- code：错误码
- message：错误描述字符串

错误事件在发生致命（不可恢复）错误时发出。object_id 参数是发生错误的对象，通常是对该对象请求的响应。代码可识别错误，并由对象接口定义。因此，每个接口都定义了自己的错误代码集。消息是对错误的简要描述，以方便（调试）。

#### wl_display::delete_id

```
wl_display::delete_id - acknowledge object ID deletion
```
- id：删除对象的ID

该事件由对象 ID 管理逻辑在内部使用。当客户端删除其创建的对象时，服务器将发送此事件以确认已看到删除请求。客户端收到该事件后，就会知道可以安全地重复使用该对象 ID。

### wl_display提供的枚举类型

```
wl_display::error - global error values
```

这些错误是全局性的，可以响应任何服务器请求
- invalid_object：0 找不到对象
- invalid_method：1 指定接口上不存在方法或请求
- no_memory：2 服务端内存耗尽
- implementation：3 合成器上实现错误

## wl_registry - 全局注册表对象

单例全局注册表对象。服务器有许多全局对象可供所有客户端使用。这些对象通常代表服务器中的实际对象（例如输入设备），或者是提供扩展功能的单例对象。

当客户端创建注册表对象时，注册表对象将为当前注册表中的每个全局对象发出一个全局事件。由于设备或监视器热插拔、重新配置或其他事件，全局对象会出现和消失，注册表会发送全局和 `global_remove` 事件，让客户端了解最新变化。要标记初始突发事件的结束，客户端可以在调用 `wl_display.get_registry` 之后立即使用 `wl_display.sync` 请求。

客户端可以通过使用绑定请求绑定到全局对象。这会创建一个客户端句柄，让对象向客户端发送事件，并让客户端在对象上调用请求。

### 由 wl_registry 提供的请求

#### wl_registry::bind

```
wl_registry::bind - 将对象绑定到显示屏上
```
- name：对象的唯一数字表示
- id：new_id - 对象

使用指定名称作为标识符，将客户端创建的新对象绑定到服务器。

### 由 wl_registry 提供的事件

#### wl_registry::global

```
wl_registry::global - announce global object
```
- name：uint - 全局对象的数字名称
- interface：string - 对象实现的接口
- version：uint - 接口版本

通知客户端全局对象。该事件通知客户端，具有给定名称的全局对象现在可用，并且实现了给定接口的给定版本。

#### wl_registry::global_remove

```
wl_registry::global_remove - announce removal of global object
```
- name：uint - 全局对象的数字名称

通知客户端已删除的全局对象。

该事件会通知客户端，名称所标识的全局对象已不再可用。如果客户端使用绑定请求绑定了全局对象，则现在应销毁该对象。

在客户端销毁该对象之前，该对象仍然有效，对该对象的请求也将被忽略，以避免全局对象消失和客户端向其发送请求之间的冲突。

## wl_callback - 回调对象

客户端可以处理 “done” 事件，以便在相关请求完成时获得通知。

请注意，由于 `wl_callback` 对象是由多个独立的工厂接口创建的，因此 wl_callback 接口被冻结在版本 1。

### 由 wl_callback 提供的事件

#### wl_callback::done

```
wl_callback::done - done event
```
- callback_data：uint - 回调请求的特定数据

相关请求完成后通知客户端

## wl_compositor - 合成器单例

合成器。该对象是一个单例全局对象。合成器负责将多个surface的内容合成为一个可显示的输出。

### 由 wl_compositor 提供的请求

#### wl_compositor::create_surface

```
wl_compositor::create_surface - create new surface
```
- id：新surface的 id - 新surface

要求合成器创建一个新的surface。

#### wl_compositor::create_region

```
wl_compositor::create_region - 创建新区域
```
- id：id for the new wl_region - the new region

要求合成器创建一个新区域

## wl_shm_pool - 共享内存池

wl_shm_pool 对象封装了合成器和客户端共享的一块内存。通过 wl_shm_pool 对象，客户端可以分配共享内存 wl_buffer 对象。通过同一池创建的所有对象都共享相同的底层映射内存。重复使用映射内存可避免设置/延迟开销，在交互式调整surface大小或许多小缓冲区时非常有用。

### 由 wl_shm_pool 提供的请求

#### wl_shm_pool::create_buffer

```
wl_shm_pool::create_buffer - create a buffer from the pool
```
- id：id for the new wl_buffer - buffer to create
- offset：int - buffer byte offset within the pool
- width：int - buffer width, in pixels
- height：int - buffer height, in pixels
- stride：int - number of bytes from the beginning of one row to the beginning of the next row
- format：wl_shm::format (uint) - buffer pixel format

从缓冲池中创建一个 `wl_buffer` 对象。
缓冲区以偏移字节的形式创建，并具有指定的宽度和高度。stride 参数指定了从一行开始到下一行开始的字节数。format 是缓冲区的像素格式，必须是通过 `wl_shm.format` 事件发布的格式之一。
缓冲区将保留对缓冲池的引用，因此在创建缓冲区后立即销毁缓冲池是有效的。

#### wl_shm_pool::destroy

```
wl_shm_pool::destroy - destroy the pool
```
销毁共享内存池。
当从该内存池中创建的所有缓冲区都用完时，就会释放映射内存

#### wl_shm_pool::resize

```
wl_shm_pool::resize - change the size of the pool mapping
```
- size：int - new size of the pool, in bytes

该请求将导致服务器根据创建池时传递的文件描述符重新映射池的后备内存，但使用新的大小。此请求只能用于增大池。

该请求只会更改服务器映射的字节数，而不会触及与创建时传递的文件描述符相对应的文件。客户端有责任确保文件至少与新的池大小一样大。

## wl_shm - 共享内存支持

为共享内存提供支持的单例全局对象。

客户端可以使用 `create_pool` 请求创建 `wl_shm_pool` 对象。

绑定 `wl_shm` 对象时，会发出一个或多个格式事件，通知客户端可用于缓冲区的有效像素格式。

### wl_shm提供的请求

#### wl_shm::create_pool

```
wl_shm::create_pool - create a shm pool
```
- id：id for the new wl_shm_pool - pool to create
- fd：fd - file descriptor for the pool
- size：int - pool size, in bytes

创建一个新的 `wl_shm_pool` 对象。

该池可用于创建基于共享内存的缓冲区对象。服务器将对所传递文件描述符的大小字节进行毫米映射，以用作池的后备内存。

#### wl_shm::release

```
wl_shm::release - release the shm object
```

通过该请求，客户端可以告诉服务器它将不再使用 shm 对象。

通过此接口创建的对象不受影响。

### 由wl_shm提供的事件

#### wl_shm::format

```
wl_shm::format - pixel format description
```
- format：wl_shm::format (uint) - buffer pixel format

通知客户端可用于缓冲区的有效像素格式。已知格式包括 argb8888 和 xrgb8888。

### 由wl_shm提供的枚举

#### wl_shm::error

```
wl_shm::error - wl_shm error values
```

这些错误会在响应 wl_shm 请求时出现。
- invalid_format：0 - 缓冲区格式未知
- invalid_stride：1 - 在创建池或缓冲区时，大小或跨距无效
- invalid_fd： 2 - 文件描述符映射失败

#### wl_shm::format

```
wl_shm::format - pixel formats
```

这描述了单个像素的内存布局。

所有渲染器都应支持 argb8888 和 xrgb8888，但其他任何格式都是可选的，使用中的特定渲染器可能并不支持。

除 argb8888 和 xrgb8888 外，drm 格式代码与 drm_fourcc.h 中定义的宏相匹配。格式事件将报告合成器实际支持的格式。

对于所有 wl_shm 格式，除非在其他协议扩展中指定，否则像素值将使用预乘法 alpha。
- argb8888： 0 - 32-bit ARGB format, [31:0] A:R:G:B 8:8:8:8 little endian
- xrgb8888：1 - 32-bit RGB format, [31:0] x:R:G:B 8:8:8:8 little endian
- c8： 0x20203843 - 8-bit color index format, [7:0] C
- rgb332： 0x38424752 - 8-bit RGB format, [7:0] R:G:B 3:3:2
- bgr233：0x38524742 - 8-bit BGR format, [7:0] B:G:R 2:3:3
- xrgb4444：0x32315258 - 16-bit xRGB format, [15:0] x:R:G:B 4:4:4:4 little endian
- xbgr4444：0x32314258 - 16-bit xBGR format, [15:0] x:B:G:R 4:4:4:4 little endian
- rgbx4444：0x32315852 - 16-bit RGBx format, [15:0] R:G:B:x 4:4:4:4 little endian
- bgrx4444：0x32315842 - 16-bit BGRx format, [15:0] B:G:R:x 4:4:4:4 little endian
- argb4444：0x32315241 - 16-bit ARGB format, [15:0] A:R:G:B 4:4:4:4 little endian
- abgr4444：0x32314241 - 16-bit ABGR format, [15:0] A:B:G:R 4:4:4:4 little endian
- rgba4444：0x32314152 - 16-bit RBGA format, [15:0] R:G:B:A 4:4:4:4 little endian
- bgra4444：0x32314142 - 16-bit BGRA format, [15:0] B:G:R:A 4:4:4:4 little endian
- xrgb1555：0x35315258 - 16-bit xRGB format, [15:0] x:R:G:B 1:5:5:5 little endian
- xbgr1555：0x35314258 - 16-bit xBGR 1555 format, [15:0] x:B:G:R 1:5:5:5 little endian
- rgbx5551：0x35315852 - 16-bit RGBx 5551 format, [15:0] R:G:B:x 5:5:5:1 little endian
- bgrx5551：0x35315842 - 16-bit BGRx 5551 format, [15:0] B:G:R:x 5:5:5:1 little endian
- argb1555：0x35315241 - 16-bit ARGB 1555 format, [15:0] A:R:G:B 1:5:5:5 little endian
- abgr1555：0x35314241 - 16-bit ABGR 1555 format, [15:0] A:B:G:R 1:5:5:5 little endian
- rgba5551：0x35314152 - 16-bit RGBA 5551 format, [15:0] R:G:B:A 5:5:5:1 little endian
- bgra5551：0x35314142 - 16-bit BGRA 5551 format, [15:0] B:G:R:A 5:5:5:1 little endian
- rgb565：0x36314752 - 16-bit RGB 565 format, [15:0] R:G:B 5:6:5 little endian
- bgr565：0x36314742 - 16-bit BGR 565 format, [15:0] B:G:R 5:6:5 little endian
- rgb888：0x34324752 - 24-bit RGB format, [23:0] R:G:B little endian
- bgr888：0x34324742 - 24-bit BGR format, [23:0] B:G:R little endian
- xbgr8888：0x34324258 - 32-bit xBGR format, [31:0] x:B:G:R 8:8:8:8 little endian
- rgbx8888：0x34325852 - 32-bit RGBx format, [31:0] R:G:B:x 8:8:8:8 little endian
- bgrx8888：0x34325842 - 32-bit BGRx format, [31:0] B:G:R:x 8:8:8:8 little endian
- abgr8888：0x34324241 - 32-bit ABGR format, [31:0] A:B:G:R 8:8:8:8 little endian
- rgba8888：0x34324152 - 32-bit RGBA format, [31:0] R:G:B:A 8:8:8:8 little endian
- bgra8888：0x34324142 - 32-bit BGRA format, [31:0] B:G:R:A 8:8:8:8 little endian
- xrgb2101010：0x30335258 - 32-bit xRGB format, [31:0] x:R:G:B 2:10:10:10 little endian
- xbgr2101010：0x30334258 - 32-bit xBGR format, [31:0] x:B:G:R 2:10:10:10 little endian
- rgbx1010102：0x30335852 - 32-bit RGBx format, [31:0] R:G:B:x 10:10:10:2 little endian
- bgrx1010102：0x30335842 - 32-bit BGRx format, [31:0] B:G:R:x 10:10:10:2 little endian
- argb2101010：0x30335241 - 32-bit ARGB format, [31:0] A:R:G:B 2:10:10:10 little endian
- abgr2101010：0x30334241 - 32-bit ABGR format, [31:0] A:B:G:R 2:10:10:10 little endian
- rgba1010102：0x30334152 - 32-bit RGBA format, [31:0] R:G:B:A 10:10:10:2 little endian
- bgra1010102：0x30334142 - 32-bit BGRA format, [31:0] B:G:R:A 10:10:10:2 little endian
- yuyv：0x56595559 - packed YCbCr format, [31:0] Cr0:Y1:Cb0:Y0 8:8:8:8 little endian
- yvyu：0x55595659 - packed YCbCr format, [31:0] Cb0:Y1:Cr0:Y0 8:8:8:8 little endian
- uyvy：0x59565955 - packed YCbCr format, [31:0] Y1:Cr0:Y0:Cb0 8:8:8:8 little endian
- vyuy：0x59555956 - packed YCbCr format, [31:0] Y1:Cb0:Y0:Cr0 8:8:8:8 little endian
- ayuv：0x56555941 - packed AYCbCr format, [31:0] A:Y:Cb:Cr 8:8:8:8 little endian
- nv12：0x3231564e - 2 plane YCbCr Cr:Cb format, 2x2 subsampled Cr:Cb plane
- nv21：0x3132564e - 2 plane YCbCr Cb:Cr format, 2x2 subsampled Cb:Cr plane
- nv16：0x3631564e - 2 plane YCbCr Cr:Cb format, 2x1 subsampled Cr:Cb plane
- nv61：0x3136564e - 2 plane YCbCr Cb:Cr format, 2x1 subsampled Cb:Cr plane
- yuv410：0x39565559 - 3 plane YCbCr format, 4x4 subsampled Cb (1) and Cr (2) planes
- yvu410：0x39555659 - 3 plane YCbCr format, 4x4 subsampled Cr (1) and Cb (2) planes
- yuv411：0x31315559 - 3 plane YCbCr format, 4x1 subsampled Cb (1) and Cr (2) planes
- yvu411：0x31315659 - 3 plane YCbCr format, 4x1 subsampled Cr (1) and Cb (2) planes
- yuv420：0x32315559 - 3 plane YCbCr format, 2x2 subsampled Cb (1) and Cr (2) planes
- yvu420：0x32315659 - 3 plane YCbCr format, 2x2 subsampled Cr (1) and Cb (2) planes
- yuv422：0x36315559 - 3 plane YCbCr format, 2x1 subsampled Cb (1) and Cr (2) planes
- yvu422：0x36315659 - 3 plane YCbCr format, 2x1 subsampled Cr (1) and Cb (2) planes
- yuv444：0x34325559 - 3 plane YCbCr format, non-subsampled Cb (1) and Cr (2) planes
- yvu444：0x34325659 - 3 plane YCbCr format, non-subsampled Cr (1) and Cb (2) planes
- r8：0x20203852 - [7:0] R
- r16：0x20363152 - [15:0] R little endian
- rg88：0x38384752 - [15:0] R:G 8:8 little endian
- gr88：0x38385247 - [15:0] G:R 8:8 little endian
- rg1616：0x32334752 - [31:0] R:G 16:16 little endian
- gr1616：0x32335247 - [31:0] G:R 16:16 little endian
- xrgb16161616f：0x48345258 - [63:0] x:R:G:B 16:16:16:16 little endian
- xbgr16161616f：0x48344258 - [63:0] x:B:G:R 16:16:16:16 little endian
- argb16161616f：0x48345241 - [63:0] A:R:G:B 16:16:16:16 little endian
- abgr16161616f：0x48344241 - [63:0] A:B:G:R 16:16:16:16 little endian
- xyuv8888：0x56555958 - [31:0] X:Y:Cb:Cr 8:8:8:8 little endian
- vuy888：0x34325556 - [23:0] Cr:Cb:Y 8:8:8 little endian
- vuy101010：0x30335556 - Y followed by U then V, 10:10:10. Non-linear modifier only
- y210：0x30313259 - [63:0] Cr0:0:Y1:0:Cb0:0:Y0:0 10:6:10:6:10:6:10:6 little endian per 2 Y pixels
- y212：0x32313259 - [63:0] Cr0:0:Y1:0:Cb0:0:Y0:0 12:4:12:4:12:4:12:4 little endian per 2 Y pixels
- y216：0x36313259 - [63:0] Cr0:Y1:Cb0:Y0 16:16:16:16 little endian per 2 Y pixels
- y410：0x30313459 - [31:0] A:Cr:Y:Cb 2:10:10:10 little endian
- y412：0x32313459 - [63:0] A:0:Cr:0:Y:0:Cb:0 12:4:12:4:12:4:12:4 little endian
- y416：0x36313459 - [63:0] A:Cr:Y:Cb 16:16:16:16 little endian
- xvyu2101010：0x30335658 - [31:0] X:Cr:Y:Cb 2:10:10:10 little endian
- xvyu12_16161616：0x36335658 - [63:0] X:0:Cr:0:Y:0:Cb:0 12:4:12:4:12:4:12:4 little endian
- xvyu16161616：0x38345658 - [63:0] X:Cr:Y:Cb 16:16:16:16 little endian
- y0l0：0x304c3059 - [63:0] A3:A2:Y3:0:Cr0:0:Y2:0:A1:A0:Y1:0:Cb0:0:Y0:0 1:1:8:2:8:2:8:2:1:1:8:2:8:2:8:2 little endian
- x0l0：0x304c3058 - [63:0] X3:X2:Y3:0:Cr0:0:Y2:0:X1:X0:Y1:0:Cb0:0:Y0:0 1:1:8:2:8:2:8:2:1:1:8:2:8:2:8:2 little endian
- y0l2：0x324c3059 - [63:0] A3:A2:Y3:Cr0:Y2:A1:A0:Y1:Cb0:Y0 1:1:10:10:10:1:1:10:10:10 little endian
- x0l2：0x324c3058 - [63:0] X3:X2:Y3:Cr0:Y2:X1:X0:Y1:Cb0:Y0 1:1:10:10:10:1:1:10:10:10 little endian
- yuv420_8bit：0x38305559
- yuv420_10bit：0x30315559
- xrgb8888_a8：0x38415258
- xbgr8888_a8：0x38414258
- rgbx8888_a8：0x38415852
- bgrx8888_a8：0x38415842
- rgb888_a8：0x38413852
- bgr888_a8：0x38413842
- rgb565_a8：0x38413552
- bgr565_a8：0x38413542
- nv24：0x3432564e - non-subsampled Cr:Cb plane
- nv42：0x3234564e - non-subsampled Cb:Cr plane
- p210：0x30313250 - 2x1 subsampled Cr:Cb plane, 10 bit per channel
- p010：0x30313050 - 2x2 subsampled Cr:Cb plane 10 bits per channel
- p012：0x32313050 - 2x2 subsampled Cr:Cb plane 12 bits per channel
- p016：0x36313050 - 2x2 subsampled Cr:Cb plane 16 bits per channel
- axbxgxrx106106106106：0x30314241 - [63:0] A:x:B:x:G:x:R:x 10:6:10:6:10:6:10:6 little endian
- nv15：0x3531564e - 2x2 subsampled Cr:Cb plane
- q410：0x30313451
- q401：0x31303451
- xrgb16161616：0x38345258 - [63:0] x:R:G:B 16:16:16:16 little endian
- xbgr16161616：0x38344258 - [63:0] x:B:G:R 16:16:16:16 little endian
- argb16161616：0x38345241 - [63:0] A:R:G:B 16:16:16:16 little endian
- abgr16161616：0x38344241 - [63:0] A:B:G:R 16:16:16:16 little endian
- c1：0x20203143 - [7:0] C0:C1:C2:C3:C4:C5:C6:C7 1:1:1:1:1:1:1:1 eight pixels/byte
- c2：0x20203243 - [7:0] C0:C1:C2:C3 2:2:2:2 four pixels/byte
- c4：0x20203443 - [7:0] C0:C1 4:4 two pixels/byte
- d1：0x20203144 - [7:0] D0:D1:D2:D3:D4:D5:D6:D7 1:1:1:1:1:1:1:1 eight pixels/byte
- d2：0x20203244 - [7:0] D0:D1:D2:D3 2:2:2:2 four pixels/byte
- d4：0x20203444 - [7:0] D0:D1 4:4 two pixels/byte
- d8：0x20203844 - [7:0] D
- r1： 0x20203152 - [7:0] R0:R1:R2:R3:R4:R5:R6:R7 1:1:1:1:1:1:1:1 eight pixels/byte
- r2：0x20203252 - [7:0] R0:R1:R2:R3 2:2:2:2 four pixels/byte
- r4：0x20203452 - [7:0] R0:R1 4:4 two pixels/byte
- r10： 0x20303152 - [15:0] x:R 6:10 little endian
- r12： 0x20323152 - [15:0] x:R 4:12 little endian
- avuy8888： 0x59555641 - [31:0] A:Cr:Cb:Y 8:8:8:8 little endian
- xvuy8888： 0x59555658 - [31:0] X:Cr:Cb:Y 8:8:8:8 little endian
- p030：0x30333050 - 2x2 subsampled Cr:Cb plane 10 bits per channel packed

## wl_buffer - wl_surface 的内容

缓冲区为 wl_surface 提供内容。缓冲区是通过 wl_shm、wp_linux_buffer_params（来自 linux-dmabuf 协议扩展）或类似的工厂接口创建的。缓冲区有宽度和高度，可以附加到 wl_surface 上，但客户端提供和更新内容的机制是由缓冲区工厂接口定义的。

除非另有说明，否则颜色通道被假定为电气通道而非光学通道（换句话说，使用传递函数进行编码）。如果缓冲区使用的格式有阿尔法通道，除非另有说明，否则阿尔法通道将被假定为预先乘以色彩通道值（在传递函数编码后）。

请注意，由于 wl_buffer 对象是由多个独立的工厂接口创建的，因此 wl_buffer 接口被冻结在版本 1。

### 由 wl_buffer 提供的请求

#### wl_buffer::destroy

```
wl_buffer::destroy - destroy a buffer
```

销毁缓冲区 是否需要以及如何释放后备存储空间由缓冲区工厂接口定义。

关于曲面可能产生的副作用，请参阅 wl_surface.attach。

### 由 wl_buffer 提供的事件

#### wl_buffer::release

```
wl_buffer::release - compositor releases buffer
```

当合成器不再使用此 wl_buffer 时发送。客户端现在可以自由地重新使用或销毁该缓冲区及其后备存储空间。

如果客户端在将此 wl_buffer 附加到曲面的 wl_surface.commit 中请求的帧回调之前收到释放事件，则客户端可以立即自由地重复使用此缓冲区及其后备存储空间，而不需要为下一次曲面内容更新准备第二个缓冲区。通常情况下，如果合成器维护了一份 wl_surface 内容副本（如作为 GL 纹理），就可以做到这一点。对于使用 wl_shm 客户端的 GL(ES) 合成器来说，这是一项重要的优化。

## wl_data_offer - 传输数据

wl_data_offer 表示另一个客户端（源客户端）提供传输的数据。它被复制粘贴和拖放机制所使用。要约描述了数据可转换成的不同 MIME 类型，并提供了直接从源客户端传输数据的机制

### 由 wl_data_offer 提供的请求

#### wl_data_offer::accept

```
wl_data_offer::accept - accept one of the offered mime types
```
- serial：uint - serial number of the accept request
- mime_type：string - mime type accepted by the client

表示客户端可以接受给定的 MIME 类型，或 NULL 表示不接受。

对于版本 2 或更早的对象，客户端使用此请求来反馈是否可以接收给定的 mime 类型，如果不接受则为 NULL；反馈信息并不决定拖放操作是否成功。

对于版本 3 或更新的对象，该请求将决定拖放操作的最终结果。如果最终结果是未接受任何 MIME 类型，拖放操作将被取消，相应的拖放源将收到 wl_data_source.cancelled。客户端仍可将此事件与 wl_data_source.action 结合使用，以获得反馈。

#### wl_data_offer::receive

```
wl_data_offer::receive - request that the data is transferred
```
- mime_type：string - mime type desired by receiver
- fd：fd - file descriptor for data transfer

为了传输所提供的数据，客户端会发出这一请求，并指出它希望接收的 MIME 类型。传输通过传递的文件描述符（通常使用管道系统调用创建）进行。源客户端以请求的 MIME 类型表示法写入数据，然后关闭文件描述符。

接收客户端从管道的读取端读取数据，直到 EOF，然后关闭其末端，至此传输完成。

这种请求可能会在 wl_data_device.drop 之前和之后针对不同的 MIME 类型出现多次。拖放目标客户端可能会抢先获取数据或更仔细地检查数据以确定是否接受。

#### wl_data_offer::destroy

```
wl_data_offer::destroy - destroy data offer
```

#### wl_data_offer::finish

```
wl_data_offer::finish - the offer will no longer be used
```

通知合成器拖动目标已成功完成拖放操作。

收到此请求后，合成器将在拖动源客户端上发出 wl_data_source.dnd_finished。

如果在此请求之后执行除 wl_data_offer.destroy 以外的其他请求，则属于客户端错误。如果在 wl_data_offer.accept 中设置了 NULL MIME 类型，或没有通过 wl_data_offer.action 接收到任何操作，则执行此请求也是错误的。

如果收到的 wl_data_offer.finish 请求不是拖放操作，则会引发 invalid_finish 协议错误。

#### wl_data_offer::set_actions

```
wl_data_offer::set_actions - set the available/preferred drag-and-drop actions
```
- dnd_actions：wl_data_device_manager::dnd_action (uint) - actions supported by the destination client
- preferred_action：wl_data_device_manager::dnd_action (uint) - action preferred by the destination client

设置目标端客户端支持此操作的动作。如果合成器需要更改所选操作，该请求可能会触发 wl_data_source.action 和 wl_data_offer.action 事件。

在整个拖放操作过程中，该请求可被多次调用，通常是对 wl_data_device.enter 或 wl_data_device.motion 事件的响应。

该请求决定拖放操作的最终结果。如果最终结果是不接受任何操作，拖动源将收到 wl_data_source.cancelled。

dnd_actions 参数必须只包含在 wl_data_device_manager.dnd_actions 枚举中表达的值，而 preferred_action 参数必须只包含这些值中的一个，否则将导致协议错误。

在管理 “ask ”操作时，目标拖放客户端可能会执行更多的 wl_data_offer.receive 请求，并预计在请求 wl_data_offer.finish 之前，执行最后一个 wl_data_offer.set_actions 请求，其中包含 “ask ”以外的首选操作（以及可选的 wl_data_offer.accept），以便传达用户选择的操作。如果首选操作不在 wl_data_offer.source_actions mask 中，则会出错。

如果 “询问 ”操作被取消（如用户取消），客户端应立即执行 wl_data_offer.destroy。

该请求只能在拖放提议时提出，否则将引发协议错误。

### 由 wl_data_offer 提供的事件

#### wl_data_offer::offer

```
wl_data_offer::offer - advertise offered mime type
```
- mime_type：string - offered mime type

在创建 wl_data_offer 对象后立即发送。每种提供的 MIME 类型只能有一个事件。

#### wl_data_offer::source_actions

```
wl_data_offer::source_actions - notify the source-side available actions
```
- source_actions：wl_data_device_manager::dnd_action (uint) - actions offered by the data source

该事件表示数据源提供的操作。该事件将在创建 wl_data_offer 对象后立即发送，或在数据源方通过 wl_data_source.set_actions 更改其提供的操作时发送。

#### wl_data_offer::action

```
wl_data_offer::action - notify the selected action
```
- dnd_action：wl_data_device_manager::dnd_action (uint) - action selected by the compositor

此事件表示合成器在匹配源端/目的端动作后选择的动作。此处将只提供一个动作（或无动作）。

此事件可在拖放操作过程中多次触发，以响应通过 wl_data_offer.set_actions 进行的目标操作更改。

在拖放目的地发生 wl_data_device.drop 之后，该事件将不再被触发，客户端必须遵守最后收到的操作，或在处理 “询问 ”操作时通过 wl_data_offer.set_actions 设置的最后首选操作。

合成器也可能临时更改所选动作，主要是为了响应拖放操作过程中键盘修改器的更改。

最近接收到的动作总是有效的。在接收到 wl_data_device.drop 之前，所选操作可能会发生变化（例如，由于按下键盘修改器）。在接收到 wl_data_device.drop 时，拖放目标必须遵守最后收到的操作。

wl_data_device.drop 之后仍可能发生操作更改，尤其是在 “询问 ”操作中，拖放目标可能会在之后选择另一个操作。在此阶段发生的操作更改总是客户端间协商的结果，合成器将不再能够诱导不同的操作。

在进行 “询问 ”操作时，预计拖放目标可能会根据 wl_data_offer.source_actions 选择不同的操作和/或 mime 类型，并最终由用户做出选择（例如弹出一个包含可用选项的菜单）。最后的 wl_data_offer.set_actions 和 wl_data_offer.accept 请求必须在调用 wl_data_offer.finish 之前发出。

### 由 wl_data_offer 提供的枚举

#### wl_data_offer::error

- invalid_finish：0 - finish request was called untimely
- invalid_action_mask：1 - action mask contains invalid values
- invalid_action：2 - action argument has an invalid value
- invalid_offer：3 - offer doesn't accept this request

## wl_data_source - 传输数据

wl_data_source 对象是 wl_data_offer 的源端。它由数据传输中的源客户端创建，提供了一种描述所提供数据的方法，以及一种响应数据传输请求的方法。

### 由 wl_data_source 提供的请求

#### wl_data_source::offer

```
wl_data_source::offer - add an offered mime type
```
- mime_type：string - mime type offered by the data source

此请求将一种 MIME 类型添加到向目标发布的 MIME 类型集合中。可多次调用以提供多种类型。

#### wl_data_source::destroy

```
wl_data_source::destroy - destroy the data source
```

#### wl_data_source::set_actions

```
wl_data_source::set_actions - set the available drag-and-drop actions
```
- dnd_actions：wl_data_device_manager::dnd_action (uint) - actions supported by the data source

设置源客户端支持此操作的动作。如果合成器需要更改所选操作，该请求可能会触发 wl_data_source.action 和 wl_data_offer.action 事件。

dnd_actions 参数必须只包含 wl_data_device_manager.dnd_actions 枚举中的值，否则将导致协议错误。

此请求只能发出一次，并且只能在拖放中使用的源上发出，因此必须在 wl_data_device.start_drag 之前执行。如果试图将源用于拖放以外的用途，则会导致协议错误。

### 由 wl_data_source 提供的事件

#### wl_data_source::target

```
wl_data_source::target - a target accepts an offered mime type
```
- mime_type：string - mime type accepted by the target

当目标接受指针聚焦或运动事件时发送。如果目标不接受任何提供的类型，则类型为空。
在拖放过程中用于反馈。

#### wl_data_source::send

```
wl_data_source::send - send the data
```
- mime_type：string - mime type for the data
- fd：fd - file descriptor for the data

请求从客户端获取数据。通过传递的文件描述符以指定的 mime 类型发送数据，然后关闭文件描述符。

#### wl_data_source::cancelled

```
wl_data_source::cancelled - selection was cancelled
```

该数据源不再有效。出现这种情况有几种原因：
- 数据源已被其他数据源取代
- 执行了拖放操作，但拖放目标不接受通过 wl_data_source.target 提供的任何 MIME 类型
- 已执行拖放操作，但拖放目标未选择通过 wl_data_source.action 提供的掩码中的任何操作
- 已执行拖放操作，但未在曲面上进行拖放
- 合成器取消了拖放操作（例如，合成器超时以避免拖放传输过时）。
客户端应清理并销毁该数据源。
对于版本 2 或更早的对象，wl_data_source.cancelled 只会在数据源被其他数据源替换时发出。

#### wl_data_source::dnd_drop_performed

```
wl_data_source::dnd_drop_performed - the drag-and-drop operation physically finished
```

用户执行了下拉操作。该事件并不表示接受，如果拖放目标不接受任何 MIME 类型，wl_data_source.cancelled 仍可能在之后被发出。
但是，如果合成器在此事件发生前取消了拖放操作，则可能不会收到此事件。
请注意，data_source 在未来仍有可能被使用，因此不应在此销毁。

#### wl_data_source::dnd_finished

```
wl_data_source::dnd_finished - the drag-and-drop operation concluded
```
丢弃目的地已完成与该数据源的互操作，因此客户端现在可以销毁该数据源并释放所有相关数据。

如果用于执行操作的操作是 “移动”，那么数据源现在可以删除传输的数据。

#### wl_data_source::action

```
wl_data_source::action - notify the selected action
```
- dnd_action：wl_data_device_manager::dnd_action (uint) - 合成器选择的动作

此事件表示合成器在匹配源端/目的端动作后选择的动作。此处只提供一个操作（或无操作）。

该事件可在拖放操作过程中多次发生，主要是为了响应通过 wl_data_offer.set_actions 进行的目标端更改，以及数据设备进入/离开表面时发生的更改。

只有当拖放操作以 “询问 ”操作结束时，才有可能在 wl_data_source.dnd_drop_performed 之后接收到该事件，在这种情况下，最终的 wl_data_source.action 事件将紧接着 wl_data_source.dnd_finished 发生。

编译器还可能临时更改所选的操作，这主要是为了响应拖放操作过程中键盘修改器的更改。

最近收到的操作总是有效的。所选操作可能会在协商过程中发生变化（例如，“询问 ”操作可能会变成 “移动 ”操作），因此最终操作的效果必须始终应用于 wl_data_offer.dnd_finished。

客户机可以在此时触发光标表面变化，从而反映当前的操作。

### 由 wl_data_source 提供的枚举

#### wl_data_source::error

- invalid_action_mask：0 - 动作掩码包含无效值
- invalid_source：1 - 源不接受此请求

## wl_data_device - 数据传输设备

每个seat有一个 wl_data_device，可从全局 wl_data_device_manager 单例中获取。

通过 wl_data_device，可以访问客户端之间的数据传输机制，如复制粘贴和拖放。

### 由 wl_data_device 提供的请求

#### wl_data_device::start_drag

```
wl_data_device::start_drag - start drag-and-drop operation
```
- source：wl_data_source - 最终传输的数据源
- origin：wl_surface - 开始拖动的surface
- icon：wl_surface - 拖放图标绘制surface
- serial：uint - 原点上隐式抓取的序列号

此请求要求合成器代表客户端启动拖放操作。

源参数是为最终数据传输提供数据的数据源。如果源为 NULL，则进入、离开和移动事件只会发送到启动拖放的客户端，客户端应在内部处理数据传输。如果源被销毁，拖放会话将被取消。

origin surface是拖动的起始界面，客户端必须有一个与序列匹配的活动隐式抓取。

图标surface（可以是 NULL），它提供了一个可随光标移动的图标。最初，图标表面的左上角位于光标热点处，但随后的 `wl_surface.offset` 请求可以移动相对位置。必须像往常一样使用 `wl_surface.commit` 确认附加请求。图标surface被赋予拖放图标的角色。如果图标surface已具有其他角色，则会引发协议错误。

对于具有拖放图标角色的 wl_surfaces，输入区域将被忽略。

给定的源不可用于任何进一步的 `set_selection` 或 `start_drag` 请求。试图重复使用以前使用过的源可能会发送 `used_source` 错误。

#### wl_data_device::set_selection

```
wl_data_device::set_selection - copy data to the selection
```
- source：wl_data_source - 选区的数据源
- serial：uint - 触发此请求的事件序列号

此请求要求合成器代表客户端将选区设置为源数据。

要取消设置选区，请将源设置为 NULL。

给定的源将不会在任何其他 `set_selection` 或 `start_drag` 请求中使用。如果试图重新使用以前使用过的源，可能会发送 `used_source` 错误。

#### wl_data_device::release

```
wl_data_device::release - destroy data device
```

### 由 wl_data_device 提供的事件

#### wl_data_device::data_offer

```
wl_data_device::data_offer - introduce a new wl_data_offer
```
- id：新的 wl_data_offer 的 id - 新的 data_offer 对象

data_offer 事件引入了一个新的 wl_data_offer 对象，该对象随后将用于 data_device.enter 事件（用于拖放）或 data_device.selection 事件（用于选择）。紧接着 data_device.data_offer 事件，新的 data_offer 对象将发送 data_offer.offer 事件，以描述其提供的 MIME 类型。

#### wl_data_device::enter

```
wl_data_device::enter - initiate drag-and-drop session
```
- serial：uint - serial number of the enter event
- surface：wl_surface - client surface entered
- x：fixed - surface-local x coordinate
- y：fixed - surface-local y coordinate
- id：wl_data_offer - source data_offer object

当活动的拖放指针进入客户端拥有的曲面时，将发送此事件。指针进入时的位置由 x 和 y 参数提供，以曲面本地坐标表示。

#### wl_data_device::leave

```
wl_data_device::leave - end drag-and-drop session
```

该事件在拖放指针离开表面和会话结束时发送。此时，客户端必须销毁输入时引入的 wl_data_offer。

#### wl_data_device::motion

```
wl_data_device::motion - drag-and-drop session motion
```
- time：uint - timestamp with millisecond granularity
- x：fixed - surface-local x coordinate
- y：fixed - surface-local y coordinate

当拖放指针在当前聚焦的surface内移动时，将发送此事件。指针的新位置由 x 和 y 参数提供，以曲面局部坐标表示。

#### wl_data_device::drop

```
wl_data_device::drop - end drag-and-drop session successfully
```

由于隐式抓取被移除，拖放操作结束时会发送该事件。

如果拖放操作的结果是 “复制 ”或 “移动”，目的地仍可执行 wl_data_offer.receive 请求，并通过 wl_data_offer.finish 请求结束所有传输。

如果产生的操作是 “ask”，该操作将不被视为最终操作。拖放目的地应执行最后一个 wl_data_offer.set_actions 请求或 wl_data_offer.destroy 请求，以取消操作。

#### wl_data_device::selection

```
wl_data_device::selection - advertise new selection
```
- id：wl_data_offer - selection data_offer object

发送选择事件是为了通知客户端该设备有新的 wl_data_offer 供选择。data_device.data_offer 和 data_offer.offer 事件紧接在此事件之前发出，以引入数据要约对象。选择事件会在客户端接收到键盘焦点之前立即发送给客户端，并在客户端拥有键盘焦点时设置新的选择时发送给客户端。在收到新的 data_offer 或 NULL 或客户端失去键盘焦点之前，data_offer 一直有效。在同一客户端内切换键盘焦点并不意味着会发送新的选择。客户端必须在收到此事件后销毁之前的选择`data_offer`（如果有的话）。

### 由 wl_data_device 提供的枚举

#### wl_data_device::error

- role：0 - given wl_surface has another role
- used_source：1 - source has already been used

## wl_data_device_manager - 数据传输接口

wl_data_device_manager 是一个单例全局对象，它提供了对客户端间数据传输机制（如复制粘贴和拖放）的访问。这些机制与 wl_seat 绑定，客户端可通过该接口获取与 wl_seat 相对应的 wl_data_device。
根据绑定的版本，从绑定的 wl_data_device_manager 对象创建的对象在正常运行时会有不同的要求。详见 wl_data_source.set_actions、wl_data_offer.accept 和 wl_data_offer.finish。

### 由 wl_data_device_manager 提供的请求

#### wl_data_device_manager::create_data_source

```
wl_data_device_manager::create_data_source - create a new data source
```
- id：id for the new wl_data_source - data source to create

创建一个新的数据源。

#### wl_data_device_manager::get_data_device

```
wl_data_device_manager::get_data_device - create a new data device
```
- id：id for the new wl_data_device - data device to create
- seat：wl_seat - seat associated with the data device

为指定seat创建新的data_device

### 由 wl_data_device_manager 提供的枚举

#### wl_data_device_manager::dnd_action

```
wl_data_device_manager::dnd_action - bitfield - drag and drop actions
```

 这是拖放操作中可用/首选操作的位掩码。

在合成器中，选定的操作是源端和目标端提供的操作相匹配的结果。如果不匹配，“action” 事件的动作为 “none”将同时发送到源端和目标端。所有进一步的检查实际上都是在（源操作 ∩ 目的地操作）的基础上进行的。

此外，合成器还可以根据按下的按键修饰符选择不同的动作。主要工具包中常用的一种设计（也是建议合成器采用的行为）是：如果没有按下修改器，合成器就会选择不同的动作：

- 如果没有按下修改器，将使用第一个匹配项（按位顺序）
- 如果mask中启用了 “move” 功能，按下 Shift 键会选择 “移动”。
- 如果mask中启用了 “copy” 功能，按下 Control 会选择 “复制”。

除此之外的行为将视执行情况而定。例如，合成器可以将其他修改器（如 Alt/Meta）或使用 BTN_LEFT 以外的其他按钮启动的拖动绑定到特定操作（如 “ask”）。
- none：0 - no action
- copy：1 - copy action
- move：2 - move action
- ask：4 - ask action

## wl_shell - 创建桌面风格的surface

该接口由提供桌面风格用户界面的服务器实现。

它允许客户端将 wl_shell_surface 与基本surface关联起来。

注意 此协议已被弃用，不用于生产。对于桌面风格的用户界面，请使用 xdg_shell。合成器和客户端不应实现此接口。

### 由 wl_shell 提供的请求

#### wl_shell::get_shell_surface

```
wl_shell::get_shell_surface - create a shell surface from a surface
```
- id：id for the new wl_shell_surface - shell surface to create
- surface：wl_surface - surface to be given the shell surface role

为现有surface创建shell surface。这会赋予 wl_surface shell surface的角色。如果 wl_surface 已有另一个角色，会产生协议错误。

一个指定的surface只能有一个shell surface。

### 由 wl_shell 提供的枚举

#### wl_shell::error

- role：0 - given wl_surface has another role

## wl_shell_surface - 桌面风格的元数据接口

可由 wl_surface 实现的接口，用于提供桌面风格用户界面的实现。

它提供了将表面处理为顶层、全屏或弹出窗口、移动、调整大小或最大化、关联标题和类等元数据的请求。

在服务器端，当相关的 wl_surface 被销毁时，该对象也会自动销毁。在客户端，必须在销毁 wl_surface 对象之前调用 wl_shell_surface_destroy()。

### 由 wl_shell_surface 提供的请求

#### wl_shell_surface::pong

```
wl_shell_surface::pong - respond to a ping event
```
- serial：uint - serial number of the ping event

客户端必须用 pong 请求来响应 ping 事件，否则客户端可能会被视为无响应。

#### wl_shell_surface::move

```
wl_shell_surface::move - start an interactive move
```
- seat：wl_seat - seat whose pointer is used
- serial：uint - serial number of the implicit grab on the pointer

开始指针驱动的surface移动。

此请求必须用于响应按钮按下事件。服务器可能会根据surface的状态（如全屏或最大化）忽略移动请求。

#### wl_shell_surface::resize

```
wl_shell_surface::resize - start an interactive resize
```
- seat：wl_seat - seat whose pointer is used
- serial：uint - serial number of the implicit grab on the pointer
- edges：wl_shell_surface::resize (uint) - which edge or corner is being dragged

启动指针驱动的曲面尺寸调整。

此请求必须用于响应按钮按下事件。服务器可能会根据曲面的状态（如全屏或最大化）忽略调整大小请求。

#### wl_shell_surface::set_toplevel

```
wl_shell_surface::set_toplevel - make the surface a toplevel surface
```

将surface映射为顶层surface。

顶层surface不是全屏、最大化或瞬态的。

#### wl_shell_surface::set_transient

```
wl_shell_surface::set_transient - make the surface a transient surface
```
- parent：wl_surface - parent surface
- x：int - surface-local x coordinate
- y：int - surface-local y coordinate
- flags：wl_shell_surface::transient (uint) - transient surface behavior

将surface映射到现有surface。

x 和 y 参数指定surface左上角相对于父surface左上角的位置，以surface局部坐标表示。

flags 参数控制瞬态行为的细节。

#### wl_shell_surface::set_fullscreen

```
wl_shell_surface::set_fullscreen - make the surface a fullscreen surface
```
- method：wl_shell_surface::fullscreen_method (uint) - method for resolving size conflict
- framerate：uint - framerate in mHz
- output：wl_output - output on which the surface is to be fullscreen

将surface映射为全屏surface。

如果给定了输出参数，曲面将在该输出上全屏显示。如果客户端没有指定输出，合成器将应用其策略--通常会选择曲面面积最大的输出。

客户可指定一种方法来解决输出尺寸和曲面尺寸之间的尺寸冲突，该方法通过方法参数提供。

帧速率参数仅在方法设置为 “驱动程序 ”时使用，用于指示首选帧速率。值为 0 表示客户端不关心帧频。帧频以 mHz 为单位指定，即帧频为 60000 时为 60Hz。

缩放 “或 ”驱动 "方法意味着通过直接缩放操作或改变输出模式对曲面进行缩放操作。这将覆盖任何形式的输出缩放，因此映射一个缓冲区大小与模式相等的曲面可以填充屏幕，而不受缓冲区_scale 的影响。

填充 "方法意味着我们不会缩放缓冲区，但会应用任何输出缩放。这意味着您可能会遇到一种边缘情况，即应用程序映射的缓冲区大小与输出模式相同，但缓冲区_scale 为 1（从而使曲面大于输出）。在这种情况下，允许将结果缩小以适应屏幕。

合成器必须通过配置事件回复此请求，并提供输出的尺寸，曲面将在此尺寸上全屏显示。

#### wl_shell_surface::set_popup

```
wl_shell_surface::set_popup - make the surface a popup surface
```
- seat：wl_seat - seat whose pointer is used
- serial：uint - serial number of the implicit grab on the pointer
- parent：wl_surface - parent surface
- x：int - surface-local x coordinate
- y：int - surface-local y coordinate
- flags：wl_shell_surface::transient (uint) - transient surface behavior

将surface映射为弹出式surface

弹出式surface是一种瞬态surface，带有附加的指针抓取功能。

现有的隐式抓取将更改为所有者事件模式，而弹出抓取将在隐式抓取结束后继续（即释放鼠标按钮不会导致弹出窗口被取消映射）。

弹出窗口抓取将一直持续到窗口被销毁或在其他客户端窗口中按下鼠标键为止。在任何客户端surface的点击都会被正常报告，但在其他客户端surface的点击将被丢弃并触发回调。

x 和 y 参数指定了surface左上角相对于父surface左上角的位置，以本地坐标表示。

#### wl_shell_surface::set_maximized

```
wl_shell_surface::set_maximized - make the surface a maximized surface
```
- output：wl_output - output on which the surface is to be maximized

将surface映射为最大化surface。

如果给定了输出参数，surface将在该输出上最大化。如果客户端未指定输出，合成器将应用其策略--通常会选择surface面积最大的输出。

合成器会回复一个配置事件，告知预期的新surface尺寸。该操作将在下一个连接到该surface的缓冲区上完成。

除面板等桌面元素外，最大化的surface通常会填满与之绑定的整个输出。这是最大化shell 表面与全屏shell表面的主要区别。

具体细节取决于合成器的实现。

#### wl_shell_surface::set_title

```
wl_shell_surface::set_title - set surface title
```
- title：string - surface title

设置surface的简短标题。

此字符串可用于在任务栏、窗口列表或合成器提供的其他用户界面元素中识别surface。

该字符串必须以 UTF-8 编码。

#### wl_shell_surface::set_class

```
wl_shell_surface::set_class - set surface class
```
- class_：string - surface class

设置surface的类别。

surface类标识了surface所属的一般应用程序类别。常见的做法是使用应用程序 .desktop 文件的文件名（或完整路径，如果是非标准位置）作为类。

### 由 wl_shell_surface 提供的事件

#### wl_shell_surface::ping

```
wl_shell_surface::ping - ping client
```
- serial：uint - serial number of the ping

Ping 客户端，检查它是否正在接收事件和发送请求。客户端会回复一个 pong 请求。

#### wl_shell_surface::configure

```
wl_shell_surface::configure - suggest resize
```
- edges：wl_shell_surface::resize (uint) - how the surface was resized
- width：int - new width of the surface
- height：int - new height of the surface

配置事件会要求客户端调整surface的大小。

尺寸是一个提示，如果不调整，客户端可以忽略它，或选择一个较小的尺寸（以满足长宽比或以 NxM 像素为单位调整）。

边缘参数提供了关于如何调整曲面大小的提示。客户端可以使用该信息来决定如何调整其内容以适应新的尺寸（例如，滚动区域可以调整其内容位置，使可视内容保持不动）。

除了最后收到的配置事件外，客户端可以自行取消所有配置事件。

宽度和高度参数以表面局部坐标指定窗口的大小。

#### wl_shell_surface::popup_done

```
wl_shell_surface::popup_done - popup interaction is done
```

popup_done 事件会在弹出抓取中断时发出，也就是说，当用户点击不属于拥有弹出表面的客户端的表面时。

### 由 wl_shell_surface 提供的枚举

#### wl_shell_surface::resize

```
wl_shell_surface::resize - bitfield - edge values for resizing
```
这些值用于指示在调整大小操作中拖动的是surface的哪个边缘。服务器可利用这些信息调整其行为，例如选择合适的光标图像。
- none：0 - no edge
- top：1 - top edge
- bottom：2 - bottom edge
- left：4 - left edge
- top_left：5 - top and left edges
- bottom_left：6 - bottom and left edges
- right：8 - right edge
- top_right：9 - top and right edges
- bottom_right：10 - bottom and right edges

#### wl_shell_surface::transient

```
wl_shell_surface::transient - bitfield - details of transient behaviour
```

这些标志指定了瞬态surface的预期行为细节。在 set_transient 请求中使用。
- inactive：0x1 - do not set keyboard focus

#### wl_shell_surface::fullscreen_method

```
wl_shell_surface::fullscreen_method - different method to set the surface fullscreen
```
提示合成器如何处理surface尺寸与输出尺寸之间的冲突。合成器可以忽略此参数。
- default：0 - no preference, apply default policy
- scale：1 - scale, preserve the surface's aspect ratio and center on output
- driver：2 - switch output mode to the smallest mode that can fit the surface, add black borders to compensate size mismatch
- fill：3 - no upscaling, center on output and add black borders to compensate size mismatch

## wl_surface - 屏幕surface

surface是一个矩形区域，可显示在零个或多个输出端上，显示次数由合成器决定。surface可以显示 wl_buffers、接收用户输入并定义本地坐标系。

surface的大小（以及surface上的相对位置）以surface本地坐标描述，如果使用了缓冲变换（buffer_transform）或缓冲缩放（buffer_scale），这些坐标可能与像素内容的缓冲坐标不同。

没有 “role”的surface是无用的：合成器不知道在何时何地以何种方式呈现它。role是 wl_surface 的目的。role的例子包括指针的光标（由 wl_pointer.set_cursor）、拖曳图标（wl_data_device.start_drag）、子surface（wl_subcompositor.get_subsurface）和由 shell 协议定义的窗口（如 wl_shell.get_shell_surface）。

一个surface一次只能有一个role。最初，wl_surface 没有role。一旦 wl_surface 被赋予了role，它就会在 wl_surface 对象的整个生命周期中被永久地设置。除非相关的接口规范明确禁止，否则允许再次赋予当前的role。

surface role是由其它接口（如 wl_pointer.set_cursor）的请求赋予的。该请求应明确提及该请求将一个role赋予了一个 wl_surface。通常，该请求还会创建一个新的协议对象来表示role，并为 wl_surface 添加额外的功能。当客户要销毁一个 wl_surface 时，他们必须先销毁这个role对象，然后再销毁 wl_surface，否则就会发送一个 defunct_role_object 错误。

销毁role对象并不会从 wl_surface 中删除role，但它可能会阻止 wl_surface “playing role”。例如，如果销毁了一个 wl_subsurface 对象，那么为其创建的 wl_surface 将被取消映射，并忘记其位置和 Z 轴顺序。允许再次为同一个 wl_surface 创建一个 wl_subsurface，但不允许将该 wl_surface 用作光标（光标与 sub-surface 的role不同，不允许切换role）。

### 由 wl_surface 提供的请求

#### wl_surface::destroy

```
wl_surface::destroy - delete surface
```

删除surface并使其对象 ID 失效

#### wl_surface::attach

```
wl_surface::attach - set the surface contents
```
- buffer：wl_buffer - buffer of surface contents
- x：int - surface-local x coordinate
- y：int - surface-local y coordinate

设置一个缓冲区作为此surface的内容。

surface的新大小是根据通过反缓冲区变换（inverse buffer_transform）和反缓冲区刻度（inverse buffer_scale）变换的缓冲区大小计算得出的。这意味着在提交时，所提供的缓冲区大小必须是缓冲区刻度的整数倍。如果不是，就会发送无效大小错误信息。

x 和 y 参数指定了新的待处理缓冲区左上角相对于当前缓冲区左上角的位置，以表面局部坐标表示。换句话说，x 和 y 参数与新的曲面尺寸一起定义了surface尺寸变化的方向。我们不鼓励设置 0 以外的 x 和 y 参数，而应该使用单独的 wl_surface.offset 请求来代替。

当绑定的 wl_surface 版本为 5 或更高时，传递任何非零的 x 或 y 参数都是违反协议的行为，并会引发 “invalid_offset”（无效偏移）错误。x 和 y 参数将被忽略，不会改变待处理状态。要实现等效语义，请使用 wl_surface.offset。

surface内容是双缓冲状态，请参阅 wl_surface.commit。

wl_surface.attach 将给定的 wl_buffer 指定为待处理的 wl_buffer。wl_surface.commit 会将待处理的 wl_buffer 设置为新的surface内容，surface的大小会变成根据 wl_buffer 计算出的大小，如上所述。提交后，就没有待处理的缓冲区了，直到下一次附加。

提交待处理的 wl_buffer 后，合成器就可以读取 wl_buffer 中的像素。合成器可在 wl_surface.commit 请求后的任何时间访问像素。当合成器不再访问像素时，它将发送 wl_buffer.release 事件。只有在收到 wl_buffer.release 事件后，客户端才能重新使用该 wl_buffer。如果一个 wl_buffer 已被附加，然后又被另一个附加取代，而不是被提交，那么它将不会收到释放事件，也不会被合成器使用。

如果一个待处理的 wl_buffer 已被提交到多个 wl_surface，那么 wl_buffer.release 事件的传递将变得无法定义。在这种情况下，行为良好的客户端不应依赖 wl_buffer.release 事件。或者，客户端可以从同一个后备存储中创建多个 wl_buffer 对象，或者使用 wp_linux_buffer_release。

在 wl_buffer.release 之后销毁 wl_buffer 不会更改surface内容。在 wl_buffer.release 之前销毁 wl_buffer 是允许的，只要底层缓冲存储器没有被重新使用（例如在客户端进程终止时可能会发生这种情况）。但是，如果客户端在接收到 wl_buffer.release 事件之前销毁了 wl_buffer，并更改了底层缓冲区存储空间，表面内容就会立即变得未定义。

如果 wl_surface.attach 是在 wl_buffer 为空的情况下发送的，那么接下来的 wl_surface.commit 将删除surface内容。

如果待处理的 wl_buffer 已被销毁，则不会指定结果。许多合成器会在下一次 wl_surface.commit 时移除surface内容，但这种行为并不普遍。为了最大限度地提高兼容性，客户端不应销毁待处理的缓冲区，并应确保即使在销毁缓冲区后也明确地从surface中移除内容。

#### wl_surface::damage

```
wl_surface::damage - mark part of the surface damaged
```
- x：int - surface-local x coordinate
- y：int - surface-local y coordinate
- width：int - width of damage rectangle
- height：int - height of damage rectangle

此请求用于描述待处理缓冲区与当前surface内容不同的区域，以及需要重新绘制surface的区域。合成器会忽略surface外的damage部分。

damage是双缓冲状态，请参阅 wl_surface.commit。

damage矩形是以surface局部坐标指定的，其中 x 和 y 指定damage矩形的左上角。

wl_surface.damage 会增加待处理的damage：新的待处理damage是旧的待处理damage与给定矩形的结合。

wl_surface.commit 将待定damage指定为当前damage，并清除待定damage。服务器在重绘曲面时会清除当前的damage。

注意！新客户端不应使用此请求。可以使用 wl_surface.damage_buffer（使用缓冲区坐标而非曲面坐标）来发布damage。

#### wl_surface::frame

```
wl_surface::frame - request a frame throttling hint
```
- callback：id for the new wl_callback - callback object for the frame request

通过创建帧回调，在适合开始绘制新帧时请求通知。这对于节流重绘操作和驱动动画非常有用。

当客户端在 wl_surface 上绘制动画时，它可以使用 “帧 ”请求来获取通知，以确定何时是绘制和提交下一帧动画的好时机。如果客户端在此之前提交更新，很可能会导致某些更新无法显示，而客户端也会因为过于频繁地绘制而浪费资源。

帧请求将在下一次 wl_surface.commit 时生效。除非再次请求，否则通知只会发布一帧。对于 wl_surface，通知会按照帧请求提交的顺序发布。

服务器在发送通知时，必须确保客户端不会发送过多的更新，同时也要为等待回复后再绘图的客户端提供尽可能高的更新率。服务器在发送帧回调事件后，应给予客户端一定的时间进行绘制和提交，以便让它进行下一次输出刷新。

如果surface以任何方式不可见，例如surface不在屏幕上或完全被其他不透明surface遮挡，服务器就应避免发出帧回调信号。

此请求返回的对象将在回调触发后被合成器销毁，因此客户端在此之后不得再尝试使用该对象。

回调中传递的 callback_data 是当前时间（以毫秒为单位），基数未定义。

#### wl_surface::set_opaque_region

```
wl_surface::set_opaque_region - set opaque region
```
- region：wl_region - opaque region of the surface

此请求设置surface中包含不透明内容的区域。

不透明区域是合成器的优化提示，可让合成器优化不透明区域后面内容的重绘。设置不透明区域并不是正确行为的必要条件，但将透明内容标记为不透明会导致重绘伪影。

不透明区域以曲面局部坐标指定。

合成器会忽略不透明区域中位于曲面之外的部分。

不透明区域是双缓冲状态，请参阅 wl_surface.commit。

wl_surface.set_opaque_region 会改变待处理的不透明区域。否则，待处理区域和当前区域都不会改变。

不透明区域的初始值为空。设置待处理的不透明区域具有复制语义，并且 wl_region 对象可以立即销毁。如果 wl_region 为空，则待定不透明区域将被设置为空。

#### wl_surface::set_input_region

```
wl_surface::set_input_region - set input region
```
- region：wl_region - input region of the surface

此请求会设置可接收指针和触摸事件的surface区域。

在此区域外发生的输入事件将尝试服务器surface堆栈中的下一个surface。合成器会忽略输入区域中位于surface之外的部分。

输入区域以surface本地坐标指定。

输入区域是双缓冲状态，请参阅 wl_surface.commit。

wl_surface.set_input_region 会改变待处理的输入区域。否则，除了光标和图标表面是特殊情况，请参阅 wl_pointer.set_cursor 和 wl_data_device.start_drag，待处理区域和当前区域永远不会改变。

输入区域的初始值是无限的。这意味着整个surface都将接受输入。设置待定输入区域具有复制语义，wl_region 对象可以立即销毁。如果 wl_region 为空，则输入区域将被设置为无限大。

#### wl_surface::commit

```
wl_surface::commit - commit pending surface state
```

表面状态（输入、不透明和damage区域、附加缓冲区等）是双重缓冲的。协议请求修改的是待处理状态，而不是合成器正在使用的活动状态。

提交请求会从待处理状态原子式地创建内容更新，即使待处理状态未被触及也是如此。内容更新会被放置在一个队列中，直到变成活动状态。提交后，新的待处理状态与每个相关请求的记录一样。

应用内容更新时，wl_buffer 会在所有其他状态之前应用。这意味着除了 wl_surface.attach 本身外，双缓冲状态中的所有坐标都是相对于新附加的 wl_buffer 的。如果没有新附加的 wl_buffer，坐标则相对于上一次内容更新。

所有需要提交才能生效的请求都会影响双缓冲状态。

其他接口可能会进一步增加双缓冲表面状态。

#### wl_surface::set_buffer_transform

```
wl_surface::set_buffer_transform - sets the buffer transformation
```
- transform：wl_output::transform (int) - transform for interpreting buffer contents

此请求将设置客户端已应用于缓冲区内容的变换。变换参数的可接受值是 wl_output.transform 的值。

合成器在使用缓冲区内容时会应用此变换的逆变换。

缓冲变换是双缓冲状态，请参阅 wl_surface.commit。

新创建的surface会将其缓冲区变换设置为法线。

wl_surface.set_buffer_transform 会改变待处理的缓冲区变换，wl_surface.commit 会将待处理的缓冲区变换复制到当前的缓冲区变换。否则，待处理值和当前值都不会改变。

此请求的目的是允许客户端根据输出变换来渲染内容，从而允许合成器在显示屏旋转的情况下使用某些优化功能。使用硬件覆盖和扫描全屏表面的客户端缓冲区就是此类优化的例子。这些优化措施在很大程度上取决于合成器的实现，因此应根据具体情况考虑是否使用此请求。

请注意，如果变换值包含 90 或 270 度旋转，缓冲区的宽度将成为surface高度，缓冲区的高度将成为surface宽度。

如果 transform 不是 wl_output.transform 枚举中的值之一，则会出现 invalid_transform 协议错误。

#### wl_surface::set_buffer_scale

```
wl_surface::set_buffer_scale - sets the buffer scaling factor
```
- scale：int - scale for interpreting buffer contents

此请求会设置一个可选的缩放因子，用于调整合成器如何解释窗口所附缓冲区的内容。

缓冲区缩放是双缓冲状态，请参阅 wl_surface.commit。

新创建的surface的缓冲区缩放比例设置为 1。

wl_surface.set_buffer_scale 会改变待处理的缓冲区缩放比例，wl_surface.commit 会将待处理的缓冲区缩放比例复制到当前值。否则，待定值和当前值永远不会改变。

此请求的目的是允许客户端提供更高分辨率的缓冲区数据，以用于高分辨率输出。我们希望您选择的缓冲区比例与surface显示的输出比例相同。这意味着合成器在该输出上渲染surface时可以避免缩放。

请注意，如果缩放比例大于 1，则必须附加一个比所需surface尺寸更大（每个维度的缩放比例因子）的缓冲区。

如果缩放比例不大于 0，则会出现无效缩放比例协议（invalid_scale protocol）错误。

#### wl_surface::damage_buffer

```
wl_surface::damage_buffer - mark part of the surface damaged using buffer coordinates
```
- x：int - buffer-local x coordinate
- y：int - buffer-local y coordinate
- width：int - width of damage rectangle
- height：int - height of damage rectangle

此请求用于描述待处理缓冲区与当前曲面内容不同的区域，以及需要重新绘制surface的区域。合成器会忽略surface外的damage部分。

damage是双缓冲状态，请参阅 wl_surface.commit。

damage矩形是以缓冲坐标指定的，其中 x 和 y 指定damage矩形的左上角。

wl_surface.damage_buffer 会添加待处理的damage：新的待处理damage是旧的待处理damage与给定矩形的结合。

wl_surface.commit 将待定damage指定为当前damage，并清除待定damage。服务器在重绘surface时会清除当前的damage。

此请求与 wl_surface.damage 只有一个不同之处，即它是以缓冲区坐标而非surface本地坐标来获取damage值。虽然这通常比表面坐标更直观，但在使用 wp_viewport 或绘图库（如 EGL）不知道缓冲区缩放和缓冲区变换的情况下，这一点尤为重要。

注意：由于缓冲区变换变化和damage请求可能会在协议流中交错出现，因此在 wl_surface.commit 之前无法确定surface和缓冲区damage之间的实际映射关系。因此，希望同时考虑这两种damage的合成器必须分别累积来自这两种请求的damage，并在收到 wl_surface.commit 之后才将其中一种damage转换为另一种damage。

#### wl_surface::offset

```
wl_surface::offset - set the surface contents offset
```
- x：int - surface-local x coordinate
- y：int - surface-local y coordinate

x 和 y 参数指定了新的待处理缓冲区左上角相对于当前缓冲区左上角的位置，以surface局部坐标表示。换句话说，x 和 y 参数与新的surface尺寸一起定义了surface尺寸变化的方向。

surface位置偏移是双缓冲状态，请参阅 wl_surface.commit。

在 wl_surface 5 之前的版本中，此请求在语义上等同于并取代了 wl_surface.attach 请求中的 x 和 y 参数。详见 wl_surface.attach。

### 由 wl_surface 提供的事件

#### wl_surface::enter

```
wl_surface::enter - surface enters an output
```
- output：wl_output - output entered by the surface

当一个surface的创建、移动或大小调整导致它的某些部分位于一个输出的扫描区域内时，就会发出这个信号。

请注意，一个surface可能与零个或多个输出重叠。

#### wl_surface::leave

```
wl_surface::leave - surface leaves an output
```
- output：wl_output - output left by the surface

当surface的创建、移动或大小调整导致其不再有任何部分位于输出的扫描区域内时，就会发出该指令。

客户端不应将surface所在的输出数用于帧节流目的。即使没有发送离开事件，surface也可能被隐藏；即使没有发送进入事件，合成器也可能期待新的surface内容更新。因此应使用帧事件。

#### wl_surface::preferred_buffer_scale

```
wl_surface::preferred_buffer_scale - preferred buffer scale for the surface
```
- factor：int - preferred scaling factor

此事件表示此surface的首选缓冲比例。当合成器的偏好发生变化时，它就会被发送。

在收到此事件之前，此surface的首选缓冲区缩放比例为 1。

有意识到缩放的客户端可使用此事件来缩放其内容，并使用 wl_surface.set_buffer_scale 来指示他们已渲染的缩放比例。这样，客户端就可以提供细节更高的缓冲区。

合成器应发出一个大于 0 的缩放值。

#### wl_surface::preferred_buffer_transform

```
wl_surface::preferred_buffer_transform - preferred buffer transform for the surface
```
- transform：wl_output::transform (uint) - preferred transform

此事件表示此surface的首选缓冲区变换。当合成器的偏好发生变化时，它就会被发送。

在收到此事件之前，此surface的首选缓冲区变换是正常值。

将此变换应用于surface缓冲区内容并使用 wl_surface.set_buffer_transform（设置缓冲区变换）可能会使合成器更有效地使用surface缓冲区。

### 由 wl_surface 提供的枚举

#### wl_surface::error

```
wl_surface::error - wl_surface error values
```

这些错误会在响应 wl_surface 请求时发出

- invalid_scale：0 - buffer scale value is invalid
- invalid_transform：1 - buffer transform value is invalid
- invalid_size：2 - buffer size is invalid
- invalid_offset：3 - buffer offset is invalid
- defunct_role_object：4 - surface was destroyed before its role object

## wl_seat - 一组输入设备

seat是一组键盘、指针和触摸设备。该对象在启动时或热插拔此类设备时作为全局对象发布。seat通常有一个指针，并保持键盘焦点和指针焦点。

### 由 wl_seat 提供的请求

#### wl_seat::get_pointer

```
wl_seat::get_pointer - return pointer object
```
- id：id for the new wl_pointer - seat pointer

所提供的 ID 将被初始化为该seat的 wl_pointer 接口。

只有当seat具有指针功能或过去具有指针功能时，此请求才会生效。对从未拥有过指针功能的seat发出该请求是违反协议的。在这种情况下，将发送 missing_capability 错误。

#### wl_seat::get_keyboard

```
wl_seat::get_keyboard - return keyboard object
```
- id：id for the new wl_keyboard - seat keyboard

所提供的 ID 将被初始化为该席位的 wl_keyboard 接口。

该请求只有在座位具有键盘功能或过去具有键盘功能时才会生效。对从未有过键盘功能的席位发出此请求是违反协议的。在这种情况下，将发送 missing_capability 错误。

#### wl_seat::get_touch

```
wl_seat::get_touch - return touch object
```
- id：id for the new wl_touch - seat touch interface

所提供的 ID 将被初始化为该seat的 wl_touch 界面。

只有当seat具有触摸功能或过去具有触摸功能时，此请求才会生效。对从未拥有过触摸功能的seat发出此请求是违反协议的。在这种情况下，将发送 missing_capability 错误

#### wl_seat::release

```
wl_seat::release - release the seat object
```

通过该请求，客户端可以告诉服务器它将不再使用seat对象

### 由 wl_seat 提供的事件

#### wl_seat::capabilities

```
wl_seat::capabilities - seat capabilities changed
```
- capabilities：wl_seat::capability (uint) - capabilities of the seat

每当一个seat获得或失去指针、键盘或触摸功能时，都会发出该命令。参数是一个功能枚举，其中包含该seat所具有的全部功能。

添加指针功能后，客户端可使用 wl_seat.get_pointer 请求创建一个 wl_pointer 对象。该对象将接收指针事件，直到该功能被移除。

当指针功能被移除时，客户端应使用 wl_pointer.release 请求销毁与移除功能的座位相关联的 wl_pointer 对象。这些对象将不会再收到指针事件。

在某些合成器中，如果某个seat重新获得了指针功能，而客户机先前获得的 wl_pointer 对象版本为 4 或更低，则该对象可能会再次开始发送指针事件。版本 5 或更高的 wl_pointer 对象如果是在通知客户端已增加指针功能的最新事件之前创建的，则不得发送事件。

上述行为也适用于分别具有键盘和触摸功能的 wl_keyboard 和 wl_touch。

#### wl_seat::name

```
wl_seat::name - unique identifier for this seat
```
- name：string - seat identifier

在多seat配置中，客户端可以使用seat名称来帮助识别seat所代表的物理设备。

seat名称是一个 UTF-8 字符串，其内容没有约定俗成的定义。在所有 wl_seat 全局中，每个名称都是唯一的。只有当前的合成器实例才能保证名称的唯一性。

所有客户端都使用相同的seat名称。因此，该名称可跨进程共享，以指代特定的 wl_seat 全局。

名称事件会在绑定到seat全局后发送。每个seat对象只发送一次该事件，而且名称在 wl_seat 全局的生命周期内不会改变。

如果 wl_seat 全局被销毁并在以后重新创建，合成器可以重新使用相同的seat名称。

### 由 wl_seat提供的枚举

#### wl_seat::capability

```
wl_seat::capability - bitfield - seat capability bitmask
```

这是该seat功能的位掩码；如果某个成员被设置，则表示该seat具有该功能。
- pointer：1 - the seat has pointer devices
- keyboard：2 - the seat has one or more keyboards
- touch：4 - the seat has touch devices

#### wl_seat::error

```
wl_seat::error - wl_seat error values
```

这些错误会在响应 wl_seat 请求时出现。

- missing_capability：0 - 在没有匹配功能的座位上调用 get_pointer、get_keyboard 或 get_touch。

## wl_pointer - 指针输入设备

wl_pointer 界面代表一个或多个输入设备（如鼠标），可控制seat的指针位置和指针焦点。

wl_pointer 接口可为指针所在的surface生成动作、进入和离开事件，并为按下按钮、释放按钮和滚动生成按钮和轴事件。

### 由 wl_pointer 提供的请求

#### wl_pointer::set_cursor

```
wl_pointer::set_cursor - set the pointer surface
```
- serial：uint - serial number of the enter event
- surface：wl_surface - pointer surface
- hotspot_x：int - surface-local x coordinate
- hotspot_y：int - surface-local y coordinate

设置指针，即包含指针图像（光标）的surface。该请求赋予该surface游标的role。如果该surface已具有其他role，则会引发协议错误。

只有当该设备的指针焦点是请求客户端的一个surface或surface参数是当前指针表面时，游标才会实际改变。如果在该请求中设置了先前的surface，则该surface将被替换。如果 surface 为空，指针图像将被隐藏。

参数 hotspot_x 和 hotspot_y 定义了指针surface相对于指针位置的位置。它的左上角始终位于 (x, y) - (hotspot_x, hotspot_y)，其中 (x, y) 是指针位置的坐标，以surface局部坐标表示。

当向指针surface发出 wl_surface.offset 请求时，hotspot_x 和 hotspot_y 会根据请求中传递的 x 和 y 参数递减。偏移必须像往常一样由 wl_surface.commit 应用。

也可以通过将当前设置的指针surface与 hotspot_x 和 hotspot_y 的新值一起传递给此请求来更新热点。

对于具有光标作用的 wl_surfaces，输入区域将被忽略。当游标作用结束时，wl_surface 将被取消映射。

序列参数必须与发送到客户端的最新 wl_pointer.enter 序列号一致。否则，请求将被忽略。

#### wl_pointer::release

```
wl_pointer::release - release the pointer object
```

通过该请求，客户端可以告诉服务器它将不再使用指针对象。

该请求会销毁指针代理对象，因此客户端在使用该请求后不得调用 wl_pointer_destroy()。

### 由 wl_pointer 提供的事件

#### wl_pointer::enter

```
wl_pointer::enter - enter event
```
- serial：uint - serial number of the enter event
- surface：wl_surface - surface entered by the pointer
- surface_x：fixed - surface-local x coordinate
- surface_y：fixed - surface-local y coordinate

通知此seat的指针已聚焦到某个surface。

当seat的焦点进入某个surface时，指针图像是未定义的，客户端应通过 set_cursor 请求设置适当的指针图像来响应此事件。

#### wl_pointer::leave

```
wl_pointer::leave - leave event
```
- serial：uint - serial number of the leave event
- surface：wl_surface - surface left by the pointer

通知该seat的指针不再聚焦于某个surface。

离开通知会在新焦点的进入通知之前发送。

#### wl_pointer::motion

```
wl_pointer::motion - pointer motion event
```
- time：uint - timestamp with millisecond granularity
- surface_x：fixed - surface-local x coordinate
- surface_y：fixed - surface-local y coordinate

指针位置更改通知。参数 surface_x 和 surface_y 是相对于聚焦表面的位置。

#### wl_pointer::button

```
wl_pointer::button - pointer button event
```
- serial：uint - serial number of the button event
- time：uint - timestamp with millisecond granularity
- button：uint - button that produced the event
- state：wl_pointer::button_state (uint) - physical state of the button

鼠标键点击和释放通知。

点击的位置由最后一次移动或输入事件给出。时间参数是毫秒粒度的时间戳，基数未定义。

按钮是 Linux 内核 linux/input-event-codes.h 头文件中定义的按钮代码，例如 BTN_LEFT。

任何 16 位按钮代码值都是为内核事件代码列表的未来添加而保留的。0xFFFF 以上的所有其他按钮代码目前尚未定义，但可能会在本协议的未来版本中使用。

#### wl_pointer::axis

```
wl_pointer::axis - axis event
```
- time：uint - timestamp with millisecond granularity
- axis：wl_pointer::axis (uint) - axis type
- value：fixed - length of vector in surface-local coordinate space

滚动和其他axis通知。

对于滚动事件（垂直和水平滚动轴），值参数是沿指定axis的向量长度，其坐标空间与运动事件的坐标空间相同，表示沿指定轴的相对运动。

对于支持与axis不平行运动的设备，将发出多个axis事件。

在适用的情况下，例如对于触摸板，服务器可以选择发出滚动事件，其运动矢量等同于运动事件矢量。

如果适用，客户端可以根据滚动距离变换其内容。

#### wl_pointer::frame

```
wl_pointer::frame - end of a pointer event sequence
```

表示逻辑上属于一组事件的结束。客户端在继续处理之前，应先累积该帧内所有事件的数据。

在 wl_pointer.frame 事件之前的所有 wl_pointer 事件在逻辑上都属于同一个事件。例如，在对角滚动运动中，合成器将发送一个可选的 wl_pointer.axis_source 事件、两个 wl_pointer.axis 事件（水平和垂直），最后发送一个 wl_pointer.frame 事件。客户端可以使用这些信息来计算用于滚动的对角线向量。

当多个 wl_pointer.axis 事件发生在同一帧内时，运动矢量就是所有事件的运动总和。当 wl_pointer.axis 和 wl_pointer.axis_stop 事件出现在同一帧中时，这表示一个轴上的轴运动已经停止，但另一个轴上的轴运动仍在继续。当同一帧内发生多个 wl_pointer.axis_stop 事件时，表示这些轴在同一实例中停止。

每个逻辑事件组都会发送一个 wl_pointer.frame 事件，即使该组只包含一个 wl_pointer 事件。具体来说，客户端可能会收到以下序列：motion、frame、button、frame、axis、frame、axis_stop、frame。

wl_pointer.enter 和 wl_pointer.leave 事件是由合成器而非硬件生成的逻辑事件。这些事件也由 wl_pointer.frame 分组。当指针从一个表面移动到另一个表面时，合成器应将 wl_pointer.leave 事件归入同一个 wl_pointer.frame 中。但是，客户端不能依赖于 wl_pointer.leave 和 wl_pointer.enter 位于同一 wl_pointer.frame 中。特定于合成器的策略可能要求将 wl_pointer.leave 和 wl_pointer.enter 事件分割到多个 wl_pointer.frame 组中。

#### wl_pointer::axis_source

```
wl_pointer::axis_source - axis source event
```
- axis_source：wl_pointer::axis_source (uint) - source of the axis event

滚动axis和其他axis的源信息。

该事件不会单独发生。它会在 wl_pointer.frame 事件之前发送，并为该帧内的所有事件提供源信息。

源指定了该事件的生成方式。如果源为 wl_pointer.axis_source.finger，当用户将手指从设备上抬起时，将发送 wl_pointer.axis_stop 事件。

如果源为 wl_pointer.axis_source.wheel、wl_pointer.axis_source.wheel_tilt 或 wl_pointer.axis_source.continuous，则可能会也可能不会发送 wl_pointer.axis_stop 事件。合成器是否为这些滚动源发送 axis_stop 事件取决于特定的硬件和实施情况；客户端不得依赖于接收这些滚动源的 axis_stop 事件，并应默认将这些滚动源的滚动序列视为未终止序列。

此事件为可选事件。如果某个axis事件序列的源未知，则不会发送任何事件。每帧只允许有一个 wl_pointer.axis_source 事件。

不保证 wl_pointer.axis_discrete 和 wl_pointer.axis_source 的顺序。

#### wl_pointer::axis_stop

```
wl_pointer::axis_stop - axis stop event
```
- time：uint - timestamp with millisecond granularity
- axis：wl_pointer::axis (uint) - the axis stopped with this event

滚动axis和其他axis的停止通知。

对于某些 wl_pointer.axis_source 类型，会发送 wl_pointer.axis_stop 事件来通知客户端轴序列已终止。这样，客户端就能实现动态滚动。有关何时可能生成该事件的信息，请参阅 wl_pointer.axis_source 文档。

在此事件之后，任何具有相同 axis_source 的 wl_pointer.axis 事件都应被视为新axis运动的开始。

时间戳的解释与 wl_pointer.axis 事件中的时间戳相同。时间戳值可能与之前的 wl_pointer.axis 事件相同。

#### wl_pointer::axis_discrete

```
wl_pointer::axis_discrete - axis click event
```
- axis：wl_pointer::axis (uint) - axis type
- discrete：int - number of steps

滚动axis和其他axis的离散步长信息。

该事件将 wl_pointer.axis 事件的axis值以离散步长的形式（如鼠标滚轮点击）传送。

该事件在 wl_pointer 版本 8 中已被弃用 - 该事件不会发送给支持版本 8 或更高版本的客户端。

该事件不会单独发生，而是与 wl_pointer.axis 事件结合在一起，后者在连续刻度上表示该坐标轴的值。该协议保证，每个 axis_discrete 事件之后，在同一 wl_pointer.frame 中总会有一个具有相同axis编号的axis事件。请注意，该协议允许在 axis_discrete 及其耦合轴事件之间发生其他事件，包括其他 axis_discrete 或axis事件。每个axis类型的 wl_pointer.frame 所包含的 axis_discrete 事件不得超过一个。

此事件为可选事件；连续滚动设备（如触摸板上的双指滚动）没有离散步长，因此不会生成此事件。

离散值包含方向信息，例如，值为 -2 表示向该轴的负方向移动两步。

axis编号与相关axis事件中的axis编号相同。

wl_pointer.axis_discrete 和 wl_pointer.axis_source 的顺序没有保证。

#### wl_pointer::axis_value120

```
wl_pointer::axis_value120 - axis high-resolution scroll event
```
- axis：wl_pointer::axis (uint) - axis type
- value120：int - scroll distance as fraction of 120

离散高分辨率滚动信息。

该事件包含高分辨率滚轮滚动信息，120 的每个倍数代表一个逻辑滚动步进（一个滚轮制动）。例如，在同一硬件事件中，ax_value120 为 30 表示正方向逻辑滚动步数的四分之一，value120 为 -240 表示负方向的两个逻辑滚动步数。依赖离散滚动的客户端应在处理事件之前将值 120 累加到 120 的倍数。

值 120 不得为零。

在支持 wl_pointer 版本 8 或更高版本的客户端中，该事件将取代 wl_pointer.axis_discrete 事件。

如果 wl_pointer.axis_source 事件出现在同一 wl_pointer.frame 中，则轴源适用于该事件。

不保证 wl_pointer.axis_value120 和 wl_pointer.axis_source 的顺序。

#### wl_pointer::axis_relative_direction

```
wl_pointer::axis_relative_direction - axis relative physical direction event
```
- axis：wl_pointer::axis (uint) - axis type
- direction：wl_pointer::axis_relative_direction (uint) - physical direction relative to axis motion

引起axis运动的实体的相对方向信息。

对于 wl_pointer.axis 事件，wl_pointer.axis_relative_direction 事件指定了引起 wl_pointer.axis 事件的实体的运动方向。
例如：
- 如果用户的手指在触摸板上向下移动，并导致 wl_pointer.axis vertical_scroll down 事件，则物理方向为 “相同”
- 如果用户的手指在触摸板上向下移动，并导致 wl_pointer.axis vertical_scroll up 向上滚动事件（“自然滚动”），则物理方向为 “相反”。

客户端可以使用这一信息来调整组件的滚动运动。具体来说，与传统的滚动相比，启用自然滚动会导致内容改变方向。无论自然滚动是否激活，音量控制滑块等一些部件通常都应与物理方向相匹配。该事件可使客户端将 widget 的滚动方向与物理方向相匹配。

此事件不会单独发生，而是与表示此axis值的 wl_pointer.axis 事件结合在一起。

该协议保证，每个 axis_relative_direction 事件之后，在同一 wl_pointer.frame 中总会有一个具有相同axis数的axis事件。

请注意，该协议允许在 axis_relative_direction 及其耦合axis事件之间发生其他事件。

axis编号与相关axis事件中的axis编号相同。

不保证 wl_pointer.axis_relative_direction、wl_pointer.axis_discrete 和 wl_pointer.axis_source 的顺序。

### 由 wl_pointer 提供的枚举

#### wl_pointer::error

- role：0 - given wl_surface has another role

#### wl_pointer::button_state

```
wl_pointer::button_state - physical button state
```
描述产生按钮事件的按钮物理状态。
- released：0 - the button is not pressed
- pressed：1 - the button is pressed

#### wl_pointer::axis

```
wl_pointer::axis - axis types
```

描述滚动事件的axis类型。
- vertical_scroll：0 - vertical axis
- horizontal_scroll：1 - horizontal axis

#### wl_pointer::axis_source

```
wl_pointer::axis_source - axis source types
```

描述axis事件的源类型。这向客户端说明了axis事件的物理生成方式；客户端可据此调整用户界面。例如，来自 “手指”源的滚动事件可能是在平滑坐标空间中的动态滚动，而来自 “滚轮”源的滚动事件可能是以若干行为单位的离散步进。

连续 "axis源是在连续坐标空间中产生事件的设备，但使用的不是手指。这种axis源的一个例子是基于按钮的滚动，即在按住按钮的同时，设备的垂直运动被转换为滚动事件。

滚轮倾斜 "axis源表示实际设备是一个滚轮，但滚动事件不是由滚轮旋转而是由滚轮倾斜（通常是侧向倾斜）引起的。
- wheel：0 - a physical wheel rotation
- finger：1 - finger on a touch surface
- continuous：2 - continuous coordinate space
- wheel_tilt：3 - a physical wheel tilt

#### wl_pointer::axis_relative_direction

```
wl_pointer::axis_relative_direction - axis relative direction
```

指定引起 wl_pointer.axis 事件的物理运动的方向，相对于 wl_pointer.axis 方向。
- identical：0 - physical motion matches axis direction
- inverted：1 - physical motion is the inverse of the axis direction

## wl_keyboard - 键盘输入设备

wl_keyboard 接口表示与seat相关联的一个或多个键盘。

每个 wl_keyboard 都有以下逻辑状态：
- 活动surface（可能为空）
- 当前逻辑上按下的键
- 活动修饰符
- 活动组。

默认情况下，活动surface为空，当前逻辑按键为空，活动修饰符和活动组为 0。

### 由 wl_keyboard 提供的请求

#### wl_keyboard::release

```
wl_keyboard::release - release the keyboard object
```

### 由 wl_keyboard 提供的事件

#### wl_keyboard::keymap

```
wl_keyboard::keymap - keyboard mapping
```
- format：wl_keyboard::keymap_format (uint) - keymap format
- fd：fd - keymap file descriptor
- size：uint - keymap size, in bytes

该事件为客户端提供了一个文件描述符，可在只读模式下进行内存映射，以提供键盘映射描述。

从第 7 版开始，接收方必须用 MAP_PRIVATE 映射 fd，否则 MAP_SHARED 可能会失败。

#### wl_keyboard::enter

```
wl_keyboard::enter - enter event
```
- serial：uint - serial number of the enter event
- surface：wl_surface - surface gaining keyboard focus
- keys：array - the keys currently logically down

通知此seat的键盘焦点位于某个surface上。

合成器必须在此事件后发送 wl_keyboard.modifiers 事件。

在 wl_keyboard 逻辑状态下，此事件会将活动surface设置为表面参数，并将当前逻辑下的按键设置为按键参数中的按键。如果在此事件之前 wl_keyboard 已经有一个活动surface，则合成器不得发送此事件。

#### wl_keyboard::leave

```
wl_keyboard::leave - leave event
```
- serial： uint - serial number of the leave event
- surface： wl_surface - surface that lost keyboard focus

通知此seat的键盘焦点不再位于某个surface上。

离开通知会在新焦点的输入通知之前发送。

在 wl_keyboard 逻辑状态下，此事件会将所有值重置为默认值。如果在此事件发生前，wl_keyboard 的活动表面不等于表面参数，则合成器不得发送此事件。

#### wl_keyboard::key

```
wl_keyboard::key - key event
```
- serial：uint - serial number of the key event
- time：uint - timestamp with millisecond granularity
- key：uint - key that produced the event
- state：wl_keyboard::key_state (uint) - physical state of the key

按键被按下或释放。时间参数是毫秒粒度的时间戳，基数未定义。

按键是特定于平台的按键代码，可通过将其输入键盘映射来解释。

如果该事件导致修改器发生变化，则必须在该事件之后发送由此产生的 wl_keyboard.modifiers 事件。

在 wl_keyboard 逻辑状态下，该事件会将按键添加到当前逻辑按下的按键中（如果状态参数为按下），或将按键从当前逻辑按下的按键中移除（如果状态参数为释放）。如果在此事件发生前 wl_keyboard 没有活动表面，则合成器不得发送此事件。如果状态参数为按下（或释放），且按键在此事件发生前已处于逻辑下降状态（或未处于逻辑下降状态），则合成器不得发送此事件。

#### wl_keyboard::modifiers

```
wl_keyboard::modifiers - modifier and group state
```
- serial：uint - serial number of the modifiers event
- mods_depressed：uint - depressed modifiers
- mods_latched：uint - latched modifiers
- mods_locked：uint - locked modifiers
- group：uint - keyboard layout

通知客户端修改器和/或组的状态已发生变化，客户端应更新其本地状态。

合成器可在客户端没有键盘焦点的情况下发送此事件，例如将修改器信息与指针焦点绑定。如果带有按下修饰符的修饰符事件在发送前没有发生回车事件，客户端就会认为修饰符状态有效，直到收到下一个 wl_keyboard.modifiers 事件。为了再次重置修改器状态，合成器可以发送一个没有按下修改器的 wl_keyboard.modifiers 事件。

在 wl_keyboard 逻辑状态下，该事件会更新修改器和组。

#### wl_keyboard::repeat_info

```
wl_keyboard::repeat_info - repeat rate and delay
```
- rate：int - the rate of repeating keys in characters per second
- delay：int - delay in milliseconds since key down until repeating starts

通知客户端键盘的重复率和延迟。

此事件会在 wl_keyboard 对象创建后立即发送，并保证在任何按键事件之前被客户端接收。

速率或延迟的负值都是非法的。速率为零将禁用任何重复（无论延迟值如何）。

如有必要，该事件还可在以后以新值发送，因此客户端应在创建 wl_keyboard 之后继续监听该事件。

### 由 wl_keyboard 提供的枚举

#### wl_keyboard::keymap_format

```
wl_keyboard::keymap_format - keyboard mapping format
```
指定通过 wl_keyboard.keymap 事件提供给客户端的键盘映射格式。
- no_keymap：0 - no keymap; client must understand how to interpret the raw keycode
- xkb_v1：1 - libxkbcommon compatible, null-terminated string; to determine the xkb keycode, clients must add 8 to the key event keycode

#### wl_keyboard::key_state

```
wl_keyboard::key_state - physical key state
```

描述产生按键事件的按键物理状态。
- released：0 - key is not pressed
- pressed：1 - key is pressed

## wl_touch - 触摸屏输入设备

wl_touch 界面表示与seat相关联的触摸屏。

触摸互动可由一个或多个触点组成。对于每个接触点，都会产生一系列事件，从向下事件开始，接着是零个或多个运动事件，最后是向上事件。与同一接触点相关的事件可通过序列 ID 进行识别。

### 由 wl_touch 提供的请求

#### wl_touch::release

```
wl_touch::release - release the touch object
```

### 由 wl_touch 提供的事件

#### wl_touch::down

```
wl_touch::down - touch down event and beginning of a touch sequence
```
- serial：uint - serial number of the touch down event
- time：uint - timestamp with millisecond granularity
- surface：wl_surface - surface touched
- id：int - the unique ID of this touch point
- x：fixed - surface-local x coordinate
- y：fixed - surface-local y coordinate

surface出现了一个新的触摸点。这个触摸点被分配了一个唯一的 ID。该触摸点的未来事件将引用该 ID。在一次润饰事件后，ID 不再有效，但将来可以重复使用。

#### wl_touch::up

```
wl_touch::up - end of a touch event sequence
```
- serial：uint - serial number of the touch up event
- time：uint - timestamp with millisecond granularity
- id：int - the unique ID of this touch point

触摸点已消失。不会再为该触摸点发送任何事件，该触摸点的 ID 已被释放，可在未来的触点事件中重新使用。

#### wl_touch::motion

```
wl_touch::motion - update of touch point coordinates
```
- time：uint - timestamp with millisecond granularity
- id：int - the unique ID of this touch point
- x：fixed - surface-local x coordinate
- y：fixed - surface-local y coordinate

触点座标发生了改变

#### wl_touch::frame

```
wl_touch::frame - end of touch frame event
```

表示逻辑上属于一组事件的结束。客户机在继续操作之前，需要累积该帧内所有事件的数据。

一个 wl_touch.frame 至少会终止一个事件，除此之外，不保证帧内的事件集。客户端必须假定，帧中未更新的任何状态都与之前已知的状态相同。

#### wl_touch::cancel

```
wl_touch::cancel - touch session cancelled
```

如果合成器判定触摸流为全局手势，则会发送。该手势不会再向客户端发送其他事件。触摸取消适用于该客户端表面上当前激活的所有触摸点。客户端负责最终确定触摸点，该表面上的未来触摸点可重复使用触摸点 ID。

取消事件后不需要帧事件。

#### wl_touch::shape

```
wl_touch::shape - update shape of touch point
```
- id：int - the unique ID of this touch point
- major：fixed - length of the major axis in surface-local coordinates
- minor：fixed - length of the minor axis in surface-local coordinates

当触点改变形状时发送。

此事件不会单独发生。它会在 wl_touch.frame 事件之前发送，并为该帧中任何先前报告的或新的触摸点携带新的形状信息。

描述触摸点的其他事件（如 wl_touch.down、wl_touch.motion 或 wl_touch.orientation）可能会在同一 wl_touch.frame 内发送。客户端应将这些事件视为单个逻辑触摸点更新。wl_touch.shape 、wl_touch.orientation 和 wl_touch.motion 的顺序并无保证。wl_touch.down 事件保证发生在该触摸 ID 的第一个 wl_touch.shape 事件之前，但这两个事件可能发生在同一个 wl_touch.frame 中。

触摸点形状近似于一个通过主轴和次轴长度的椭圆。主轴长度描述的是椭圆的长直径，而次轴长度描述的是短直径。主轴和次轴是正交的，都以表面局部坐标指定。椭圆的中心始终位于 wl_touch.down 或 wl_touch.move 所报告的触摸点位置。

只有当触摸设备支持形状报告时，合成器才会发送此事件。如果客户端没有收到此事件，则必须对形状做出合理的假设。

#### wl_touch::orientation

```
wl_touch::orientation - update orientation of touch point
```
- id：int - the unique ID of this touch point
- orientation：fixed - angle between major axis and positive surface y-axis in degrees

当触摸点改变方向时发送。

该事件不会单独发生。它是在 wl_touch.frame 事件之前发送的，并带有该帧中任何先前报告的或新的触摸点的新形状信息。

描述触摸点的其他事件（如 wl_touch.down、wl_touch.motion 或 wl_touch.shape）可能会在同一 wl_touch.frame 内发送。客户端应将这些事件视为单个逻辑触摸点更新。wl_touch.shape 、wl_touch.orientation 和 wl_touch.motion 的顺序并无保证。wl_touch.down 事件保证发生在该触摸 ID 的第一个 wl_touch.orientation 事件之前，但这两个事件可能发生在同一个 wl_touch.frame 中。

方向描述了触摸点主轴与正表面 y 轴的顺时针角度，并归一化为 -180 至 +180 度范围。方向的粒度取决于触摸设备，有些设备只支持 0 至 90 度的二进制旋转值。

只有当触摸设备支持方向报告时，合成器才会发送此事件。

## wl_output - 合成输出区域

输出描述合成器几何图形的一部分。合成器在 “合成器坐标系” 中工作，输出对应于该空间中实际可见的矩形区域。这通常对应于显示合成空间部分的显示器。该对象在启动或热插拔显示器时发布为全局对象。

### 由 wl_output 提供的请求

#### wl_output::release

```
wl_output::release - release the output object
```

通过该请求，客户端可以告诉服务器它将不再使用输出对象

### 由 wl_output 提供的事件

#### wl_output::geometry
```
wl_output::geometry - properties of the output
```
- x：int - x position within the global compositor space
- y：int - y position within the global compositor space
- physical_width：int - width in millimeters of the output
- physical_height：int - height in millimeters of the output
- subpixel：wl_output::subpixel (int) - subpixel orientation of the output
- make：string - textual description of the manufacturer
- model：string - textual description of the model
- transform：wl_output::transform (int) - additional transformation applied to buffer contents during presentation

几何事件描述输出的几何属性。当绑定到输出对象时，以及任何属性发生变化时，都会发送该事件。

如果物理尺寸对该输出无效（如投影仪或虚拟输出），则可将其设置为零。

几何体事件之后会有一个完成事件（从版本 2 开始）。

客户端应使用 wl_surface.preferred_buffer_transform（首选缓冲区变换）而不是此事件公布的变换来查找曲面的首选缓冲区变换。

注意：wl_output 只公布有关输出位置和标识的部分信息。一些合成器，例如那些没有实现桌面风格输出布局的合成器或那些公开虚拟输出的合成器，可能会伪造这些信息。客户端应使用 xdg_output.logical_position，而不是使用 x 和 y。客户应使用名称和描述，而不是使用品牌和型号。

#### wl_output::mode

```
wl_output::mode - advertise available modes for the output
```
- flags：wl_output::mode (uint) - bitfield of mode flags
- width：int - width of the mode in hardware units
- height：int - height of the mode in hardware units
- refresh：int - vertical refresh rate in mHz

模式事件描述了输出的可用模式。

该事件在绑定到输出对象时发送，并且始终只有一种模式，即当前模式。如果输出改变了模式，则会针对当前模式再次发送该事件。换句话说，当前模式始终是最后接收到并设置了当前标志的模式。

非当前模式已被弃用。合成器可以决定只公布当前模式，而不再发送其他模式。客户端不应依赖非当前模式。

模式的大小以输出设备的物理硬件单位表示。这不一定与全局合成空间中的输出大小相同。例如，输出可能会按比例缩放（如 wl_output.scale 中所述）或变换（如 wl_output.transform 中所述）。希望在全局合成空间中获取输出尺寸的客户端应使用 xdg_output.logical_size（逻辑尺寸）来代替。

如果垂直刷新率对该输出（如虚拟输出）没有意义，可将其设置为零。

模式事件之后会有一个完成事件（从版本 2 开始）。

客户端不应使用刷新率来调度帧。相反，它们应使用 wl_surface.frame 事件或演示时间协议。

注意：此信息并不总是对所有输出都有意义。某些合成器（如那些公开虚拟输出的合成器）可能会伪造刷新率或尺寸。

#### wl_output::done

```
wl_output::done - sent all information about output
```

绑定到输出对象后，所有其他属性都已发送，在此之后的任何其他属性更改也都已发送，然后才会发送此事件。这样，即使输出属性的更改是通过多个事件发生的，也能被视为原子事件。

#### wl_output::scale

```
wl_output::scale - output scaling properties
```
- factor：int - scaling factor of output

该事件包含几何体事件中没有的缩放几何体信息。它可以在绑定输出对象后发送，也可以在输出比例稍后发生变化时发送。合成器将为缩放比例发送一个非零的正值。如果没有发送，客户端应假设缩放比例为 1。

比例大于 1 意味着合成器在渲染时将自动按此比例缩放曲面缓冲区。这适用于分辨率非常高的显示屏，因为在这种情况下，以原始分辨率渲染的应用程序会因为太小而无法辨认。

客户端应使用 wl_surface.preferred_buffer_scale（首选缓冲区缩放）来查找曲面的首选缓冲区缩放。

缩放事件之后会有一个完成事件。

#### wl_output::name

```
wl_output::name - name of this output
```
- name：string - output name

许多合成器会为其输出分配用户友好的名称，并向用户显示这些名称，允许用户引用输出等。客户端可能也希望知道这个名称，以便向用户提供类似的行为。

名称是一个 UTF-8 字符串，其内容没有约定俗成的定义。在所有 wl_output 全局中，每个名称都是唯一的。只有合成器实例才能保证名称的唯一性。

对于给定的 wl_output 全局，所有客户端都使用相同的输出名称。因此，该名称可在不同进程间共享，以指代特定的 wl_output 全局。

该名称不能保证在不同会话中持续存在，因此不能用于在配置文件等中可靠地识别输出。

名称示例包括 “HDMI-A-1”、“WL-1”、“X11-1 ”等。但是，不要认为名称反映了底层 DRM 连接器、X11 连接等。

名称事件在绑定输出对象后发送。每个输出对象只发送一次该事件，而且名称在 wl_output 全局的生命周期内不会改变。

如果 wl_output 全局被销毁并在以后重新创建，那么合成器可以重复使用相同的输出名称。合成器应尽可能避免重复使用相同的名称。

名称事件之后会有一个完成事件。

#### wl_output::description

```
wl_output::description - human-readable description of this output
```
- description：string - output description

许多合成器可以对其输出结果进行人类可读的描述。客户端可能也希望知道这种描述，例如用于输出选择目的。

该描述是一个 UTF-8 字符串，其内容没有约定俗成的定义。在所有 wl_output 全局中，该描述不保证是唯一的。例如 “Foocorp 11” Display “或 ”Virtual X11 output via :1"。

描述事件会在绑定输出对象后以及描述发生变化时发送。描述是可选的，可能根本不会发送。

描述事件之后会有一个完成事件。

### 由 wl_output 提供的枚举

#### wl_output::subpixel

```
wl_output::subpixel - subpixel geometry information
```

该枚举描述了输出上物理像素的布局方式。
- unknown：0 - unknown geometry
- none：1 - no geometry
- horizontal_rgb：2 - horizontal RGB
- horizontal_bgr：3 - horizontal BGR
- vertical_rgb：4 - vertical RGB
- vertical_bgr：5 - vertical BGR

#### wl_output::transform

```
wl_output::transform - transformation applied to buffer contents
```

这描述了客户端和合成器对缓冲区内容进行的变换。

翻转值对应于绕垂直轴的初始翻转，然后是旋转。

其主要目的是让客户端进行相应的渲染并告知合成器，这样对于全屏表面，合成器仍能直接从客户端表面扫描出来。

#### wl_output::mode

```
wl_output::mode - bitfield - mode information
```

这些标记描述了输出模式的属性。它们用于模式事件的标志位域。
- current：0x1 - indicates this is the current mode
- preferred：0x2 - indicates this is the preferred mode

## wl_region - 区域接口

区域对象描述一个区域。

区域对象用于描述曲面的不透明区域和输入区域。

### 由 wl_region 提供的请求

#### wl_region::destroy
```
wl_region::destroy - destroy region
```

销毁区域。这将使对象 ID 失效。

#### wl_region::add

```
wl_region::add - add rectangle to region
```
- x：int - region-local x coordinate
- y：int - region-local y coordinate
- width：int - rectangle width
- height：int - rectangle height

将指定的矩形添加到区域中

#### wl_region::subtract

```
wl_region::subtract - subtract rectangle from region
```
- x：int - region-local x coordinate
- y：int - region-local y coordinate
- width：int - rectangle width
- height：int - rectangle height

从区域中减去指定的矩形。

## wl_subcompositor - 子surface合成器

子surface合成功能的全局接口。与子surfae关联的 wl_surface 称为父surface。子surface可以任意嵌套，并形成子surface树。

子surface树中的根surface是主surface。主surface不能是子surface，因为子surface必须有一个父surface。

主surface与其子surface构成一个（复合）窗口。就窗口管理而言，这组 wl_surface 对象应被视为一个窗口，其行为也应如此。

子sufrace的目的是将窗口内的部分合成工作从客户端下放到合成器。视频播放器就是一个典型的例子，它的装饰和视频都位于不同的 wl_surface 对象中。这样，在可能的情况下，合成器就可以将 YUV 视频缓冲区的处理工作交给专用的叠加硬件。

### 由 wl_subcompositor 提供的请求

#### wl_subcompositor::destroy

```
wl_subcompositor::destroy - unbind from the subcompositor interface
```

通知服务器客户端将不再使用此协议对象。这不会影响任何其他对象，包括 wl_subsurface 对象。

#### wl_subcompositor::get_subsurface

```
wl_subcompositor::get_subsurface - give a surface the role sub-surface
```
- id：id for the new wl_subsurface - the new sub-surface object ID
- surface：wl_surface - the surface to be turned into a sub-surface
- parent：wl_surface - the parent surface

为给定的surface创建子surface接口，并将其与给定的父surface关联。这会将一个普通的 wl_surface 变成一个子surface。

将要创建的子surface必须还没有其他角色(role)，也必须还没有已存在的 wl_subsurface 对象。否则将引发 bad_surface 协议错误。

向父对象添加子surface是对父对象的双缓冲操作（参见 wl_surface.commit）。添加子surface的效果会在下一次应用父surface的状态时显现。

父surface不能是子surface的后代，父surface必须与子surface不同，否则会产生 bad_parent 协议错误。

此请求会修改 wl_surface.commit 请求在子surface上的行为，请参阅 wl_subsurface 接口的文档。

### 由 wl_subcompositor 提供的枚举

#### wl_subcompositor::error
- bad_surface：0 - the to-be sub-surface is invalid
- bad_parent：1 - the to-be sub-surface parent is invalid

## wl_subsurface - wl_surface 的子surface接口

当 wl_surface 对象成为子 surface 时候的附加接口。一个子surface有一个父surface。子surface的大小和位置不受父surface的限制。尤其是子surface不会自动剪切到父surface的区域。

当应用了非空的 wl_buffer 且父surface被映射时，子surface才会被映射。先后顺序并不重要。如果父surface被隐藏或应用了空的 wl_buffer，子surface也会被隐藏。这些规则在曲面树中递归应用。

对子surface的 wl_surface.commit 请求的行为取决于子surface的模式。可能的模式有同步（synchronized）和非同步（desynchronized），请参阅 wl_subsurface.set_sync 和 wl_subsurface.set_desync 方法。同步模式会缓存 wl_surface 状态，以便在父对象的状态被应用时应用，而非同步模式会直接应用待处理的 wl_surface 状态。子surface最初处于同步模式。

与 wl_surface 请求不同，子surface还有另一种状态，由 wl_subsurface 请求管理。这种状态包括子surface相对于父surface的位置（wl_subsurface.set_position），以及父surface和子surface的堆叠顺序（wl_subsurface.place_above 和 .place_below）。无论子surface的模式如何，当父surface的 wl_surface 状态被应用时，该状态也会被应用。作为例外，set_sync 和 set_desync 会立即生效。

可以认为主surface始终处于非同步模式，因为它没有子surface意义上的父surface。

即使子surface处于非同步模式，如果其父surface的行为与同步模式相同，那么子surface的行为也与同步模式相同。这一规则在整个surface树中递归应用。这意味着我们可以将一个子surface设置为同步模式，然后假定它的所有子surface和孙子surface也都是同步的，而无需明确设置它们。

销毁子surface会立即生效。如果需要将子surface的删除与父surface的更新同步，请先通过附加一个 NULL 的 wl_buffer 来取消子surface的映射，更新父surface，然后销毁子surface。

如果父 wl_surface 对象被销毁，子surface也会被取消映射。

子surface永远不会有任何座位的键盘焦点。

### 由 wl_subsurface 提供的请求

#### wl_subsurface::destroy

```
wl_subsurface::destroy - remove sub-surface interface
```

通过 wl_subcompositor.get_subsurface 请求变成子曲面的 wl_surface 对象中的子曲面接口会被删除。wl_surface 与父对象的关联被删除。wl_surface 会被立即解除映射。

#### wl_subsurface::set_position

```
wl_subsurface::set_position - reposition the sub-surface
```
- x：int - x coordinate in the parent surface
- y：int - y coordinate in the parent surface

这将改变子表面的位置。子表面将被移动，使其原点（左上角像素）位于父表面坐标系的 x、y 位置。坐标不限于父表面区域。允许使用负值。

当父表面的状态被应用时，预定的坐标将生效。

如果在提交父表面之前，客户端调用了多个 set_position 请求，新请求的位置总是会取代之前请求的预定位置。

初始位置为 0, 0。

#### wl_subsurface::place_above

```
wl_subsurface::place_above - restack the sub-surface
```
- sibling：wl_surface - the reference surface

这个子曲面会被从堆栈中取出，放回参照曲面的正上方，同时改变子曲面的 Z 排序。参考曲面必须是兄弟曲面或父曲面之一。使用任何其他曲面，包括该子曲面，都会导致协议错误。

Z 轴顺序是双缓冲的。请求会按顺序处理，并立即应用到待处理状态。最后的待处理状态会在下一次应用父表面状态时复制到活动状态。

一个新的子曲面最初会被添加为其同级曲面和父曲面堆栈中的最顶层。

#### wl_subsurface::place_below

```
wl_subsurface::place_below - restack the sub-surface
```
- sibling：wl_surface - the reference surface

将子曲面放置在参考曲面的正下方。请参阅 wl_subsurface.place_above。

#### wl_subsurface::set_sync

```
wl_subsurface::set_sync - set sub-surface to synchronized mode
```
将子surface的提交行为改为同步模式，也称为父级依赖模式。

在同步模式下，子surface上的 wl_surface.commit 会在缓存中累积已提交的状态，但该状态不会被应用，因此也不会改变合成器的输出。父surface的状态应用后，缓存状态会立即应用到子surface。这确保了父surface及其所有同步子surface的原子更新。应用缓存状态会使缓存失效，因此父surface的进一步提交不会（重新）应用旧状态。

有关此模式的递归效果，请参阅 wl_subsurface。

#### wl_subsurface::set_desync

```
wl_subsurface::set_desync - set sub-surface to desynchronized mode
```

将子surface的提交行为改为非同步模式，也称为独立或自由运行模式。

在非同步模式下，子surface上的 wl_surface.commit 会直接应用待处理状态，而不会像 wl_surface 般进行缓存。在父surface上调用 wl_surface.commit 不会影响子surface的 wl_surface 状态。这种模式允许子曲面自行更新。

如果在非同步模式下调用 wl_surface.commit 时存在缓存状态，则待更新状态会被添加到缓存状态中，并作为一个整体应用。这将使缓存失效。

注意：即使子surface被设置为去同步，父子surface也可以覆盖它，使其表现为同步。详情请参阅 wl_subsurface。

如果一个子surface的父surface的行为是去同步的，那么在 set_desync 时会应用缓存状态。

### 由 wl_subsurface 提供的枚举

#### wl_subsurface::error

- bad_surface：0 - wl_surface is not a sibling or the parent
