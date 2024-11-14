# Linux 0.0.0
## 编译环境

- bin86
- make

## 运行

```shell
bochs -f ./bochsrc.bxrc
```

> 可以看到虚拟机循环输出 `AAABBB` 字符串

## 分析

源码仅包含两个文件 `boot.s` 和 `head.s`

boot 运行于CPU实模式下，机器启动后，BIOS读取磁盘第一扇区内容并存放到内存 `0x7C00` 处，并从 `0x7C00` 处开始执行。

> 实模式下采用 `CS:IP` (段地址 向左偏移4位 + 偏移地址) 方式寻址指令地址。BIOS将MBR加载到 `0x0000:0x7c00` 处对应的地址是 `0x7C00`。在启动代码中为了简化地址计算，将CS段寄存器设置为 `0x07C0`，将IP设置为0，这样 CS:IP的组合地址仍然指向`0x7C00`

程序执行逻辑如下：
1. 代码段设置为`0x07C0`，IP设置为`0x0000`，这样`CS:IP`仍然指向`0x7C00`。
2. 将数据段(DS)、堆栈段(SS)设置为`0x07C0`，堆栈栈顶寄存器(SP)设置为`0x400`(补充：堆栈寻址方式 SS:SP，堆栈从高地址向低地址生长)，栈顶`0x8000`，即最大64KB。
3. 利用BIOS提供的`0x13`中断调用，将操作系统从0磁盘0柱面2扇区开始的17个扇区读取到`0x10000`处 64KB --> 72.5KB。如果读取磁盘失败则进入死循环，成功则继续执行。
4. 禁用CPU中断(调用cli，清除CPU标志寄存器的中断允许位)
5. 设置DS: 0x1000, ES: 0x0000，将 DS:SI = `0x10000` 处 0x2000(8192字节) 长度的数据复制到 0x0000。相当于把 操作系统代码 复制到 0x00000，物理地址位置，同时覆盖了原先BIOS生成的中断向量表。
6. 设置DS: 0x07C0，将数据段指向 boot 代码的起始位置
7. 加载 中断描述符 表、加载 全局描述符 表
8. 使用 `lmsw` 指令修改CR0寄存器的低4位，进入保护模式。通过设置CRO的第0位(PE位)可以将处理器从实模式切换到保护模式；同理，清除CRO第0位可以退出保护模式。
9. 开始在保护模式下执行代码(实际相当于开始执行head.s代码)。`jmpi 0,8` 中，0表示偏移地址，即跳转到段内的偏移地址0；8表示段选择子，通常选择子8指向一个代码段描述符。
10. 设置 SS 和 SP/ESP，调用 setup_idt、setup_gdt，初始化程序指针、设置中断描述符表(IDT)和全局描述符表(GDT)，初始化数据段(DS、ES、FS、GS)、初始化堆栈段
11. 设置内存模型：平坦模型
12. 初始化计时器
13. 设置两个中断描述符：定时器中断、系统调用中断
14. 初始化任务状态段、局部描述符表
15. 设置当前任务
16. 启用中断
17. 切换到用户模式并开始执行 task0

> 每次执行task0则会触发系统调用中断在屏幕上打印'A'字符, 当定时器触发中断后会切换到task1，task1通过触发系统调用中断在屏幕上打印'B'字符。


## 源码

### boot.s

```asm
; boot.s

; 说明：汇编代码使用 MASM 风格语法
; 此时 CPU 处于实模式，寻址空间是 1MB


BOOTSEG = 0X07C0            ; BIOS 读取可启动设备第一扇区内容放到内存地址 0X7C00 处
SYSSEG  = 0X1000            ; 操作系统加载位置 0X10000
SYSLEN  = 0X11              ; 操作系统占用扇区大小(17)


entry start
start:
    jmpi    go, #BOOTSEG    ; jmpi 段间跳转指令，用于x86实模式下，假设当前 CS==00H，
                            ; 执行此指令后将跳转到段 CS=0X0C70，段也会变为 0X0C70，
                            ; 接下来将执行 0X7C00:go 处指令
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
    mov es, ax              ; 附加数据段指向 #SYSSEG 也就是 0X1000 处地址，
                            ; 保存数据的起始位置
    xor bx, bx              ; 将 BX 寄存器设置为0，磁盘中数据读取到 0X0000 处
    mov ax, #0X200+SYSLEN   ; 0X0200 + 0X11 = 0X0211，读磁盘 + 读17个扇区
    int 0X13                ; 0X13 中断向量指向磁盘服务程序，用于进行低级磁盘和
                            ; 磁盘控制器的读写操作。
                            ; int 0X13 可以用来读写磁盘、格式化磁盘、获取磁盘参数、
                            ; 检测磁盘状态等
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
                            ;  CF是进位标志位，当执行一条带进位的操作指令时候，
                            ;  如果产生了进位，则 CF 被设置为 1，否则被设置为0

die:    jmp die             ; 读磁盘失败，则停止继续执行，进入死循环。

! 
ok_load:
    cli                     ; 清除 CPU 中断允许标志，不再响应中断，直到执行 STI
    mov ax, #SYSSEG         ; 0X1000
    mov ds, ax              ; DS: 数据段寄存器，存储数据段起始地址，实际上是 0X10000，
                            ; 最终地址=DS:SI
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
```

### heade.s

```asm
# 这个是 AT&T 格式的汇编

SCRN_SEL        = 0x18
TSS0_SEL        = 0x20
LDT0_SEL        = 0x28
TSS1_SEL        = 0X30
LDT1_SEL        = 0x38
.code32
.global startup_32
.text
startup_32:
    movl $0x10,%eax             #
    mov %ax,%ds
#   mov %ax,%es
    lss init_stack,%esp         # lss (Load Segment and Offset for SS)，
                                # 同时加载栈段选择子SS 和 偏移地址 ESP 或 SP，
                                # 从而设置栈段基址和栈指针地址。
                                # 用于切换新的栈段时使用，特别是多任务操作或内核代码中。
                                # lss src, reg，reg 通常是ESP(32位)或SP(16位)，
                                # 将第一个字加载到SS段寄存器(16位)、
                                #   第二个字或双子会被加载到目标寄存器reg
                                # 将 init_stack 的地址赋值给 %esp 寄存器

# setup base fields of descriptors.
    call setup_idt              # 初始化 IDT
    call setup_gdt              # 初始化 GDT
    movl $0x10,%eax             # 将0x10加载到EAX，通常0x10是一个段选择子，
                                # 用于选择内核数据段的描述符
                                #  (在GDT中定义的描述符表项)，这是一个平坦内核数据段选择子
    mov %ax,%ds                 # after changing gdt. 
    mov %ax,%es
    mov %ax,%fs
    mov %ax,%gs                 # 将这些段寄存器指向数据段选择子
    lss init_stack,%esp         # 加载 SS 和 ESP。init_stack前16位是栈段选择子，
                                # 后32位是栈指针偏移。
                                # 至此，SS和ESP都被设置为新的栈段和栈指针地址，
                                # 指向 init_stack 所定义的栈空间。

# setup up timer 8253 chip.
# 用于设置x86处理器上可编程间隔计时器(PIT)，通常是为了初始化系统定时器(系统时钟或时间中断)
    movb $0x36, %al             # 0x36 是一种控制字，用于配置 PIT 计时器的模式，具体来说：
                                # 0x36 表示将计时器设置为模式2，用于周期性中断
                                # 同时指示 PIT 计数器将以低字节和高字节的顺序读取计数值
    movl $0x43, %edx            # 将数值 0x43 加载到 EDX 中
                                # 0x43 是PIT的控制端口，
                                # 写入该端口的值可以配置PIT的计数模式、读写方式等
    outb %al, %dx               # 将 AL 中的值(0x36) 输出到 0x43 端口
    movl $11930, %eax           # 用于设置计数器的频率(10ms，即每秒中断100次)，
                                # 常用于系统时钟中断
    movl $0x40, %edx            # 0x40 是PIT计数器0端口，用于向计数器0写入初始值
    outb %al, %dx               # 将 AL 中的低8位输出到0x40端口。

    movb %ah, %al               # 将AH中的值移动到 AL，为了在下一步中输出高字节
    outb %al, %dx               # 将 AL 中高字节输出到 0x40 端口，完成计数器的设置


# setup timer & system call interrupt descriptors.
# 主要作用是设置两个中断描述符，分别用于处理定时器中断(timer_interrupt)
# 和系统调用中断(system_interrupt)。
#   - 第8个条目用于处理定时器中断，优先级为内核级
#   - 第128个条目用于处理系统调用中断，优先级为用户级
    movl $0x00080000, %eax      # 这里的 0x0008 是段选择子(通常对应于内核代码段), 
                                # 后面的 0x0000是偏移地址的高16位, 
                                # 稍后会用低16位填入偏移地址的低位部分。
    movw $timer_interrupt, %ax  # 将 timer_interrupt 地址的低16位加载到AX。
                                # 此时，EAX中的值为：高16位 0x0008(段选择子)、
                                # 低16位 timer_interrupt 的偏移地址低位
    movw $0x8E00, %dx           # 将 0x8E00 加载到DX中。
                                #  这个值指定了中断门的类型和特权级：
                                #  0x8E表示这是一个中断门，特权级别为0，已设置有效位

    movl $0x08, %ecx            # 将 0x08 加载到 ECX, 表示这是IDT中的第8个条目。
    lea idt(,%ecx,8), %esi      # %esi 使用LEA指令计算 idt[8] 的地址，并存储在 ESI 中
                                # 这个地址是第8个IDT条目的位置(定时器中断通常映射到这个条目)

    movl %eax,(%esi)            # 将 EAX 中的值(段选择子和偏移地址低16位)
                                # 写入IDT第8个条目的前4个字节
    movl %edx,4(%esi)           # 将 EDX 中的值(描述符类型和高16位偏移地址)
                                # 写入IDT第8个条目的后4个字节
                                # 至此完成了8个条目的设置，用于处理定时器中断
    movw $system_interrupt, %ax # 将 system_interrupt 的地址低16位加载到 AX 中
                                # 更新为：高16位 0x0008(段选择子)；
                                # 低16位 system_interrupt 的偏移地址低位
    movw $0xef00, %dx           # 将0xef00 加载到 DX 中。
                                # 这个值设置了系统调用中断的特权级别和类型：
                                #  0xEE表示有效的中断门，特权级别为3(用户级)，
                                # 可供用户态程序调用
    movl $0x80, %ecx            # 将 0x80 加载到 ECX,表示这是IDT中的第128个条目(0x80).
                                # 第128个中断向量通常用于系统调用
    lea idt(,%ecx,8), %esi      # %esi 计算 idt[128] 的地址，并存储在ESI中
    movl %eax,(%esi)            # 将 EAX 的值(段选择子和偏移地址低16位)写入
                                # IDT第128个条目的前4个字节
    movl %edx,4(%esi)           # 将 EDX 中的值(描述符类型和高16位偏移地址)
                                # 写入IDT第128个条目的后4个字节这样就完成了
                                # 第 128 个条目的设置，
                                # 用于处理系统调用中断

# unmask the timer interrupt.
#   movl $0x21, %edx
#   inb %dx, %al
#   andb $0xfe, %al
#   outb %al, %dx

# Move to user mode (task 0)
# 这段代码的主要作用是初始化任务状态段(TSS)、局部描述符表(LDT)、设置当前任务、启用中断。
# 最后通过 iret 指令切换到用户模式执行 task0
    pushfl                      # 将标志寄存器(EFLAGS)压栈
    andl $0xffffbfff, (%esp)    # 在栈顶的EFLAGS上清除第12位(NT位, Nested Task标志)，
                                # 避免任务嵌套
    popfl                       # 将栈顶的 EFLAGS 弹出到标志寄存器中，完成修改
    movl $TSS0_SEL, %eax        # 将任务状态段选择子TSS0_SEL加载到EAX
    ltr %ax                     # 加载TSS0_SEL到TR(任务寄存器)，使CPU知道当前任务的TSS。
                                # TSS包含了任务切换时候需要保存的CPU状态信息
    movl $LDT0_SEL, %eax        # 将局部描述符表(LDT)的选择子LDT0_SEL加载到EAX
    lldt %ax                    # 加载 LDT 选择子到 LDTR，让CPU使用指定的LDT, 
                                # 这个表可以包含与任务相关的段描述符

    # 设置当前任务并启用中断。
    movl $0, current            # 将当前任务设置为0，通常是一个全局变量current, 
                                # 用于记录当前运行的任务
    sti                         # 设置中断标志位 IF, 允许处理器响应外部中断。

    # 切换到用户模式并执行task0
    pushl $0x17                 # 将用户数据段选择子0x17压栈，切换到用户模式的数据段
    pushl $init_stack           # 将用户态栈地址 init_stack 压栈, 
                                # 这是切换到用户模式时候将使用的栈地址
    pushfl                      # 将当前标志寄存器(EFLAGS)压栈，
                                # 用于在用户模式下恢复标志寄存器状态
    pushl $0x0f                 # 将用户代码段选择子 0x0f 压栈，
                                # 用于切换到用户模式的代码段
    pushl $task0                # 将 task0 的地址压栈，
                                # 这是用户模式下即将执行的任务的入口地址
    iret                        # 从栈中弹出 CS:EIP、EFLAGS、SS:ESP，
                                # 切换到用户模式并开始 task0


/**
 * 初始化 GDT
 */
setup_gdt:
    lgdt lgdt_opcode
    ret

/**
 * 初始化 IDT(中断描述符表)
 */
setup_idt:
    lea ignore_int,%edx         # 将 ignore_int 标签的地址加载到EDX寄存器中，
                                # 这通常是一个默认的中断处理程序的地址，
                                # 所有中断向量会被初始化为指向该处理程序
    movl $0x00080000,%eax       # 将值 0x00080000加载到 %EAX，
                                # 通常 0x0008是段选择子、指向内核代码段。
                                # 高16位位0x0008，用于描述符中的段选择子，
                                # 低16位位0x0000，作为偏移地址的高位
    movw %dx,%ax                # 将 EDX 的低16位DX加载到EAX的低16位AX，
                                # 这样EAX中就包含了段选择子和偏移地址的组合。
                                # EAX 现在包含了段选择子 0x0008(对应内核代码段) 
                                # 和 ignore_int 的偏移地址
    movw $0x8E00,%dx            # 将 0x8E00 加载到 DX, 这是描述符中的类型和属性字段：
                                # 0x8E表示有效的中断门，特权级0
    lea idt,%edi                # 将idt的地址加载到 EDI 中。EDI将作为IDT表的基址地址，
                                # 用于填充IDT表的每个条目。
    mov $256,%ecx               # 设置计数寄存器 ECX 为 256，表示IDT表包含256个条目

rp_sidt:                        # 循环 256 次，为每个IDT表项设置描述符
    movl %eax,(%edi)            # 将 EAX 中的段选择子和偏移部分(即中断处理程序地址)
                                # 写入IDT表的当前条目
    movl %edx,4(%edi)           # 将 EDX 中的类型和属性字段写入 IDT 表条目的高4字节
    addl $8,%edi                # 将 EDI 增加8，指向下一个IDT条目
    dec %ecx                    # 将 ECX 递减
    jne rp_sidt                 # ... 跳转回去、循环 256 次
    lidt lidt_opcode            # 使用 lidt 指令将 lidt_opcode 指向的 
                                # IDT 地址和大小加载到 IDTR 寄存器，
                                # 激活新的 IDT 表。
    ret

# -----------------------------------
write_char:
    push %gs
    pushl %ebx
#   pushl %eax
    mov $SCRN_SEL, %ebx
    mov %bx, %gs
    movl scr_loc, %ebx
    shl $1, %ebx
    movb %al, %gs:(%ebx)
    shr $1, %ebx
    incl %ebx
    cmpl $2000, %ebx
    jb 1f
    movl $0, %ebx
1:  movl %ebx, scr_loc      
#   popl %eax
    popl %ebx
    pop %gs
    ret

/***********************************************/
/* This is the default interrupt "handler" :-) */
.align 2
ignore_int:
    push %ds
    pushl %eax
    movl $0x10, %eax
    mov %ax, %ds
    movl $67, %eax      # print 'C'
    call write_char
    popl %eax
    pop %ds
    iret

/* Timer interrupt handler */ 
# 这段代码定义了一个中断处理程序
# 它的主要作用是响应定时器硬件的中断请求，执行任务切换逻辑，
# 切换到一个新的任务状态段(TSS), 从而实现多任务调度
.align 2                    # 对齐代码段, 以提高性能
timer_interrupt:
    push %ds                # 将数据段寄存器DS的当前值压栈, 以备稍后恢复
    pushl %eax              # 将 EAX 寄存器的当前值压栈，以备稍后恢复
    movl $0x10, %eax        # 将内核数据段选择子 0x10 加载到 EAX 中
    mov %ax, %ds            # 设置 DS 为内核数据段，以便访问内核数据
    movb $0x20, %al         # 将值 0x20 加载到 AL 中
    outb %al, $0x20         # 将 AL 中的值发送到 0x20 端口，
                            # 这是向主 PIC(可编程中断控制器) 发送“中断结束”信号，
                            # 以便允许PIC发送新的中断
    movl $1, %eax           # 将1加载到 EAX 中，用作任务ID
    cmpl %eax, current      # 将 EAX 中的值1与current的值进行比较(%eax - current)
    je 1f                   # 如果 current 等于1,则 ZF=1,则跳转到标签1, 
                            # 表示当前任务为任务1, 。
    movl %eax, current      # 当前不是任务1, 将 EAX 中的值1写入 current, 
                            # 表示切换到任务1
    ljmp $TSS1_SEL, $0      # 执行长跳转到 TSS1_SEL, 切换到任务1的TSS,
                            # 完成任务切换
    jmp 2f                  # 无条件跳转到标签2
1:  movl $0, current        # 将 0 写入 current, 表示切换到任务0
    ljmp $TSS0_SEL, $0      # 执行长跳转到 TSS0_SEL, 切换到任务 0 的TSS
2:  popl %eax               # 恢复EAX
    pop %ds                 # 恢复DS
    iret                    # 返回中断处理, 恢复 CS:EIP、EFLAGS、SS:ESP，
                            # 继续执行被中断代码

/* system call handler */
.align 2
system_interrupt:
    push %ds
    pushl %edx
    pushl %ecx
    pushl %ebx
    pushl %eax
    movl $0x10, %edx
    mov %dx, %ds
    call write_char         # 打印字符
    popl %eax
    popl %ebx
    popl %ecx
    popl %edx
    pop %ds
    iret

/*********************************************/
current:.long 0
scr_loc:.long 0

.align 2
lidt_opcode:
    .word 256*8-1           # idt contains 256 entries
    .long idt               # This will be rewrite by code. 
lgdt_opcode:
    .word (end_gdt-gdt)-1   # so does gdt 
    .long gdt               # This will be rewrite by code.

    .align 8
idt: .fill 256,8,0          # idt is uninitialized

gdt: .quad 0x0000000000000000   /* NULL descriptor */
    .quad 0x00c09a00000007ff    /* 8Mb 0x08, base = 0x00000 */
    .quad 0x00c09200000007ff    /* 8Mb 0x10 */
    .quad 0x00c0920b80000002    /* screen 0x18 - for display */

    .word 0x0068, tss0, 0xe900, 0x0 # TSS0 descr 0x20
    .word 0x0040, ldt0, 0xe200, 0x0 # LDT0 descr 0x28
    .word 0x0068, tss1, 0xe900, 0x0 # TSS1 descr 0x30
    .word 0x0040, ldt1, 0xe200, 0x0 # LDT1 descr 0x38
end_gdt:
    .fill 128,4,0
init_stack:                     # Will be used as user stack for task0.
    .long init_stack
    .word 0x10

/*************************************/
.align 8
ldt0:   .quad 0x0000000000000000
    .quad 0x00c0fa00000003ff    # 0x0f, base = 0x00000
    .quad 0x00c0f200000003ff    # 0x17

tss0:   .long 0                 /* back link */
    .long krn_stk0, 0x10        /* esp0, ss0 */
    .long 0, 0, 0, 0, 0         /* esp1, ss1, esp2, ss2, cr3 */
    .long 0, 0, 0, 0, 0         /* eip, eflags, eax, ecx, edx */
    .long 0, 0, 0, 0, 0         /* ebx esp, ebp, esi, edi */
    .long 0, 0, 0, 0, 0, 0      /* es, cs, ss, ds, fs, gs */
    .long LDT0_SEL, 0x8000000   /* ldt, trace bitmap */

    .fill 128,4,0
krn_stk0:
#       .long 0

/************************************/
.align 8
ldt1: .quad 0x0000000000000000
    .quad 0x00c0fa00000003ff    # 0x0f, base = 0x00000
    .quad 0x00c0f200000003ff    # 0x17

tss1: .long 0                   /* back link */
    .long krn_stk1, 0x10        /* esp0, ss0 */
    .long 0, 0, 0, 0, 0         /* esp1, ss1, esp2, ss2, cr3 */
    .long task1, 0x200          /* eip, eflags */
    .long 0, 0, 0, 0            /* eax, ecx, edx, ebx */
    .long usr_stk1, 0, 0, 0     /* esp, ebp, esi, edi */
    .long 0x17,0x0f,0x17,0x17,0x17,0x17 /* es, cs, ss, ds, fs, gs */
    .long LDT1_SEL, 0x8000000   /* ldt, trace bitmap */

    .fill 128,4,0
krn_stk1:

/************************************/
# 这段代码定义了一个名为 task0 的用户态任务。
# 其主要作用是设置数据段寄存器DS, 触发系统调用,然后进入一个循环，重复执行这一过程。
task0:
    movl $0x17, %eax    # 将用户数据段选择子 0x17 加载到 EAX 中。
                        # 这里的 0x17 是一个典型的用户态数据段选择子
    movw %ax, %ds       # 将 AX 的值(0x17)加载到数据段寄存器 DS,
                        # 设置数据段为用户态的数据段。
    movb $65, %al       # print 'A' 
    int $0x80           # 触发中断，CPU会跳转到系统调用处理程序，
                        # 并根据AL中值值进行执行对应服务，比如：打印字符
    movl $0xfff, %ecx   # 将值 0xfff 加载到 ECX 中，
                        # 设置循环计数器的初始值为4095.这个值控制了循环次数
1:  loop 1b             # 定义一个标签1. loop 1b 将ECX减1,
                        # 如果ECX不为0，则继续跳转到标签1.起延时的作用
    jmp task0 

task1:
    movl $0x17, %eax
    movw %ax, %ds
    movb $66, %al       /* print 'B' */
    int $0x80
    movl $0xfff, %ecx
1:  loop 1b
    jmp task1

    .fill 128,4,0 
usr_stk1:
```

