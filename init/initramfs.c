// SPDX-License-Identifier: GPL-2.0
#include <linux/init.h>
#include <linux/async.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/fcntl.h>
#include <linux/delay.h>
#include <linux/string.h>
#include <linux/dirent.h>
#include <linux/syscalls.h>
#include <linux/utime.h>
#include <linux/file.h>
#include <linux/kstrtox.h>
#include <linux/memblock.h>
#include <linux/mm.h>
#include <linux/namei.h>
#include <linux/init_syscalls.h>
#include <linux/umh.h>
#include <linux/security.h>

#include "do_mounts.h"

static __initdata bool csum_present;
static __initdata u32 io_csum;

static ssize_t __init xwrite(struct file *file, const unsigned char *p,
		size_t count, loff_t *pos)
{
	ssize_t out = 0;

	/* sys_write only can write MAX_RW_COUNT aka 2G-4K bytes at most */
	/* kernel_write() 单次最多只能写 MAX_RW_COUNT，即约 2G-4K 字节。 */
	while (count) {
		ssize_t rv = kernel_write(file, p, count, pos);

		if (rv < 0) {
			if (rv == -EINTR || rv == -EAGAIN)
				continue;
			return out ? out : rv;
		} else if (rv == 0)
			break;

		if (csum_present) {
			ssize_t i;

			for (i = 0; i < rv; i++)
				io_csum += p[i];
		}

		p += rv;
		out += rv;
		count -= rv;
	}

	return out;
}

static __initdata char *message;
static void __init error(char *x)
{
	/* 只记录第一个错误，后续错误不覆盖，便于保留最早失败原因。 */
	if (!message)
		message = x;
}

#define panic_show_mem(fmt, ...) \
	({ show_mem(); panic(fmt, ##__VA_ARGS__); })

/* link hash */
/* cpio 硬链接哈希表，用于根据设备号、inode 和类型找到已创建的同源文件。 */

#define N_ALIGN(len) ((((len) + 1) & ~3) + 2)

static __initdata struct hash {
	int ino, minor, major;
	umode_t mode;
	struct hash *next;
	char name[N_ALIGN(PATH_MAX)];
} *head[32];

static inline int hash(int major, int minor, int ino)
{
	/* 简单混合设备号和 inode，映射到 32 个桶。 */
	unsigned long tmp = ino + minor + (major << 3);
	tmp += tmp >> 5;
	return tmp & 31;
}

static char __init *find_link(int major, int minor, int ino,
			      umode_t mode, char *name)
{
	struct hash **p, *q;

	/* 如果相同对象已经出现过，返回首次解包出的路径，用于创建硬链接。 */
	for (p = head + hash(major, minor, ino); *p; p = &(*p)->next) {
		if ((*p)->ino != ino)
			continue;
		if ((*p)->minor != minor)
			continue;
		if ((*p)->major != major)
			continue;
		if (((*p)->mode ^ mode) & S_IFMT)
			continue;
		return (*p)->name;
	}

	/* 第一次遇到该对象时记录路径，后续同源条目可以链接到它。 */
	q = kmalloc(sizeof(struct hash), GFP_KERNEL);
	if (!q)
		panic_show_mem("can't allocate link hash entry");
	q->major = major;
	q->minor = minor;
	q->ino = ino;
	q->mode = mode;
	strcpy(q->name, name);
	q->next = NULL;
	*p = q;
	return NULL;
}

static void __init free_hash(void)
{
	struct hash **p, *q;

	/* 解包结束后释放硬链接查找表占用的临时内存。 */
	for (p = head; p < head + 32; p++) {
		while (*p) {
			q = *p;
			*p = q->next;
			kfree(q);
		}
	}
}

#ifdef CONFIG_INITRAMFS_PRESERVE_MTIME
static void __init do_utime(char *filename, time64_t mtime)
{
	/* 将文件的访问时间和修改时间都恢复为 cpio 头中的 mtime。 */
	struct timespec64 t[2] = { { .tv_sec = mtime }, { .tv_sec = mtime } };
	init_utimes(filename, t);
}

static void __init do_utime_path(const struct path *path, time64_t mtime)
{
	/* 已持有 path 的场景直接通过 vfs_utimes() 设置时间，避免重新路径查找。 */
	struct timespec64 t[2] = { { .tv_sec = mtime }, { .tv_sec = mtime } };
	vfs_utimes(path, t);
}

static __initdata LIST_HEAD(dir_list);
struct dir_entry {
	struct list_head list;
	time64_t mtime;
	char name[];
};

static void __init dir_add(const char *name, time64_t mtime)
{
	size_t nlen = strlen(name) + 1;
	struct dir_entry *de;

	/* 目录时间戳需要延后设置，避免后续在目录内创建文件时再次更新目录 mtime。 */
	de = kmalloc(sizeof(struct dir_entry) + nlen, GFP_KERNEL);
	if (!de)
		panic_show_mem("can't allocate dir_entry buffer");
	INIT_LIST_HEAD(&de->list);
	strscpy(de->name, name, nlen);
	de->mtime = mtime;
	list_add(&de->list, &dir_list);
}

static void __init dir_utime(void)
{
	struct dir_entry *de, *tmp;

	/* 所有条目解包完成后，再统一恢复目录 mtime。 */
	list_for_each_entry_safe(de, tmp, &dir_list, list) {
		list_del(&de->list);
		do_utime(de->name, de->mtime);
		kfree(de);
	}
}
#else
static void __init do_utime(char *filename, time64_t mtime) {}
static void __init do_utime_path(const struct path *path, time64_t mtime) {}
static void __init dir_add(const char *name, time64_t mtime) {}
static void __init dir_utime(void) {}
#endif

static __initdata time64_t mtime;

/* cpio header parsing */
/* newc/crc 格式 cpio 头解析，字段均为 ASCII 十六进制字符串。 */

static __initdata unsigned long ino, major, minor, nlink;
static __initdata umode_t mode;
static __initdata unsigned long body_len, name_len;
static __initdata uid_t uid;
static __initdata gid_t gid;
static __initdata unsigned rdev;
static __initdata u32 hdr_csum;

static void __init parse_header(char *s)
{
	unsigned long parsed[13];
	char buf[9];
	int i;

	/* 跳过 6 字节 magic，随后依次解析 13 个 8 字节十六进制字段。 */
	buf[8] = '\0';
	for (i = 0, s += 6; i < 13; i++, s += 8) {
		memcpy(buf, s, 8);
		parsed[i] = simple_strtoul(buf, NULL, 16);
	}
	ino = parsed[0];
	mode = parsed[1];
	uid = parsed[2];
	gid = parsed[3];
	nlink = parsed[4];
	mtime = parsed[5]; /* breaks in y2106 */
	/* mtime 使用 32 位秒值编码，理论上会在 2106 年溢出。 */
	body_len = parsed[6];
	major = parsed[7];
	minor = parsed[8];
	rdev = new_encode_dev(MKDEV(parsed[9], parsed[10]));
	name_len = parsed[11];
	hdr_csum = parsed[12];
}

/* FSM */
/* 解包 cpio 流的有限状态机。 */

static __initdata enum state {
	Start,		/* 准备读取 cpio 头。 */
	Collect,	/* 当前输入不足，先收集到临时缓冲区。 */
	GotHeader,	/* 已取得完整头部，准备解析字段。 */
	SkipIt,		/* 跳过当前条目剩余数据或对齐填充。 */
	GotName,	/* 已取得路径名，准备创建对应文件节点。 */
	CopyFile,	/* 正在写入普通文件内容。 */
	GotSymlink,	/* 已取得符号链接名和目标。 */
	Reset		/* 一个归档成员结束，清理填充并准备下一个成员。 */
} state, next_state;

static __initdata char *victim;
static unsigned long byte_count __initdata;
static __initdata loff_t this_header, next_header;

static inline void __init eat(unsigned n)
{
	/* 消耗当前输入缓冲区中的 n 字节，并同步推进归档偏移。 */
	victim += n;
	this_header += n;
	byte_count -= n;
}

static __initdata char *collected;
static long remains __initdata;
static __initdata char *collect;

static void __init read_into(char *buf, unsigned size, enum state next)
{
	/* 如果当前输入足够，直接引用输入缓冲区；否则复制到指定临时缓冲区。 */
	if (byte_count >= size) {
		collected = victim;
		eat(size);
		state = next;
	} else {
		collect = collected = buf;
		remains = size;
		next_state = next;
		state = Collect;
	}
}

static __initdata char *header_buf, *symlink_buf, *name_buf;

static int __init do_start(void)
{
	/* newc/crc cpio 头固定为 110 字节。 */
	read_into(header_buf, 110, GotHeader);
	return 0;
}

static int __init do_collect(void)
{
	unsigned long n = remains;

	/* 跨输入块收集数据，直到满足 read_into() 请求的长度。 */
	if (byte_count < n)
		n = byte_count;
	memcpy(collect, victim, n);
	eat(n);
	collect += n;
	if ((remains -= n) != 0)
		return 1;
	state = next_state;
	return 0;
}

static int __init do_header(void)
{
	/* 支持 newc(070701) 和 crc(070702)；老式 070707 不被接受。 */
	if (!memcmp(collected, "070701", 6)) {
		csum_present = false;
	} else if (!memcmp(collected, "070702", 6)) {
		csum_present = true;
	} else {
		if (memcmp(collected, "070707", 6) == 0)
			error("incorrect cpio method used: use -H newc option");
		else
			error("no cpio magic");
		return 1;
	}
	parse_header(collected);

	/* 计算下一个头部的偏移，文件名和文件体都按 4 字节边界对齐。 */
	next_header = this_header + N_ALIGN(name_len) + body_len;
	next_header = (next_header + 3) & ~3;
	state = SkipIt;
	if (name_len <= 0 || name_len > PATH_MAX)
		return 0;
	if (S_ISLNK(mode)) {
		if (body_len > PATH_MAX)
			return 0;
		/* 符号链接需要同时收集路径名和链接目标。 */
		collect = collected = symlink_buf;
		remains = N_ALIGN(name_len) + body_len;
		next_state = GotSymlink;
		state = Collect;
		return 0;
	}
	if (S_ISREG(mode) || !body_len)
		read_into(name_buf, N_ALIGN(name_len), GotName);
	return 0;
}

static int __init do_skip(void)
{
	/* 跳到当前 cpio 条目的结束位置，可能跨多个输入块。 */
	if (this_header + byte_count < next_header) {
		eat(byte_count);
		return 1;
	} else {
		eat(next_header - this_header);
		state = next_state;
		return 0;
	}
}

static int __init do_reset(void)
{
	/* 跳过归档成员之间的 NUL 填充，并检查头部是否保持 4 字节对齐。 */
	while (byte_count && *victim == '\0')
		eat(1);
	if (byte_count && (this_header & 3))
		error("broken padding");
	return 1;
}

static void __init clean_path(char *path, umode_t fmode)
{
	struct kstat st;

	/* 如果目标路径已存在但文件类型不同，先删除旧对象，避免创建失败。 */
	if (!init_stat(path, &st, AT_SYMLINK_NOFOLLOW) &&
	    (st.mode ^ fmode) & S_IFMT) {
		if (S_ISDIR(st.mode))
			init_rmdir(path);
		else
			init_unlink(path);
	}
}

static int __init maybe_link(void)
{
	/* 多硬链接文件先尝试复用已解包路径，避免重复创建同一 inode 内容。 */
	if (nlink >= 2) {
		char *old = find_link(major, minor, ino, mode, collected);
		if (old) {
			clean_path(collected, 0);
			return (init_link(old, collected) < 0) ? -1 : 1;
		}
	}
	return 0;
}

static __initdata struct file *wfile;
static __initdata loff_t wfile_pos;

static int __init do_name(void)
{
	state = SkipIt;
	next_state = Reset;

	/* TRAILER!!! 是 cpio 归档结束标记。 */
	if (strcmp(collected, "TRAILER!!!") == 0) {
		free_hash();
		return 0;
	}
	clean_path(collected, mode);
	if (S_ISREG(mode)) {
		/* 普通文件可能是硬链接，也可能需要新建并写入文件体。 */
		int ml = maybe_link();
		if (ml >= 0) {
			int openflags = O_WRONLY|O_CREAT|O_LARGEFILE;
			if (ml != 1)
				openflags |= O_TRUNC;
			wfile = filp_open(collected, openflags, mode);
			if (IS_ERR(wfile))
				return 0;
			wfile_pos = 0;
			io_csum = 0;

			/* 先设置属主、权限和长度，再进入 CopyFile 状态写入内容。 */
			vfs_fchown(wfile, uid, gid);
			vfs_fchmod(wfile, mode);
			if (body_len)
				vfs_truncate(&wfile->f_path, body_len);
			state = CopyFile;
		}
	} else if (S_ISDIR(mode)) {
		/* 目录时间戳延后恢复，避免目录内容创建过程修改 mtime。 */
		init_mkdir(collected, mode);
		init_chown(collected, uid, gid, 0);
		init_chmod(collected, mode);
		dir_add(collected, mtime);
	} else if (S_ISBLK(mode) || S_ISCHR(mode) ||
		   S_ISFIFO(mode) || S_ISSOCK(mode)) {
		/* 设备节点、FIFO 和 socket 通过 mknod 创建，同样处理属主和权限。 */
		if (maybe_link() == 0) {
			init_mknod(collected, mode, rdev);
			init_chown(collected, uid, gid, 0);
			init_chmod(collected, mode);
			do_utime(collected, mtime);
		}
	}
	return 0;
}

static int __init do_copy(void)
{
	/* 写入普通文件内容；输入不足时保留状态，等待下一段缓冲区继续写。 */
	if (byte_count >= body_len) {
		if (xwrite(wfile, victim, body_len, &wfile_pos) != body_len)
			error("write error");

		do_utime_path(&wfile->f_path, mtime);
		fput(wfile);
		/* crc cpio 格式需要校验文件内容累加和。 */
		if (csum_present && io_csum != hdr_csum)
			error("bad data checksum");
		eat(body_len);
		state = SkipIt;
		return 0;
	} else {
		if (xwrite(wfile, victim, byte_count, &wfile_pos) != byte_count)
			error("write error");
		body_len -= byte_count;
		eat(byte_count);
		return 1;
	}
}

static int __init do_symlink(void)
{
	/* collected 前半部分是路径名，后半部分是符号链接目标。 */
	collected[N_ALIGN(name_len) + body_len] = '\0';
	clean_path(collected, 0);
	init_symlink(collected + N_ALIGN(name_len), collected);
	init_chown(collected, uid, gid, AT_SYMLINK_NOFOLLOW);
	do_utime(collected, mtime);
	state = SkipIt;
	next_state = Reset;
	return 0;
}

static __initdata int (*actions[])(void) = {
	[Start]		= do_start,
	[Collect]	= do_collect,
	[GotHeader]	= do_header,
	[SkipIt]	= do_skip,
	[GotName]	= do_name,
	[CopyFile]	= do_copy,
	[GotSymlink]	= do_symlink,
	[Reset]		= do_reset,
};

static long __init write_buffer(char *buf, unsigned long len)
{
	byte_count = len;
	victim = buf;

	/* 驱动状态机消费当前输入块，直到需要更多输入或本块处理完成。 */
	while (!actions[state]())
		;
	return len - byte_count;
}

static long __init flush_buffer(void *bufv, unsigned long len)
{
	char *buf = bufv;
	long written;
	long origLen = len;

	/* 解压器的输出回调，将解压后的 cpio 数据继续喂给状态机。 */
	if (message)
		return -1;
	while ((written = write_buffer(buf, len)) < len && !message) {
		char c = buf[written];
		/* 一个压缩归档结束后，可能紧跟未压缩 cpio 或 NUL 填充。 */
		if (c == '0') {
			buf += written;
			len -= written;
			state = Start;
		} else if (c == 0) {
			buf += written;
			len -= written;
			state = Reset;
		} else
			error("junk within compressed archive");
	}
	return origLen;
}

static unsigned long my_inptr __initdata; /* index of next byte to be processed in inbuf */
/* my_inptr 表示解压器在输入缓冲区中已经处理到的位置。 */

#include <linux/decompress/generic.h>

static char * __init unpack_to_rootfs(char *buf, unsigned long len)
{
	long written;
	decompress_fn decompress;
	const char *compress_name;
	static __initdata char msg_buf[64];

	/* 分配状态机使用的临时缓冲区: 头部、符号链接和路径名。 */
	header_buf = kmalloc(110, GFP_KERNEL);
	symlink_buf = kmalloc(PATH_MAX + N_ALIGN(PATH_MAX) + 1, GFP_KERNEL);
	name_buf = kmalloc(N_ALIGN(PATH_MAX), GFP_KERNEL);

	if (!header_buf || !symlink_buf || !name_buf)
		panic_show_mem("can't allocate buffers");

	state = Start;
	this_header = 0;
	message = NULL;
	while (!message && len) {
		loff_t saved_offset = this_header;
		/* 未压缩 newc/crc cpio 以 ASCII '0' 开头，且头部应 4 字节对齐。 */
		if (*buf == '0' && !(this_header & 3)) {
			state = Start;
			written = write_buffer(buf, len);
			buf += written;
			len -= written;
			continue;
		}
		if (!*buf) {
			buf++;
			len--;
			this_header++;
			continue;
		}
		this_header = 0;
		/* 非 cpio 明文时尝试识别并调用对应解压器。 */
		decompress = decompress_method(buf, len, &compress_name);
		pr_debug("Detected %s compressed data\n", compress_name);
		if (decompress) {
			int res = decompress(buf, len, NULL, flush_buffer, NULL,
				   &my_inptr, error);
			if (res)
				error("decompressor failed");
		} else if (compress_name) {
			if (!message) {
				snprintf(msg_buf, sizeof msg_buf,
					 "compression method %s not configured",
					 compress_name);
				message = msg_buf;
			}
		} else
			error("invalid magic at start of compressed archive");
		if (state != Reset)
			error("junk at the end of compressed archive");
		/* my_inptr 告诉外层本次压缩流消耗了多少原始输入。 */
		this_header = saved_offset + my_inptr;
		buf += my_inptr;
		len -= my_inptr;
	}
	/* 所有文件创建完成后再恢复目录时间戳，并释放临时缓冲区。 */
	dir_utime();
	kfree(name_buf);
	kfree(symlink_buf);
	kfree(header_buf);
	return message;
}

static int __initdata do_retain_initrd;

static int __init retain_initrd_param(char *str)
{
	/* retain_initrd 不带参数时生效，保留外部 initrd 供后续访问。 */
	if (*str)
		return 0;
	do_retain_initrd = 1;
	return 1;
}
__setup("retain_initrd", retain_initrd_param);

#ifdef CONFIG_ARCH_HAS_KEEPINITRD
static int __init keepinitrd_setup(char *__unused)
{
	/* 部分架构兼容 keepinitrd 参数，同样表示不要释放 initrd。 */
	do_retain_initrd = 1;
	return 1;
}
__setup("keepinitrd", keepinitrd_setup);
#endif

static bool __initdata initramfs_async = true;
static int __init initramfs_async_setup(char *str)
{
	/* initramfs_async= 可控制 rootfs 填充是否异步执行。 */
	return kstrtobool(str, &initramfs_async) == 0;
}
__setup("initramfs_async=", initramfs_async_setup);

extern char __initramfs_start[];
extern unsigned long __initramfs_size;
#include <linux/initrd.h>
#include <linux/kexec.h>

static BIN_ATTR(initrd, 0440, sysfs_bin_attr_simple_read, NULL, 0);

void __init reserve_initrd_mem(void)
{
	phys_addr_t start;
	unsigned long size;

	/* Ignore the virtul address computed during device tree parsing */
	/* 忽略设备树解析阶段临时计算出的虚拟地址，后续重新建立映射。 */
	initrd_start = initrd_end = 0;

	if (!phys_initrd_size)
		return;
	/*
	 * Round the memory region to page boundaries as per free_initrd_mem()
	 * This allows us to detect whether the pages overlapping the initrd
	 * are in use, but more importantly, reserves the entire set of pages
	 * as we don't want these pages allocated for other purposes.
	 *
	 * 按 free_initrd_mem() 的规则把 initrd 区域扩展到页边界。这样既能检测
	 * 与 initrd 重叠的页是否已被占用，更重要的是能完整保留覆盖 initrd 的
	 * 所有页，避免它们被分配给其它用途。
	 */
	start = round_down(phys_initrd_start, PAGE_SIZE);
	size = phys_initrd_size + (phys_initrd_start - start);
	size = round_up(size, PAGE_SIZE);

	if (!memblock_is_region_memory(start, size)) {
		pr_err("INITRD: 0x%08llx+0x%08lx is not a memory region",
		       (u64)start, size);
		goto disable;
	}

	if (memblock_is_region_reserved(start, size)) {
		pr_err("INITRD: 0x%08llx+0x%08lx overlaps in-use memory region\n",
		       (u64)start, size);
		goto disable;
	}

	memblock_reserve(start, size);
	/* Now convert initrd to virtual addresses */
	/* 现在将 initrd 的物理地址转换为内核虚拟地址。 */
	initrd_start = (unsigned long)__va(phys_initrd_start);
	initrd_end = initrd_start + phys_initrd_size;
	initrd_below_start_ok = 1;

	return;
disable:
	pr_cont(" - disabling initrd\n");
	initrd_start = 0;
	initrd_end = 0;
}

void __weak __init free_initrd_mem(unsigned long start, unsigned long end)
{
#ifdef CONFIG_ARCH_KEEP_MEMBLOCK
	unsigned long aligned_start = ALIGN_DOWN(start, PAGE_SIZE);
	unsigned long aligned_end = ALIGN(end, PAGE_SIZE);

	/* 架构保留 memblock 信息时，同步释放页对齐后的 memblock 区域。 */
	memblock_free((void *)aligned_start, aligned_end - aligned_start);
#endif

	/* 释放 initrd 的保留内存，并用 POISON_FREE_INITMEM 填充便于发现误用。 */
	free_reserved_area((void *)start, (void *)end, POISON_FREE_INITMEM,
			"initrd");
}

#ifdef CONFIG_CRASH_RESERVE
static bool __init kexec_free_initrd(void)
{
	unsigned long crashk_start = (unsigned long)__va(crashk_res.start);
	unsigned long crashk_end   = (unsigned long)__va(crashk_res.end);

	/*
	 * If the initrd region is overlapped with crashkernel reserved region,
	 * free only memory that is not part of crashkernel region.
	 *
	 * 如果 initrd 区域与 crashkernel 保留区重叠，只释放不属于 crashkernel
	 * 的那部分 initrd 内存。
	 */
	if (initrd_start >= crashk_end || initrd_end <= crashk_start)
		return false;

	/*
	 * Initialize initrd memory region since the kexec boot does not do.
	 *
	 * kexec 启动不会初始化 initrd 内存区域，因此这里先清零。
	 */
	memset((void *)initrd_start, 0, initrd_end - initrd_start);
	if (initrd_start < crashk_start)
		free_initrd_mem(initrd_start, crashk_start);
	if (initrd_end > crashk_end)
		free_initrd_mem(crashk_end, initrd_end);
	return true;
}
#else
static inline bool kexec_free_initrd(void)
{
	return false;
}
#endif /* CONFIG_KEXEC_CORE */
/* crashkernel/initrd 重叠释放处理相关代码结束。 */

#ifdef CONFIG_BLK_DEV_RAM
static void __init populate_initrd_image(char *err)
{
	ssize_t written;
	struct file *file;
	loff_t pos = 0;

	/* 外部 rootfs 不是 initramfs 时，把它保存为 /initrd.image 供旧式 initrd 使用。 */
	printk(KERN_INFO "rootfs image is not initramfs (%s); looks like an initrd\n",
			err);
	file = filp_open("/initrd.image", O_WRONLY|O_CREAT|O_LARGEFILE, 0700);
	if (IS_ERR(file))
		return;

	written = xwrite(file, (char *)initrd_start, initrd_end - initrd_start,
			&pos);
	if (written != initrd_end - initrd_start)
		pr_err("/initrd.image: incomplete write (%zd != %ld)\n",
		       written, initrd_end - initrd_start);
	fput(file);
}
#endif /* CONFIG_BLK_DEV_RAM */
/* 旧式 initrd 镜像落盘支持相关代码结束。 */

static void __init do_populate_rootfs(void *unused, async_cookie_t cookie)
{
	/* Load the built in initramfs */
	/* 先加载链接进内核镜像的内建 initramfs。 */
	char *err = unpack_to_rootfs(__initramfs_start, __initramfs_size);
	/* 内建 initramfs 解压失败是致命错误，直接 panic。 */
	if (err)
		panic_show_mem("%s", err); /* Failed to decompress INTERNAL initramfs */

	if (!initrd_start || IS_ENABLED(CONFIG_INITRAMFS_FORCE))
		goto done;

	/* 再尝试解包 bootloader 提供的外部 initrd/initramfs。 */
	if (IS_ENABLED(CONFIG_BLK_DEV_RAM))
		printk(KERN_INFO "Trying to unpack rootfs image as initramfs...\n");
	else
		printk(KERN_INFO "Unpacking initramfs...\n");

	err = unpack_to_rootfs((char *)initrd_start, initrd_end - initrd_start);
	if (err) {
#ifdef CONFIG_BLK_DEV_RAM
		populate_initrd_image(err);
#else
		printk(KERN_EMERG "Initramfs unpacking failed: %s\n", err);
#endif
	}

done:
	/* 通知安全子系统 initramfs/rootfs 已经填充完成。 */
	security_initramfs_populated();

	/*
	 * If the initrd region is overlapped with crashkernel reserved region,
	 * free only memory that is not part of crashkernel region.
	 *
	 * 如果 initrd 与 crashkernel 保留区重叠，只释放不属于 crashkernel 的部分。
	 */
	if (!do_retain_initrd && initrd_start && !kexec_free_initrd()) {
		free_initrd_mem(initrd_start, initrd_end);
	} else if (do_retain_initrd && initrd_start) {
		/* 保留 initrd 时，通过 sysfs 暴露为 /sys/firmware/initrd。 */
		bin_attr_initrd.size = initrd_end - initrd_start;
		bin_attr_initrd.private = (void *)initrd_start;
		if (sysfs_create_bin_file(firmware_kobj, &bin_attr_initrd))
			pr_err("Failed to create initrd sysfs file");
	}
	initrd_start = 0;
	initrd_end = 0;

	/* 刷新 init 阶段延迟 fput 的文件引用。 */
	init_flush_fput();
}

static ASYNC_DOMAIN_EXCLUSIVE(initramfs_domain);
static async_cookie_t initramfs_cookie;

void wait_for_initramfs(void)
{
	if (!initramfs_cookie) {
		/*
		 * Something before rootfs_initcall wants to access
		 * the filesystem/initramfs. Probably a bug. Make a
		 * note, avoid deadlocking the machine, and let the
		 * caller's access fail as it used to.
		 *
		 * rootfs_initcall 之前就访问 filesystem/initramfs 通常是 bug。
		 * 这里只记录告警并返回，避免机器死锁，让调用者像过去一样失败。
		 */
		pr_warn_once("wait_for_initramfs() called before rootfs_initcalls\n");
		return;
	}
	/* 等待 do_populate_rootfs() 所在异步域完成。 */
	async_synchronize_cookie_domain(initramfs_cookie + 1, &initramfs_domain);
}
EXPORT_SYMBOL_GPL(wait_for_initramfs);

static int __init populate_rootfs(void)
{
	/* 默认异步填充 rootfs，减少启动路径上的同步等待。 */
	initramfs_cookie = async_schedule_domain(do_populate_rootfs, NULL,
						 &initramfs_domain);
	usermodehelper_enable();
	/* 用户禁用异步模式时，在 rootfs_initcall 阶段同步等待完成。 */
	if (!initramfs_async)
		wait_for_initramfs();
	return 0;
}
rootfs_initcall(populate_rootfs);
