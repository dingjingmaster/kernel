#include <asm/tdx.h>
#include <asm/pgtable.h>

/* 尝试按指定页级别 accept 一段 TDX 私有内存，成功返回实际 accept 的大小。 */
static unsigned long try_accept_one(phys_addr_t start, unsigned long len,
				    enum pg_level pg_level)
{
	unsigned long accept_size = page_level_size(pg_level);
	struct tdx_module_args args = {};
	u8 page_size;

        /* TDX 模块要求物理地址按目标页大小对齐。 */
	if (!IS_ALIGNED(start, accept_size))
		return 0;

        /* 当前剩余长度不足一个目标页时，不能按该页级别 accept。 */
	if (len < accept_size)
		return 0;

	/*
	 * Pass the page physical address to the TDX module to accept the
	 * pending, private page.
	 *
	 * Bits 2:0 of RCX encode page size: 0 - 4K, 1 - 2M, 2 - 1G.
         *
         * 将待接受页的物理地址传给 TDX 模块。RCX 低 3 位编码页大小:
         * 0 表示 4K，1 表示 2M，2 表示 1G。
	 */
	switch (pg_level) {
	case PG_LEVEL_4K:
                /* 4K 页 accept。 */
		page_size = TDX_PS_4K;
		break;
	case PG_LEVEL_2M:
                /* 2M 大页 accept。 */
		page_size = TDX_PS_2M;
		break;
	case PG_LEVEL_1G:
                /* 1G 大页 accept。 */
		page_size = TDX_PS_1G;
		break;
	default:
                /* 不支持的页级别不执行 TDCALL。 */
		return 0;
	}

        /* 物理地址与页大小编码组合后放入 RCX。 */
	args.rcx = start | page_size;
	if (__tdcall(TDG_MEM_PAGE_ACCEPT, &args))
		return 0;

	return accept_size;
}

bool tdx_accept_memory(phys_addr_t start, phys_addr_t end)
{
	/*
	 * For shared->private conversion, accept the page using
	 * TDG_MEM_PAGE_ACCEPT TDX module call.
         *
         * 共享页转私有页时，必须通过 TDG_MEM_PAGE_ACCEPT 让 TDX 模块接受该页。
	 */
	while (start < end) {
		unsigned long len = end - start;
		unsigned long accept_size;

		/*
		 * Try larger accepts first. It gives chance to VMM to keep
		 * 1G/2M Secure EPT entries where possible and speeds up
		 * process by cutting number of hypercalls (if successful).
                 *
                 * 优先尝试 1G/2M 大页，既有利于 VMM 保持大页 Secure EPT 映射，
                 * 也能减少 hypercall 次数。
		 */

                /* 按 1G、2M、4K 顺序降级尝试。 */
		accept_size = try_accept_one(start, len, PG_LEVEL_1G);
		if (!accept_size)
			accept_size = try_accept_one(start, len, PG_LEVEL_2M);
		if (!accept_size)
			accept_size = try_accept_one(start, len, PG_LEVEL_4K);
		if (!accept_size)
                        /* 三种页大小都失败时，说明该区间无法完整 accept。 */
			return false;
		start += accept_size;
	}

	return true;
}

noinstr u64 __tdx_hypercall(struct tdx_module_args *args)
{
	/*
	 * For TDVMCALL explicitly set RCX to the bitmap of shared registers.
	 * The caller isn't expected to set @args->rcx anyway.
         *
         * TDVMCALL 需要 RCX 指定要暴露给 VMM 的寄存器位图，调用者无需自己设置。
	 */
	args->rcx = TDVMCALL_EXPOSE_REGS_MASK;

	/*
	 * Failure of __tdcall_saved_ret() indicates a failure of the TDVMCALL
	 * mechanism itself and that something has gone horribly wrong with
	 * the TDX module.  __tdx_hypercall_failed() never returns.
         *
         * __tdcall_saved_ret() 非零表示 TDCALL/TDVMCALL 机制本身失败，
         * 这不是普通 VMM 返回错误，必须进入不可返回的失败处理。
	 */
	if (__tdcall_saved_ret(TDG_VP_VMCALL, args))
		__tdx_hypercall_failed();

	/* TDVMCALL leaf return code is in R10 */
        /* TDVMCALL leaf 的返回码保存在 R10。 */
	return args->r10;
}
