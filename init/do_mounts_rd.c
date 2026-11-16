// SPDX-License-Identifier: GPL-2.0
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/minix_fs.h>
#include <linux/ext2_fs.h>
#include <linux/romfs_fs.h>
#include <uapi/linux/cramfs_fs.h>
#include <linux/initrd.h>
#include <linux/string.h>
#include <linux/slab.h>

#include "do_mounts.h"
#include "../fs/squashfs/squashfs_fs.h"

#include <linux/decompress/generic.h>

static struct file *in_file, *out_file;
/* 读入镜像和写出 ramdisk 时使用的当前位置。 */
static loff_t in_pos, out_pos;

static int __init prompt_ramdisk(char *str)
{
        /* prompt_ramdisk= 已废弃，仅保留兼容提示。 */
	pr_warn("ignoring the deprecated prompt_ramdisk= option\n");
	return 1;
}
__setup("prompt_ramdisk=", prompt_ramdisk);

int __initdata rd_image_start;		/* starting block # of image */
/* ramdisk 镜像在输入设备中的起始块号。 */

static int __init ramdisk_start_setup(char *str)
{
        /* ramdisk_start= 指定从输入设备的哪个块开始查找镜像。 */
	rd_image_start = simple_strtol(str,NULL,0);
	return 1;
}
__setup("ramdisk_start=", ramdisk_start_setup);

static int __init crd_load(decompress_fn deco);

/*
 * This routine tries to find a RAM disk image to load, and returns the
 * number of blocks to read for a non-compressed image, 0 if the image
 * is a compressed image, and -1 if an image with the right magic
 * numbers could not be found.
 *
 * We currently check for the following magic numbers:
 *	minix
 *	ext2
 *	romfs
 *	cramfs
 *	squashfs
 *	gzip
 *	bzip2
 *	lzma
 *	xz
 *	lzo
 *	lz4
 *
 * 该函数尝试识别需要加载的 RAM disk 镜像。
 * 对未压缩文件系统镜像，返回需要读取的 KiB 数；对压缩镜像返回 0；
 * 如果没有找到可识别的 magic，则返回 -1。
 *
 * 当前会检查 minix、ext2、romfs、cramfs、squashfs 以及多种压缩格式。
 */
static int __init
identify_ramdisk_image(struct file *file, loff_t pos,
		decompress_fn *decompressor)
{
	const int size = 512;
	struct minix_super_block *minixsb;
	struct romfs_super_block *romfsb;
	struct cramfs_super *cramfsb;
	struct squashfs_super_block *squashfsb;
	int nblocks = -1;
	unsigned char *buf;
	const char *compress_name;
	unsigned long n;
	int start_block = rd_image_start;

	buf = kmalloc(size, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	minixsb = (struct minix_super_block *) buf;
	romfsb = (struct romfs_super_block *) buf;
	cramfsb = (struct cramfs_super *) buf;
	squashfsb = (struct squashfs_super_block *) buf;
	memset(buf, 0xe5, size);

	/*
	 * Read block 0 to test for compressed kernel
         *
         * 先读取起始块，用于检测压缩镜像或位于块 0 的文件系统 magic。
	 */
	pos = start_block * BLOCK_SIZE;
	kernel_read(file, buf, size, &pos);

        /* 如果识别到压缩格式，记录对应解压函数；缺少解压器时稍后会报错。 */
	*decompressor = decompress_method(buf, size, &compress_name);
	if (compress_name) {
		printk(KERN_NOTICE "RAMDISK: %s image found at block %d\n",
		       compress_name, start_block);
		if (!*decompressor)
			printk(KERN_EMERG
			       "RAMDISK: %s decompressor not configured!\n",
			       compress_name);
		nblocks = 0;
		goto done;
	}

	/* romfs is at block zero too */
        /* romfs 的超级块也位于起始块。 */
	if (romfsb->word0 == ROMSB_WORD0 &&
	    romfsb->word1 == ROMSB_WORD1) {
		printk(KERN_NOTICE
		       "RAMDISK: romfs filesystem found at block %d\n",
		       start_block);
		nblocks = (ntohl(romfsb->size)+BLOCK_SIZE-1)>>BLOCK_SIZE_BITS;
		goto done;
	}

	if (cramfsb->magic == CRAMFS_MAGIC) {
                /* 未填充的 cramfs 直接从起始块开始。 */
		printk(KERN_NOTICE
		       "RAMDISK: cramfs filesystem found at block %d\n",
		       start_block);
		nblocks = (cramfsb->size + BLOCK_SIZE - 1) >> BLOCK_SIZE_BITS;
		goto done;
	}

	/* squashfs is at block zero too */
        /* squashfs 的超级块也从起始块开始。 */
	if (le32_to_cpu(squashfsb->s_magic) == SQUASHFS_MAGIC) {
		printk(KERN_NOTICE
		       "RAMDISK: squashfs filesystem found at block %d\n",
		       start_block);
		nblocks = (le64_to_cpu(squashfsb->bytes_used) + BLOCK_SIZE - 1)
			 >> BLOCK_SIZE_BITS;
		goto done;
	}

	/*
	 * Read 512 bytes further to check if cramfs is padded
         *
         * 再向后读取 512 字节，用于识别前面带填充的 cramfs 镜像。
	 */
	pos = start_block * BLOCK_SIZE + 0x200;
	kernel_read(file, buf, size, &pos);

	if (cramfsb->magic == CRAMFS_MAGIC) {
		printk(KERN_NOTICE
		       "RAMDISK: cramfs filesystem found at block %d\n",
		       start_block);
		nblocks = (cramfsb->size + BLOCK_SIZE - 1) >> BLOCK_SIZE_BITS;
		goto done;
	}

	/*
	 * Read block 1 to test for minix and ext2 superblock
         *
         * minix 和 ext2 的超级块检查需要读取起始块之后的第 1 块。
	 */
	pos = (start_block + 1) * BLOCK_SIZE;
	kernel_read(file, buf, size, &pos);

	/* Try minix */
        /* 尝试识别 minix 文件系统，并换算出镜像大小。 */
	if (minixsb->s_magic == MINIX_SUPER_MAGIC ||
	    minixsb->s_magic == MINIX_SUPER_MAGIC2) {
		printk(KERN_NOTICE
		       "RAMDISK: Minix filesystem found at block %d\n",
		       start_block);
		nblocks = minixsb->s_nzones << minixsb->s_log_zone_size;
		goto done;
	}

	/* Try ext2 */
        /* 尝试识别 ext2 文件系统，ext2_image_size() 返回镜像 KiB 数。 */
	n = ext2_image_size(buf);
	if (n) {
		printk(KERN_NOTICE
		       "RAMDISK: ext2 filesystem found at block %d\n",
		       start_block);
		nblocks = n;
		goto done;
	}

	printk(KERN_NOTICE
	       "RAMDISK: Couldn't find valid RAM disk image starting at %d.\n",
	       start_block);

done:
	kfree(buf);
	return nblocks;
}

static unsigned long nr_blocks(struct file *file)
{
	struct inode *inode = file->f_mapping->host;

        /* 只有块设备才能通过 inode 大小换算容量，单位为 KiB。 */
	if (!S_ISBLK(inode->i_mode))
		return 0;
	return i_size_read(inode) >> 10;
}

int __init rd_load_image(char *from)
{
	int res = 0;
	unsigned long rd_blocks, devblocks;
	int nblocks, i;
	char *buf = NULL;
	unsigned short rotate = 0;
	decompress_fn decompressor = NULL;
#if !defined(CONFIG_S390)
	char rotator[4] = { '|' , '/' , '-' , '\\' };
#endif

        /* 输出端固定为 /dev/ram，输入端为 root 设备或 /initrd.image。 */
	out_file = filp_open("/dev/ram", O_RDWR, 0);
	if (IS_ERR(out_file))
		goto out;

	in_file = filp_open(from, O_RDONLY, 0);
	if (IS_ERR(in_file))
		goto noclose_input;

	in_pos = rd_image_start * BLOCK_SIZE;
	nblocks = identify_ramdisk_image(in_file, in_pos, &decompressor);
	if (nblocks < 0)
		goto done;

	if (nblocks == 0) {
                /* 压缩镜像通过解压回调直接写入 ramdisk。 */
		if (crd_load(decompressor) == 0)
			goto successful_load;
		goto done;
	}

	/*
	 * NOTE NOTE: nblocks is not actually blocks but
	 * the number of kibibytes of data to load into a ramdisk.
         *
         * 注意: nblocks 这里并不是块数，而是需要加载到 ramdisk 的 KiB 数。
	 */
	rd_blocks = nr_blocks(out_file);
	if (nblocks > rd_blocks) {
		printk("RAMDISK: image too big! (%dKiB/%ldKiB)\n",
		       nblocks, rd_blocks);
		goto done;
	}

	/*
	 * OK, time to copy in the data
         *
         * 镜像未压缩时，直接按 BLOCK_SIZE 从输入设备复制到 /dev/ram。
	 */
	if (strcmp(from, "/initrd.image") == 0)
		devblocks = nblocks;
	else
		devblocks = nr_blocks(in_file);

	if (devblocks == 0) {
		printk(KERN_ERR "RAMDISK: could not determine device size\n");
		goto done;
	}

	buf = kmalloc(BLOCK_SIZE, GFP_KERNEL);
	if (!buf) {
		printk(KERN_ERR "RAMDISK: could not allocate buffer\n");
		goto done;
	}

	printk(KERN_NOTICE "RAMDISK: Loading %dKiB [%ld disk%s] into ram disk... ",
		nblocks, ((nblocks-1)/devblocks)+1, nblocks>devblocks ? "s" : "");
	for (i = 0; i < nblocks; i++) {
		if (i && (i % devblocks == 0)) {
                        /* 传统多盘 ramdisk 镜像只加载第一盘，后续交给用户空间处理。 */
			pr_cont("done disk #1.\n");
			rotate = 0;
			fput(in_file);
			break;
		}
		kernel_read(in_file, buf, BLOCK_SIZE, &in_pos);
		kernel_write(out_file, buf, BLOCK_SIZE, &out_pos);
#if !defined(CONFIG_S390)
		if (!(i % 16)) {
                        /* 打印简单旋转进度指示，避免长时间复制时没有反馈。 */
			pr_cont("%c\b", rotator[rotate & 0x3]);
			rotate++;
		}
#endif
	}
	pr_cont("done.\n");

successful_load:
	res = 1;
done:
	fput(in_file);
noclose_input:
	fput(out_file);
out:
	kfree(buf);
	init_unlink("/dev/ram");
	return res;
}

int __init rd_load_disk(int n)
{
        /* 为真实 root 设备和目标 ramdisk 建立临时设备节点，再从 root 读入镜像。 */
	create_dev("/dev/root", ROOT_DEV);
	create_dev("/dev/ram", MKDEV(RAMDISK_MAJOR, n));
	return rd_load_image("/dev/root");
}

static int exit_code;
static int decompress_error;

static long __init compr_fill(void *buf, unsigned long len)
{
        /* 解压器输入回调: 从压缩镜像文件读取更多数据。 */
	long r = kernel_read(in_file, buf, len, &in_pos);
	if (r < 0)
		printk(KERN_ERR "RAMDISK: error while reading compressed data");
	else if (r == 0)
		printk(KERN_ERR "RAMDISK: EOF while reading compressed data");
	return r;
}

static long __init compr_flush(void *window, unsigned long outcnt)
{
        /* 解压器输出回调: 将解压后的数据写入 /dev/ram。 */
	long written = kernel_write(out_file, window, outcnt, &out_pos);
	if (written != outcnt) {
		if (decompress_error == 0)
			printk(KERN_ERR
			       "RAMDISK: incomplete write (%ld != %ld)\n",
			       written, outcnt);
		decompress_error = 1;
		return -1;
	}
	return outcnt;
}

static void __init error(char *x)
{
        /* 解压器错误回调: 打印错误并标记加载失败。 */
	printk(KERN_ERR "%s\n", x);
	exit_code = 1;
	decompress_error = 1;
}

static int __init crd_load(decompress_fn deco)
{
	int result;

	if (!deco) {
                /* 识别到了压缩格式但内核未配置对应解压器，这是不可恢复错误。 */
		pr_emerg("Invalid ramdisk decompression routine.  "
			 "Select appropriate config option.\n");
		panic("Could not decompress initial ramdisk image.");
	}

        /* 通过 compr_fill/compr_flush 桥接通用解压器和 ramdisk 读写路径。 */
	result = deco(NULL, 0, compr_fill, compr_flush, NULL, NULL, error);
	if (decompress_error)
		result = 1;
	return result;
}
