; boot.s

; 说明：汇编代码使用 MASM 风格语法
; 此时 CPU 处于实模式，寻址空间是 1MB


BOOTSEG = 0X07C0            ; BIOS 读取可启动设备第一扇区内容放到内存地址 0X7C00 处
SYSSEG  = 0X1000            ; 操作系统加载位置 0X10000
SYSLEN  = 0X11              ; 操作系统占用扇区大小(17)


entry start
start:
    jmpi    go, #BOOTSEG    ; jmpi 段间跳转指令，用于x86实模式下，假设当前 CS==00H，执行
                            ; 此指令后将跳转到段 CS=0X0C70，段也会变为 0X0C70，接下来将执行
                            ; 0X7C00:go 处指令
go:
    mov ax, cs              ; 此时 CS 已经变为 0X07C0, 执行完成后 AX 也是 0X07C0
    mov ds, ax              ; 数据段寄存器 DS 指向 0X07C0 地址处
    mov ss, ax              ; 堆栈段寄存器 SS 指向 0X07C0 地址处
    mov sp, #0x400          ; 设置栈顶位置


; 将操作系统读取到 0X1000 处
load_system:
    mov dx, #0X0000         ; 读取 0 磁头、0 磁盘
    mov cx, #0X0002         ; 读取 0 柱面、2 扇区 
    mov ax, #SYSSEG         ; 
    mov es, ax              ; 附加数据段指向 #SYSSEG 也就是 0X1000 处地址，保存数据的起始位置
    xor bx, bx              ; 将 BX 寄存器设置为0，磁盘中数据读取到 0X0000 处
    mov ax, #0X200+SYSLEN   ; 0X0200 + 0X11 = 0X0211，读磁盘 + 读17个扇区
    int 0X13                ; 0X13 中断向量指向磁盘服务程序，用于进行低级磁盘和磁盘控制器的读写操作。
                            ; int 0X13 可以用来读写磁盘、格式化磁盘、获取磁盘参数、检测磁盘状态等
                            ;   AH: 存储BIOS功能号，0X02表示读取磁盘扇区；0X03H表示写扇区
                            ;   AL: 要读取的 扇区数
                            ;   CH: 要读取的 柱面号
                            ;   CL: 要读取的 起始扇区号
                            ;   DH: 要读取的 磁头号
                            ;   DL: 要读取的 磁盘号 0X80 表示第一个硬盘
                            ;   BX: 要读取到的 缓存地址
                            ;   
                            ;   当 CF 标志位被清 0 时候，表示磁盘操作成功完成
                            ;   当 CF 标志位被置为 1 时候，表示磁盘操作失败
                            ;   
    jnc ok_load             ; 如果 CF=0，则跳转到指定地址执行，否则继续顺序执行。
                            ;  CF是进位标志位，当执行一条带进位的操作指令时候，如果产生了进位，则 CF 被设置为 1，
                            ;  否则被设置为0

die:    jmp die             ; 读磁盘失败，则停止继续执行，进入死循环。

! 
ok_load:
    cli                     ; 清除 CPU 中断允许标志，不再响应中断，直到执行 STI
    mov ax, #SYSSEG         ; 0X1000
    mov ds, ax              ; DS: 数据段寄存器，存储数据段起始地址，实际上是 0X10000，最终地址=DS:SI
    xor ax, ax
    mov es, ax              ; es = 0
    mov cx, #0x2000         ; cx = 0x2000
    sub si, si              ; 清除 SI 寄存器的值
    sub di, di              ; 清除 DI 寄存器的值

    rep 
    movw                    ; 将 0X0000 处的值 复制到 0X1000 处，需要读取的长度是 0x2000
    mov ax, #BOOTSEG        ; 0x07C0
    mov ds, ax
    lidt idt_48             ; 设置 中断描述符表
    lgdt gdt_48             ; 设置 全局描述符表

    mov ax, #0x0001
    lmsw ax                 ; 设置 CR0 控制寄存器低 16 位 
    jmpi 0,8                ; 跳转到地址 0x80 00 00 00 00 00 00 80
                            ;  实模式下从其中提取低20位地址，则结果为：0x80，CS=0X7C0
                            ;  则实际执行地址：0x7C00+0x80 = 0x7C80

gdt: .word 0,0,0,0          ; 全局描述符表
    .word 0x07FF
    .word 0x0000
    .word 0x9A00
    .word 0x00C0

    .word 0x07FF
    .word 0x0000
    .word 0x9200
    .word 0x00C0

idt_48: .word 0             ; 低 16 位
    .word 0,0               ; 高 32 位

gdt_48: .word 0x7FF         ; 低 16 位
    .word 0x7C00+gdt,0      ; 高 32 位

.org 510                    ; 511 512 两个字符处分别是 0xAA55，表示此为可引导扇区
    .word 0xAA55
