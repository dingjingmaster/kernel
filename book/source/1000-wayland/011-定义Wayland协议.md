# 定义Wayland协议

Wayland的XML文件通常用于定义Wayland协议，描述了客户端和服务端之间的通信接口。这些XML文件被Wayland的工具(如：wayland-scanner)解析并生成相应的c语言或其它语言代码，以便Wayland客户端和服务端使用

## XML中各元素说明

### `<protocol>`

整个协议文件的根元素，用于指定协议名称和包含所有接口定义。

### `<copyright>`

声明该协议的版权信息

### `<interface>`

定义一个具体的接口，描述客户端与服务器之间的通信

### `<request>`

在<interface>内定义客户端发送给服务器的请求

### `<event>`

在<interface>内定义服务器向客户端发送的事件

### `<arg>`

- 作用：用于描述<request>或<event>中的参数，以及可选的interface(如果参数是一个对象引用)和allow-null(是否允许空值)
- 类型(type)：int、uint、string、object等

注意：arg必须是空元素，属性部分可以添加配置

### `<enum>`

- 作用：定义一个枚举类型，位某些参数限定一组离散的取值
- 内部子元素：<entry>用于列举各个枚举项，每个<entry>通常包含name和value属性

enum 属性添加bitmask="true"，将开启bitmask类型

> 注意：bitmask与enum不同在于，flags要求可以使用多个值时候用bitmask，否则用enum

### `<description>`

- 作用：为协议、接口、请求、事件、参数提供说明性文字，帮助开发者理解用途
- 位置：可以嵌入<protocol>、<interface>、<request>、<event>等元素内。


## XML例子

```xml
<?xml version="1.0" encoding="UTF-8"?>
<protocol name="example">
	<copyright>
		Copyright (C) 2025 Example
	</copyright>

	<interface name="example_interface" version="1">
		<description summary="must have">
			This is description
		</description>
		
		<request name="do_sth">
			<description summary="asd11asd">
				aaaa description
			</description>
			<arg name="param" type="int"/>
		</request>
		
		<event name="sth_happened">
			<description summary="asd11wq"> aaaa description ... </description>
			<arg name="status" type="string"/>
		</event>

		<enum name="example_enum">
			<entry name="FOO" value="0" summary="0"/>
			<entry name="BAR" value="1" summary="1"/>
		</enum>

		<enum name="example_flags" bitmask="true">
			<entry name="FLAG_A" value="1"/>
			<entry name="FLAG_B" value="2"/>
			<entry name="FLAG_C" value="4"/>
			<entry name="FLAG_D" value="8"/>
		</enum>
	</interface>
</protocol>
```

生成客户端头文件：
```shell
wayland-scanner client-header xxx.xml wayland-demo-protocol.h
```

生成客户端源文件：
```shell
wayland-scanner private-code xxx.xml wayland-demo-protocol.c
```
