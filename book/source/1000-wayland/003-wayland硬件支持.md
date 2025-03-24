# Wayland硬件支持

通常，硬件支持包括modesetting/display和EGL/GLES2。除此之外，Wayland 还需要一种在进程间有效共享缓冲区的方法。这涉及Wayland客户端和服务器端。

在客户端，我们定义了一个 Wayland EGL 平台。在 EGL 模型中，它包括本地类型（EGLNativeDisplayType、EGLNativeWindowType 和 EGLNativePixmapType）以及创建这些类型的方法。换句话说，它是将 EGL 栈及其缓冲区共享机制与通用 Wayland API 绑定在一起的胶水代码。EGL 堆栈有望提供 Wayland EGL 平台的实现。完整的 API 位于 `wayland-egl.h` 头文件中。mesa EGL 栈的开源实现在 `platform_wayland.c` 中。

EGL 协议栈将定义一个特定于供应商的协议扩展，让客户端 EGL 协议栈与合成器交流缓冲区细节，以共享缓冲区。`wayland-egl.h` API 的目的是将其抽象化，让客户端为 Wayland 表面创建 EGLSurface 并开始渲染。开源堆栈使用 drm Wayland 扩展，让客户端发现要使用的 drm 设备并进行验证，然后与合成器共享 drm (GEM) 缓冲区。

Wayland 的服务器端是垂直领域的合成器和核心UX，通常将任务切换器、应用启动器和锁屏集成在单个应用中。服务器在modsetting应用程序接口（内核modsetting、OpenWF Display 或类似应用程序接口）之上运行，并使用 EGL/GLES2 合成器和硬件叠加（如有）混合合成最终用户界面。启用modsetting、EGL/GLES2 和overlays应该是标准硬件设置的一部分。启用 Wayland 的额外要求是 `EGL_WL_bind_wayland_display` 扩展，它允许合成器从通用的 Wayland 共享缓冲区创建 EGLImage。它类似于 `EGL_KHR_image_pixmap` 扩展，可以从 X pixmap创建 EGLImage。

该扩展有一个设置步骤，需要将 EGL 显示器绑定到 Wayland 显示器。然后，当合成器从客户端接收通用的 Wayland 缓冲区时（通常是在客户端调用 eglSwapBuffers 时），它就能将 `struct wl_buffer` 指针作为 `EGLClientBuffer` 参数传递给 `eglCreateImageKHR`，并将 `EGL_WAYLAND_BUFFER_WL` 作为目标。这将创建一个 `EGLImage`，然后合成器可将其用作纹理，或传递给模式设置代码用作叠加平面。同样，这也是由厂商特定的协议扩展实现的，服务器端将接收共享缓冲区的驱动程序特定细节，并在用户调用 `eglCreateImageKHR` 时将其转化为 EGL 图像。

