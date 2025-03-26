# Wayland调试

在运行程序之前，将环境中的 `WAYLAND_DEBUG` 变量设为 1

启动新的gnome-shell以调试程序：
```shell
dbus-run-session -- gnome-shell --nested --wayland
```
