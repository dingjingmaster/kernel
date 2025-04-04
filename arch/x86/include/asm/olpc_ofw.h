/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _ASM_X86_OLPC_OFW_H
#define _ASM_X86_OLPC_OFW_H

/* index into the page table containing the entry OFW occupies */
#define OLPC_OFW_PDE_NR 1022

#define OLPC_OFW_SIG 0x2057464F	/* aka "OFW " */

#ifdef CONFIG_OLPC

extern bool olpc_ofw_is_installed(void);

/* run an OFW command by calling into the firmware */
#define olpc_ofw(name, args, res) \
	__olpc_ofw((name), ARRAY_SIZE(args), args, ARRAY_SIZE(res), res)

extern int __olpc_ofw(const char *name, int nr_args, const void **args, int nr_res,
		void **res);

/* determine whether OFW is available and lives in the proper memory */
extern void olpc_ofw_detect(void);

/* install OFW's pde permanently into the kernel's pgtable */
extern void setup_olpc_ofw_pgd(void);

/* check if OFW was detected during boot */
extern bool olpc_ofw_present(void);

extern void olpc_dt_build_devicetree(void);

#else /* !CONFIG_OLPC */
/**
 * @brief
 * 在启动过程中检测是否存在 ​OLPC Open Firmware (OFW)
 *
 * OLPC（One Laptop Per Child）项目使用 Open Firmware 作为其固件接口，以支持特定的硬件初始化和引导过程。
 * olpc_ofw_detect 的主要作用是识别系统是否基于 OLPC 的硬件平台，并相应地进行后续的引导和初始化操作。
 */
static inline void olpc_ofw_detect(void) { }
static inline void setup_olpc_ofw_pgd(void) { }
static inline void olpc_dt_build_devicetree(void) { }
#endif /* !CONFIG_OLPC */

#endif /* _ASM_X86_OLPC_OFW_H */
