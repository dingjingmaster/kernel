# 数据类型


## cs_detail

`cs_detail`结构体用于存储指令的详细分析信息，是`cs_insn`结构体中`cs_detail* detail`字段指向的类型。只有在反汇编时通过`cs_option`启用`CS_OPT_DETAIL`模式(即：`cs_option(handle, CS_OPT_DETAIL, CS_OPT_ON)`时，`cs_detail`才会被填充，否则`detail`指针为NULL。`cs_detail`的具体字段因架构(如：x86、ARM、MIPS等)而已，但其通用部分和架构特定部分可以系统的分析)。

以下是对`cs_detail`结构体通用字段以及架构特定字段(以x86为例)的详细解读，说明每个字段的功能、作用和使用场景。需要注意的是，capstone的头文件(如：capstone.h和架构特定文件如x86.h)定义了这些结构，具体实现可能因版本略有变化。

```c
typedef struct cs_detail 
{
    uint8_t regs_read[16];      // 读的寄存器列表
    uint8_t regs_read_count;    // 读寄存器数量
    uint8_t regs_write[32];     // 写的寄存器列表
    uint8_t regs_write_count;   // 写寄存器数量
    uint8_t groups[8];          // 指令所属的分组
    uint8_t groups_count;       // 分组数量
    // 架构特定的详细信息（联合体）
    union {
        cs_x86 x86;             // x86 架构的详细信息
        cs_arm64 arm64;         // ARM64 架构的详细信息
        cs_arm arm;             // ARM 架构的详细信息
        cs_mips mips;           // MIPS 架构的详细信息
        cs_ppc ppc;             // PowerPC 架构的详细信息
        cs_sparc sparc;         // SPARC 架构的详细信息
        cs_sysz sysz;           // SystemZ 架构的详细信息
        cs_xcore xcore;         // XCore 架构的详细信息
        cs_m68k m68k;           // M68K 架构的详细信息
        cs_tms320c64x tms320c64x; // TMS320C64x 架构的详细信息
        cs_m680x m680x;         // M680X 架构的详细信息
        cs_evm evm;             // Ethereum 架构的详细信息
        cs_mos65xx mos65xx;     // MOS65XX 架构的详细信息
        cs_wasm wasm;           // WebAssembly 架构的详细信息
        cs_bpf bpf;             // BPF 架构的详细信息
        cs_riscv riscv;         // RISC-V 架构的详细信息
        cs_sh sh;               // SH 架构的详细信息
        cs_tricore tricore;     // TriCore 架构的详细信息
    };
} cs_detail;
```

- regs_read：存储指令隐式或显式读取的寄存器ID列表，数组最大支持16个寄存器
- regs_read_cout：表示regs_read数组中有效的寄存器数量(0到16)
- regs_write：存储指令隐式或显式写入的寄存器ID列表，数组最大支持32个寄存器
- regs_write_count：表示 regs_write 数组中有效的寄存器数量(0到32)
- groups：存储指令所属的分组ID列表，数组最大支持8个分组。
- groups_count：表示groups数组中有效的分组数量(0到8)

```c
typedef struct cs_x86 
{
    uint8_t prefix[4];          // 指令前缀字节
    uint8_t opcode[4];          // 操作码字节
    uint8_t rex;                // REX 前缀（64 位模式）
    uint8_t addr_size;          // 地址大小（16/32/64 位）
    uint8_t modrm;              // ModR/M 字节
    uint8_t sib;                // SIB 字节
    int32_t disp;               // 偏移量（displacement）
    uint8_t sib_index;          // SIB 索引寄存器
    uint8_t sib_scale;          // SIB 缩放因子
    uint8_t sib_base;           // SIB 基址寄存器
    x86_reg imm_size;           // 立即数大小（字节）
    uint8_t xop_cc;             // XOP 条件码
    uint8_t sse_cc;             // SSE 条件码
    uint8_t avx_cc;             // AVX 条件码
    uint8_t avx_sae;            // AVX SAE（Suppress All Exceptions）
    uint8_t avx_rm;             // AVX 舍入模式
    cs_x86_op operands[8];      // 操作数列表
    uint8_t op_count;           // 操作数数量
    uint8_t encoding;           // 编码信息
    cs_x86_eflags eflags;       // EFLAGS 标志修改信息
} cs_x86;
```

- prefix：存储指令的前缀字节，最多4个(x86指令可能包含前缀，如锁前缀(LOCK)、段前缀(CS:、DS:)、操作数大小覆盖(66h)、地址大小覆盖(67h))。prefix数组按顺序存储这些前缀字节，空槽填充为0
- opcode：存储指令的操作字节码，最多4个
- rex：存储REX前缀字节(仅64位模式)。REX前缀(40h到4Fh)用于扩展寄存器(如r8到r15)、指定64位操作数大小或修改ModR/M字段。如果指令没有REX前缀，此字段为0
- addr_size：表示指令的地址大小(16、32或64位)。指定指令的内存寻址模式(例如, [eax]是32位寻址还是16位寻址，受地址大小前缀67h或CPU模式影响)
- modrm：存储ModR/M字节。ModR/M字节编码操作数的寻址模式和寄存器信息（如寄存器到寄存器操作或内存寻址）。如果指令没有ModR/M字节，此字段为0.
- sib：存储SIB字节
- sib_index：SIB的索引寄存器ID
- sib_scale：SIB的缩放因子(1、2、4、或8)
- sib_base：SIB的基址寄存器ID
- disp：存储指令的偏移量，表示内存寻址中的偏移值，如[eax + 8]中的8。可能是8位、16位或32位由符号证书，具体大小由指令编码决定。
- imm_size：表示立即数的大小(字节)。记录指令中立即数操作数的长度（如：1、2、4、或8字节）。如果指令没有立即数，此字段无效。
- xop_cc、sse_cc、avx_cc：存储XOP、SSE或AVX指令的条件码。表示向量指令的条件执行行为（如比较结果的掩码）。如果指令不涉及这些扩展，此字段为0。
- avx_sae：表示AVX指令是否启用ASE(Supress All Exceptions)
- avx_rm：表示AVX指令的舍入模式
- operands：存储指令的操作数详细信息，数组最大支持8个操作数
- op_count：表示operands数组中有效的操作数数量(0到8)
- encoding：存储指令的编码信息。提供指令编码的额外元数组（如：是否为VEX编码）
- eflags：描述指令对EFLAGS寄存器的修改。记录指令如何影响标志寄存器(如零标志ZF、进位标志CF)

```c
typedef struct cs_x86_op 
{
    x86_op_type type;       // 操作数类型（寄存器、立即数、内存等）
    union {
        x86_reg reg;        // 寄存器 ID
        int64_t imm;        // 立即数
        double fp;          // 浮点数（未广泛使用）
        cs_x86_op_mem mem;  // 内存操作数
    };
    uint8_t size;           // 操作数大小（字节）
    uint8_t access;         // 访问类型（读、写或读写）
    uint8_t avx_bcast;      // AVX 广播因子
    uint8_t avx_zero;       // AVX 零掩码
} cs_x86_op;
```

```c
typedef struct cs_x86_op_mem 
{
    x86_reg segment;    // 段寄存器
    x86_reg base;       // 基址寄存器
    x86_reg index;      // 索引寄存器
    uint8_t scale;      // 缩放因子
    int64_t disp;       // 偏移量
} cs_x86_op_mem;
```
