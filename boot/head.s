/*
 *  head.s contains the 32-bit startup code.
 *
 * NOTE!!! Startup happens at absolute address 0x00000000, which is also where
 * the page directory will exist. The startup code will be overwritten by
 * the page directory.
 */
.code32
.text
.globl idt,gdt,pg_dir,startup_32
pg_dir:
startup_32:
    movl $0x10,%eax         # EAX: 0x10
    mov %ax,%ds             # DS
    mov %ax,%es             # ES
    mov %ax,%fs             # FS
    mov %ax,%gs             # GS
    lss stack_start,%esp    # lss 从内存中加载段选择器和偏移量，到目标段寄存器
                            # kernel/sched.c 中定义 stack_start
                            # ESP 堆栈段
    call setup_idt          # 初始化IDT并加载，中断服务例程是默认函数
    call setup_gdt          # 初始化GDT并加载

    # reload all the segment registers
    movl $0x10,%eax         # EAX = 0x10, AX = 0x10
    mov %ax,%ds             # after changing gdt. CS was already reloaded in 'setup_gdt'
    mov %ax,%es
    mov %ax,%fs
    mov %ax,%gs
    #

    lss stack_start,%esp    # 段地址(SS中保存的才是选择子)
    xorl %eax,%eax

    # check that A20 really IS enabled
1:  incl %eax               # EAX = 0x00001
    movl %eax,0x000000      # 将 EAX 的值写入 0x000000
    cmpl %eax,0x100000      # 比较 EAX 与 0x100000 (1MB) 处值是否一致
    je 1b                   # 相等则向下找; 1b 表示向后查找 1 的标签
    movl %cr0,%eax          # check math chip
    andl $0x80000011,%eax   # Save PG,ET,PE
    testl $0x10,%eax
    jne 1f                  # ET is set - 387 is present
    orl $4,%eax             # else set emulate bit
1:  movl %eax,%cr0
    jmp after_page_tables

/*
 *  setup_idt
 *
 *  sets up a idt with 256 entries pointing to
 *  ignore_int, interrupt gates. It then loads
 *  idt. Everything that wants to install itself
 *  in the idt-table may do so themselves. Interrupts
 *  are enabled elsewhere, when we can be relatively
 *  sure everything is ok. This routine will be over-
 *  written by the page tables.
 */
setup_idt:
    lea ignore_int,%edx     # 设置默认中断处理程序
    movl $0x00080000,%eax
    movw %dx,%ax            # selector = 0x0008 = cs
    movw $0x8E00,%dx        # interrupt gate - dpl=0, present

    lea idt,%edi            # 
    mov $256,%ecx
rp_sidt:
    movl %eax,(%edi)        # 段选择子——代码段
    movl %edx,4(%edi)       # 权限以及中断门类型设置
    addl $8,%edi            # 下一个 idt 描述符
    dec %ecx
    jne rp_sidt
    lidt idt_descr          # 加载idt指针
    ret

/*
 *  setup_gdt
 *
 *  This routines sets up a new gdt and loads it.
 *  Only two entries are currently built, the same
 *  ones that were built in init.s. The routine
 *  is VERY complicated at two whole lines, so this
 *  rather long comment is certainly needed :-).
 *  This routine will beoverwritten by the page tables.
 */
setup_gdt:
    lgdt gdt_descr
    ret

.org 0x1000
pg0:

.org 0x2000
pg1:

.org 0x3000
pg2:        # This is not used yet, but if you
        # want to expand past 8 Mb, you'll have
        # to use it.

.org 0x4000
after_page_tables:
    pushl $0        # These are the parameters to main :-)
    pushl $0
    pushl $0
    pushl $L6       # 将L6地址压入堆栈，
                    # return address for main, if it decides to.
    pushl $main
    jmp setup_paging # 设置分页、执行系统初始化操作
L6:
    jmp L6          # main不应该在此死循环
                    # main should never return here, but
                    # just in case, we know what happens.

/* This is the default interrupt "handler" :-) */
.align 2
ignore_int:
    incb 0xb8000+160        # put something on the screen
    movb $2,0xb8000+161     # so that we know something
    iret                # happened


/*
 * Setup_paging
 *
 * This routine sets up paging by setting the page bit
 * in cr0. The page tables are set up, identity-mapping
 * the first 8MB. The pager assumes that no illegal
 * addresses are produced (ie >4Mb on a 4Mb machine).
 *
 * NOTE! Although all physical memory should be identity
 * mapped by this routine, only the kernel page functions
 * use the >1Mb addresses directly. All "normal" functions
 * use just the lower 1Mb, or the local data space, which
 * will be mapped to some other place - mm keeps track of
 * that.
 *
 * For those with more memory than 8 Mb - tough luck. I've
 * not got it, why should you :-) The source is here. Change
 * it. (Seriously - it shouldn't be too difficult. Mostly
 * change some constants etc. I left it at 8Mb, as my machine
 * even cannot be extended past that (ok, but it was cheap :-)
 * I've tried to show which constants to change by having
 * some kind of marker at them (search for "8Mb"), but I
 * won't guarantee that's all :-( )
 */

/**
 * 初始化分页机制，并通过分页相关寄存器(CR0)开启分页功能
 *
 */
.align 2
setup_paging:
    movl $1024*3,%ecx       # 将1024*3=3071加载到ECX
    xorl %eax,%eax
    xorl %edi,%edi          /* pg_dir is at 0x000 */
    cld;                    # 清除方向标志位(DF=0)。
                            # 确保字符串操作指令按地址递增顺序操作
    rep;stosl
    movl $pg0+7,pg_dir      # 将地址pg0+7写入pg_dir。
                            # pg_dir 是页目录起始地址
                            # pg0+7表示也表pg0其实地址加权限位标志
                            #  7 表示:位0(p)=1页面存在,位1(rw)读写,位2(US)=1用户模式
                            # set present bit/user r/w
    movl $pg1+7,pg_dir+4    # 页目录第2项指向页表pg1 /*  --------- " " --------- */
    movl $pg1+4092,%edi     # 将pg1+4092的地址写入EDI
                            # EDI通常用作目标地址寄存器，这里指向页表pg1的末尾
    movl $0x7ff007,%eax     # 将0x7ff007加载到EAX，作为页表条目的初始值。
                            # 高20位(0x7FF):页表条目指向的页框号
                            # 低12位(0x007):页条目的权限标志。
                            # 8Mb - 4096 + 7 (r/w user,p)
    std                     # 设置方向标志(DF=1)，使后续字符串操作指令反向(地址递减)操作
1:  stosl                   # 将EAX的值写入[EDI]，即将页表条目写入内存
                            # fill pages backwards - more efficient :-) */
    subl $0x1000,%eax       # 减少EAX的值0x1000(4kb),准备下一个页表条目
    jge 1b                  # 如果减法结果大于等于0(SF=0)，跳回标签1.
                            # 循环持续，直到EAX小于0为止
                            # 逐步初始化页表，从高地址(pg1+4092)向低地址填充
    xorl %eax,%eax          # 清空EAX /* pg_dir is at 0x0000 */
    movl %eax,%cr3          # 将EAX的值(0)加载到控制寄存器CR3
                            #  CR3保存页目录的物理地址，用于分页表的起始地址。
                            #  这里EAX为0，说明pg_dit的地址可能已经被映射到某固定的物理地址
                            # cr3 - page directory start */
    movl %cr0,%eax          # 将控制寄存器CR0的值加载到EAX
                            #  CR0包含控制寄存器标志，其中第31位(PG)用于控制分页。
    orl $0x80000000,%eax    # 对EAX的值按位或 0x80000000:
                            #  设置位31(PG) = 1，启用分页。
    movl %eax,%cr0          # 将更新后的EAX写入控制寄存器CR0。
                            # 开启分页功能(CR0.PG = 1)
                            # set paging (PG) bit
    ret                     # 返回调用处，分页初始化完成
                            # this also flushes prefetch-queue */

.align 2
.word 0
idt_descr:
    .word 256*8-1       # idt contains 256 entries
    .long idt
.align 2
.word 0
gdt_descr:
    .word 256*8-1       # GDT 大小 so does gdt (not that that's any
    .long gdt           # GDT 首地址

    .align 8
idt:    .fill 256,8,0       # idt is uninitialized

gdt: .quad 0x0000000000000000   /* NULL descriptor */
    .quad 0x00c09a00000007ff    /* 8Mb */
    .quad 0x00c09200000007ff    /* 8Mb */
    .quad 0x0000000000000000    /* TEMPORARY - don't use */
    .fill 252,8,0               /* space for LDT's and TSS's etc */
