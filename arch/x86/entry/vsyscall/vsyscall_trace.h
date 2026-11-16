/* SPDX-License-Identifier: GPL-2.0 */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM vsyscall
/* vsyscall 模拟路径的 tracepoint 定义。 */

#if !defined(__VSYSCALL_TRACE_H) || defined(TRACE_HEADER_MULTI_READ)
#define __VSYSCALL_TRACE_H

#include <linux/tracepoint.h>

TRACE_EVENT(emulate_vsyscall,

	    TP_PROTO(int nr),
            /* nr 表示被模拟的 vsyscall 槽位。 */

	    TP_ARGS(nr),

	    TP_STRUCT__entry(__field(int, nr)),
            /* trace 记录中只保存 vsyscall 编号。 */

	    TP_fast_assign(
			   __entry->nr = nr;
			   ),

	    TP_printk("nr = %d", __entry->nr)
);

#endif

#undef TRACE_INCLUDE_PATH
/* 指定 trace 头文件生成器回找当前头文件的位置。 */
#define TRACE_INCLUDE_PATH ../../arch/x86/entry/vsyscall/
#define TRACE_INCLUDE_FILE vsyscall_trace
#include <trace/define_trace.h>
