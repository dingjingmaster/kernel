# libfuse30

在 fuse3 中，`fuse_main()`函数第4个参数私有数据，与 `init` 回调函数中私有数据的区别。

这两个私有数据都可以通过`fuse_get_context()->private_data`获取，但是需要注意以下几点：
- 在 `fuse_main` 中传入的私有数据，可以在 `init` 的回调中使用 `fuse_get_context()->private_date` 获取到
- 一旦`init`回调调用结束之后，以后`fuse_get_context()->private_data`返回的将是 `init` 函数返回的指针。

> 注意：其他函数操作和 libfuse25 差别不大。
