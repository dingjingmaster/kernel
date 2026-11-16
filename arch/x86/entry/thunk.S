/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Save registers before calling assembly functions. This avoids
 * disturbance of register allocation in some inline assembly constructs.
 *
 * 调用汇编函数前保存寄存器，避免某些内联汇编场景破坏编译器寄存器分配假设。
 * Copyright 2001,2002 by Andi Kleen, SuSE Labs.
 */
#include <linux/export.h>
#include <linux/linkage.h>
#include "calling.h"
#include <asm/asm.h>

THUNK preempt_schedule_thunk, preempt_schedule                  /* 生成 preempt_schedule 的寄存器保存 thunk。 */
THUNK preempt_schedule_notrace_thunk, preempt_schedule_notrace  /* 生成 notrace 抢占调度 thunk。 */
EXPORT_SYMBOL(preempt_schedule_thunk)                           /* 导出 preempt_schedule_thunk 供其它对象引用。 */
EXPORT_SYMBOL(preempt_schedule_notrace_thunk)                   /* 导出 preempt_schedule_notrace_thunk 供其它对象引用。 */
