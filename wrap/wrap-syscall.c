/*
 * Copyright © 2012 Rob Clark <robclark@freedesktop.org>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/* bits and pieces borrowed from lima project.. concept is the same, wrap
 * various syscalls and log what happens
 */

#include "wrap.h"


struct device_info {
	const char *name;
	struct {
		const char *name;
	} ioctl_info[_IOC_NR(0xffffffff)];
};

#define IOCTL_INFO(n) \
		[_IOC_NR(n)] = { .name = #n }

static struct device_info kgsl_3d_info = {
		.name = "kgsl-3d",
		.ioctl_info = {
				IOCTL_INFO(IOCTL_KGSL_DEVICE_GETPROPERTY),
				IOCTL_INFO(IOCTL_KGSL_DEVICE_WAITTIMESTAMP),
				IOCTL_INFO(IOCTL_KGSL_RINGBUFFER_ISSUEIBCMDS),
				IOCTL_INFO(IOCTL_KGSL_CMDSTREAM_READTIMESTAMP),
				IOCTL_INFO(IOCTL_KGSL_CMDSTREAM_FREEMEMONTIMESTAMP),
				IOCTL_INFO(IOCTL_KGSL_DRAWCTXT_CREATE),
				IOCTL_INFO(IOCTL_KGSL_DRAWCTXT_DESTROY),
				IOCTL_INFO(IOCTL_KGSL_MAP_USER_MEM),
				IOCTL_INFO(IOCTL_KGSL_SHAREDMEM_FROM_PMEM),
				IOCTL_INFO(IOCTL_KGSL_SHAREDMEM_FREE),
				IOCTL_INFO(IOCTL_KGSL_SHAREDMEM_FROM_VMALLOC),
				IOCTL_INFO(IOCTL_KGSL_SHAREDMEM_FLUSH_CACHE),
				IOCTL_INFO(IOCTL_KGSL_GPUMEM_ALLOC),
				IOCTL_INFO(IOCTL_KGSL_CFF_SYNCMEM),
				IOCTL_INFO(IOCTL_KGSL_CFF_USER_EVENT),
				IOCTL_INFO(IOCTL_KGSL_TIMESTAMP_EVENT),
				/* kgsl-3d specific ioctls: */
				IOCTL_INFO(IOCTL_KGSL_DRAWCTXT_SET_BIN_BASE_OFFSET),
		},
};

// kgsl-2d => Z180 vector graphcis core.. not sure if it is interesting..
static struct device_info kgsl_2d_info = {
		.name = "kgsl-2d",
		.ioctl_info = {
				IOCTL_INFO(IOCTL_KGSL_DEVICE_GETPROPERTY),
				IOCTL_INFO(IOCTL_KGSL_DEVICE_WAITTIMESTAMP),
				IOCTL_INFO(IOCTL_KGSL_RINGBUFFER_ISSUEIBCMDS),
				IOCTL_INFO(IOCTL_KGSL_CMDSTREAM_READTIMESTAMP),
				IOCTL_INFO(IOCTL_KGSL_CMDSTREAM_FREEMEMONTIMESTAMP),
				IOCTL_INFO(IOCTL_KGSL_DRAWCTXT_CREATE),
				IOCTL_INFO(IOCTL_KGSL_DRAWCTXT_DESTROY),
				IOCTL_INFO(IOCTL_KGSL_MAP_USER_MEM),
				IOCTL_INFO(IOCTL_KGSL_SHAREDMEM_FROM_PMEM),
				IOCTL_INFO(IOCTL_KGSL_SHAREDMEM_FREE),
				IOCTL_INFO(IOCTL_KGSL_SHAREDMEM_FROM_VMALLOC),
				IOCTL_INFO(IOCTL_KGSL_SHAREDMEM_FLUSH_CACHE),
				IOCTL_INFO(IOCTL_KGSL_GPUMEM_ALLOC),
				IOCTL_INFO(IOCTL_KGSL_CFF_SYNCMEM),
				IOCTL_INFO(IOCTL_KGSL_CFF_USER_EVENT),
				IOCTL_INFO(IOCTL_KGSL_TIMESTAMP_EVENT),
				/* no kgsl-2d specific ioctls, I don't think.. */
		},
};

static struct device_info pmem_info = {
		.name = "pmem-gpu",
		.ioctl_info = {
				IOCTL_INFO(PMEM_GET_PHYS),
				IOCTL_INFO(PMEM_MAP),
				IOCTL_INFO(PMEM_GET_SIZE),
				IOCTL_INFO(PMEM_UNMAP),
				IOCTL_INFO(PMEM_ALLOCATE),
				IOCTL_INFO(PMEM_CONNECT),
				IOCTL_INFO(PMEM_GET_TOTAL_SIZE),
				IOCTL_INFO(HW3D_REVOKE_GPU),
				IOCTL_INFO(HW3D_GRANT_GPU),
				IOCTL_INFO(HW3D_WAIT_FOR_INTERRUPT),
				IOCTL_INFO(PMEM_CLEAN_INV_CACHES),
				IOCTL_INFO(PMEM_CLEAN_CACHES),
				IOCTL_INFO(PMEM_INV_CACHES),
				IOCTL_INFO(PMEM_GET_FREE_SPACE),
				IOCTL_INFO(PMEM_ALLOCATE_ALIGNED),
		},
};

static int kgsl_3d0 = -1, kgsl_2d0 = -1, kgsl_2d1 = -1, pmem_gpu0 = -1, pmem_gpu1 = -1;


static struct device_info * get_kgsl_info(int fd)
{
	if ((fd == kgsl_2d0) || (fd == kgsl_2d1))
		return &kgsl_2d_info;
	else if (fd == kgsl_3d0)
		return &kgsl_3d_info;
	return NULL;
}

void
hexdump(const void *data, int size)
{
	unsigned char *buf = (void *) data;
	char alpha[17];
	int i;

	for (i = 0; i < size; i++) {
		if (!(i % 16))
			printf("\t\t\t%08X", (unsigned int) buf + i);
		if (!(i % 4))
			printf(" ");

		if (((void *) (buf + i)) < ((void *) data)) {
			printf("   ");
			alpha[i % 16] = '.';
		} else {
			printf(" %02X", buf[i]);

			if (isprint(buf[i]) && (buf[i] < 0xA0))
				alpha[i % 16] = buf[i];
			else
				alpha[i % 16] = '.';
		}

		if ((i % 16) == 15) {
			alpha[16] = 0;
			printf("\t|%s|\n", alpha);
		}
	}

	if (i % 16) {
		for (i %= 16; i < 16; i++) {
			printf("   ");
			alpha[i] = '.';

			if (i == 15) {
				alpha[16] = 0;
				printf("\t|%s|\n", alpha);
			}
		}
	}
}


static void dump_ioctl(struct device_info *info, int dir, int fd,
		unsigned long int request, void *ptr, int ret)
{
	int nr = _IOC_NR(request);
	int sz = _IOC_SIZE(request);
	char c;
	const char *name;

	if (dir == _IOC_READ)
		c = '<';
	else
		c = '>';

	if (info->ioctl_info[nr].name)
		name = info->ioctl_info[nr].name;
	else
		name = "<unknown>";

	printf("%c [%4d] %8s: %s (%08lx)", c, fd, info->name, name, request);
	if (dir == _IOC_READ)
		printf(" => %d", ret);
	printf("\n");

	if (dir & _IOC_DIR(request))
		hexdump(ptr, sz);
}

static void dumpfile(const char *file)
{
	char buf[1024];
	int fd = open(file, 0);
	int n;

	while ((n = read(fd, buf, 1024)) > 0)
		write(1, buf, n);
	close(fd);
}

struct buffer {
	void *hostptr;
	unsigned int gpuaddr, flags, len;
	struct list node;
};

LIST_HEAD(buffers_of_interest);

static struct buffer * register_buffer(void *hostptr, unsigned int flags, unsigned int len)
{
	struct buffer *buf = calloc(1, sizeof *buf);
	buf->hostptr = hostptr;
	buf->flags = flags;
	buf->len = len;
	list_add(&buf->node, &buffers_of_interest);
	return buf;
}

static struct buffer * find_buffer(void *hostptr, unsigned int gpuaddr)
{
	struct buffer *buf;
	list_for_each_entry(buf, &buffers_of_interest, node) {
		if (hostptr)
			if ((buf->hostptr <= hostptr) && (hostptr < (buf->hostptr + buf->len)))
				return buf;
		if (gpuaddr)
			if ((buf->gpuaddr <= gpuaddr) && (gpuaddr < (buf->gpuaddr + buf->len)))
				return buf;
	}
	return NULL;
}

static void unregister_buffer(unsigned int gpuaddr)
{
	struct buffer *buf = find_buffer((void *)-1, gpuaddr);
	if (buf) {
		list_del(&buf->node);
		free(buf);
	}
}

static void dump_buffer(unsigned int gpuaddr)
{
	static int cnt = 0;
	struct buffer *buf = find_buffer((void *)-1, gpuaddr);
	if (buf) {
		char filename[32];
		int fd;
		sprintf(filename, "%04d-%08x.dat", cnt, buf->gpuaddr);
		printf("\t\tdumping: %s\n", filename);
		fd = open(filename, O_WRONLY| O_TRUNC | O_CREAT, 0644);
		write(fd, buf->hostptr, buf->len);
		close(fd);
		cnt++;
	}
}

void dump_all_buffers(void)
{
	struct buffer *buf;
	list_for_each_entry(buf, &buffers_of_interest, node)
		dump_buffer(buf->gpuaddr);
}
/*****************************************************************************/

int open(const char* path, int flags, ...)
{
	mode_t mode = 0;
	int ret;
	PROLOG(open);

	if (flags & O_CREAT) {
		va_list  args;

		va_start(args, flags);
		mode = (mode_t) va_arg(args, int);
		va_end(args);

		ret = orig_open(path, flags, mode);
	} else {
		ret = orig_open(path, flags);
	}

	if (ret != -1) {
		if (!strcmp(path, "/dev/kgsl-3d0")) {
			kgsl_3d0 = ret;
			printf("found kgsl_3d0: %d\n", kgsl_3d0);
		} else if (!strcmp(path, "/dev/kgsl-2d0")) {
			kgsl_2d0 = ret;
			printf("found kgsl_2d0: %d\n", kgsl_2d0);
		} else if (!strcmp(path, "/dev/kgsl-2d1")) {
			kgsl_2d1 = ret;
			printf("found kgsl_2d1: %d\n", kgsl_2d1);
		} else if (!strcmp(path, "/dev/pmem_gpu0")) {
			pmem_gpu0 = ret;
			printf("found pmem_gpu0: %d\n", pmem_gpu0);
		} else if (!strcmp(path, "/dev/pmem_gpu1")) {
			pmem_gpu1 = ret;
			printf("found pmem_gpu1: %d\n", pmem_gpu1);
		} else if (strstr(path, "/dev/")) {
			printf("#### missing device, path: %s: %d\n", path, ret);
		}
	}

	return ret;
}

static void kgsl_ioctl_ringbuffer_issueibcmds_pre(int fd,
		struct kgsl_ringbuffer_issueibcmds *param)
{
	int is2d = get_kgsl_info(fd) == &kgsl_2d_info;
	int i;
	struct kgsl_ibdesc *ibdesc;
	printf("\t\tdrawctxt_id:\t%08x\n", param->drawctxt_id);
	/*
For z180_cmdstream_issueibcmds():

#define KGSL_CONTEXT_SAVE_GMEM	1
#define KGSL_CONTEXT_NO_GMEM_ALLOC	2
#define KGSL_CONTEXT_SUBMIT_IB_LIST	4
#define KGSL_CONTEXT_CTX_SWITCH	8
#define KGSL_CONTEXT_PREAMBLE	16

#define Z180_STREAM_PACKET_CALL 0x7C000275   <-- seems to be always first 4 bytes..

if there isn't a context switch, skip the first PACKETSIZE_STATESTREAM words:

PACKETSIZE_STATE:
	#define NUMTEXUNITS             4
	#define TEXUNITREGCOUNT         25
	#define VG_REGCOUNT             0x39

	#define PACKETSIZE_BEGIN        3
	#define PACKETSIZE_G2DCOLOR     2
	#define PACKETSIZE_TEXUNIT      (TEXUNITREGCOUNT * 2)
	#define PACKETSIZE_REG          (VG_REGCOUNT * 2)
	#define PACKETSIZE_STATE        (PACKETSIZE_TEXUNIT * NUMTEXUNITS + \
					 PACKETSIZE_REG + PACKETSIZE_BEGIN + \
					 PACKETSIZE_G2DCOLOR)

		((25 * 2) * 4 + (0x39 * 2) + 3 + 2) =>
		((25 * 2) * 4 + (57 * 2) + 3 + 2) =>
		319

PACKETSIZE_STATESTREAM:
	#define x  (ALIGN((PACKETSIZE_STATE * \
					 sizeof(unsigned int)), 32) / \
					 sizeof(unsigned int))

	ALIGN((PACKETSIZE_STATE * sizeof(unsigned int)), 32) / sizeof(unsigned int) =>
	1280 / 4 =>
	320 => 0x140

so the context, restored on context switch, is the first: 320 (0x140) words
	*/
	printf("\t\tflags:\t\t%08x\n", param->flags);
	printf("\t\tnumibs:\t\t%08x\n", param->numibs);
	printf("\t\tibdesc_addr:\t%08x\n", param->ibdesc_addr);
	ibdesc = (struct kgsl_ibdesc *)param->ibdesc_addr;
	for (i = 0; i < param->numibs; i++) {
		// z180_cmdstream_issueibcmds or adreno_ringbuffer_issueibcmds
		printf("\t\tibdesc[%d].ctrl:\t\t%08x\n", i, ibdesc[i].ctrl);
		printf("\t\tibdesc[%d].sizedwords:\t%08x\n", i, ibdesc[i].sizedwords);
		printf("\t\tibdesc[%d].gpuaddr:\t%08x\n", i, ibdesc[i].gpuaddr);
		printf("\t\tibdesc[%d].hostptr:\t%p\n", i, ibdesc[i].hostptr);
		if (is2d) {
			if (ibdesc[i].sizedwords > PACKETSIZE_STATESTREAM) {
				unsigned int len, *ptr;
				/* note: kernel side seems to expect param->timestamp to
				 * contain same thing as ibdesc[0].hostptr ... this seems to
				 * be what actually gets read from on kernel side.  Maybe a
				 * legacy thing??
				 * Update: this seems to be needed so z180_cmdstream_issueibcmds()
				 * can patch up the cmdstream to jump back to the next ringbuffer
				 * entry.
				 */
				printf("\t\tcontext:\n");
				hexdump(ibdesc[i].hostptr,
						PACKETSIZE_STATESTREAM * sizeof(unsigned int));
				rd_write_section(RD_CONTEXT, ibdesc[i].hostptr,
						PACKETSIZE_STATESTREAM * sizeof(unsigned int));

				printf("\t\tcmd:\n");
				ptr = (unsigned int *)(ibdesc[i].hostptr +
						PACKETSIZE_STATESTREAM * sizeof(unsigned int));
				len = ptr[2] & 0xfff;
				/* 5 is length of first packet, 2 for the two 7f000000's */
				hexdump(ptr, (len + 5 + 2) * sizeof(unsigned int));
				rd_write_section(RD_CMDSTREAM, ptr,
						(len + 5 + 2) * sizeof(unsigned int));
				/* dump out full buffer in case I need to go back and check
				 * if I missed something..
				 */
				dump_buffer(ibdesc[i].gpuaddr);
			} else {
				printf("\t\tWARNING: INVALID CONTEXT!\n");
				hexdump(ibdesc[i].hostptr, ibdesc[i].sizedwords * sizeof(unsigned int));
			}
		} else {
			struct buffer *buf = find_buffer(NULL, ibdesc[i].gpuaddr);
			if (buf && buf->hostptr) {
				uint32_t *ptr = buf->hostptr + (ibdesc[i].gpuaddr - buf->gpuaddr);
				printf("\t\tcmd:\n");
				hexdump(ptr, ibdesc[i].sizedwords * sizeof(unsigned int));

				if (0 /* verbose */) {
					int j;
					/* loop thru the cmdstream and dump any other buffer that is
					 * pointed to in the cmdstream:
					 */
					for (j = 0; j < ibdesc[i].sizedwords; j++) {
						uint32_t dword = ptr[j];
						struct buffer *referenced_buf = find_buffer(NULL, dword);
						if (referenced_buf) {
							if (referenced_buf->hostptr) {
								uint32_t off = (dword - referenced_buf->gpuaddr) & ~3;
								uint32_t *referenced_ptr = referenced_buf->hostptr + off;
								uint32_t len = buf->len - off;

								/* it would be helpful to know the length.. simple
								 * shader program dumps are under 2k, so...
								 */
								printf("\t\treferenced buffer: at offset %d: %08x (start=%08x, len=%d)\n",
										j, dword, referenced_buf->gpuaddr, referenced_buf->len);
								if (len > 2048)
									len = 2048;
								hexdump(referenced_ptr, len);
							} else {
								printf("\t\tunmapped referenced buffer: at offset %d: %08x (start=%08x, len=%d)\n",
										j, dword, referenced_buf->gpuaddr, referenced_buf->len);
							}
						}
					}
					dump_all_buffers();
				}
			}
		}
	}
}

static void kgsl_ioctl_ringbuffer_issueibcmds_post(int fd,
		struct kgsl_ringbuffer_issueibcmds *param)
{
	printf("\t\ttimestamp:\t%08x\n", param->timestamp);
}

static void kgsl_ioctl_drawctxt_create_pre(int fd,
		struct kgsl_drawctxt_create *param)
{
	printf("\t\tflags:\t\t%08x\n", param->flags);
}

static void kgsl_ioctl_drawctxt_create_post(int fd,
		struct kgsl_drawctxt_create *param)
{
	printf("\t\tdrawctxt_id:\t%08x\n", param->drawctxt_id);
}

#define PROP_INFO(n) [n] = #n
static const char *propnames[] = {
		PROP_INFO(KGSL_PROP_DEVICE_INFO),
		PROP_INFO(KGSL_PROP_DEVICE_SHADOW),
		PROP_INFO(KGSL_PROP_DEVICE_POWER),
		PROP_INFO(KGSL_PROP_SHMEM),
		PROP_INFO(KGSL_PROP_SHMEM_APERTURES),
		PROP_INFO(KGSL_PROP_MMU_ENABLE),
		PROP_INFO(KGSL_PROP_INTERRUPT_WAITS),
		PROP_INFO(KGSL_PROP_VERSION),
		PROP_INFO(KGSL_PROP_GPU_RESET_STAT),
};

static void kgsl_ioctl_device_getproperty_post(int fd,
		struct kgsl_device_getproperty *param)
{
	printf("\t\ttype:\t\t%08x (%s)\n", param->type, propnames[param->type]);
	hexdump(param->value, param->sizebytes);
}

static int len_from_vma(unsigned int hostptr)
{
	long long addr, endaddr, offset, inode;
	FILE *f;
	int ret;

	// TODO: only for debug..
	if (0)
		dumpfile("/proc/self/maps");

	f = fopen("/proc/self/maps", "r");

	do {
		char c;
		ret = fscanf(f, "%llx-%llx", &addr, &endaddr);
		if (addr == hostptr)
			return endaddr - addr;
		/* find end of line.. we could do this more cleverly w/ glibc.. :-( */
		while (((ret = fscanf(f, "%c", &c)) > 0) && (c != '\n'));
	} while (ret > 0);
	return -1;
}

static void kgsl_ioctl_sharedmem_from_vmalloc_pre(int fd,
		struct kgsl_sharedmem_from_vmalloc *param)
{
	int len;

	/* just make gpuaddr == hostptr.. should make it easy to track */
	printf("\t\tflags:\t\t%08x\n", param->flags);
	printf("\t\thostptr:\t%08x\n", param->hostptr);
	if (param->gpuaddr) {
		len = param->gpuaddr;
	} else {
		/* note: if gpuaddr not specified, need to figure out length from
		 * vma.. that is nasty!
		 */
		len = len_from_vma(param->hostptr);

		/* for 2d/z180, all of the 0x5000 length buffers seem to be what
		 * will be passed back in IOCTL_KGSL_RINGBUFFER_ISSUEIBCMDS cmds
		 *
		 * Additional buffers that seem to be something other than RGB
		 * surfaces with lengths:
		 *
		 *   0x00001000
		 *   0x00009000
		 *   0x00081000
		 *   0x00001000
		 *   0x00009000
		 *   0x00081000
		 *
		 * These are created before the sequence of 6 0x5000 len bufs
		 * on the first c2dCreateSurface() call (which seems to be
		 * triggering some initialization).
		 *
		 * Possibly sets of two due to having both kgsl-2d0 and kgsl-2d1?
		 * But the 0x5000 buffers passed in to ISSUEIBCMDS are round-
		 * robin'd even though all ISSUEIBCMDS go to kgsl-2d1..
		 *
		 * gpu addresses seem to be consistent from run to run.. (at least
		 * as long as sequence of c2d calls doesn't change)  Currently:
		 *
		 *   00001000 - 6601a000-6601b000
		 *   00009000 - 6601c000-66025000
		 *   00081000 - 66026000-660a7000
		 *
		 *   00001000 - 660a8000-660a9000
		 *   00009000 - 660aa000-660b3000
		 *   00081000 - 660b4000-66135000
		 *
		 *   00005000 - 66136000-6613b000
		 *   00005000 - 6613c000-66141000
		 *   00005000 - 66142000-66147000
		 *   00005000 - 66148000-6614d000
		 *   00005000 - 6614e000-66153000
		 *   00005000 - 66154000-66159000
		 *
		 * Hmm, interesting that each mapping ends up with a page between..
		 *
		 * Last buffer allocated is the surface (64x64x4bpp -> 0x4000):
		 *   00004000 - 6615a000-6615e000
		 *
		 * Then for c2dReadSurface() another surface is created (no longer
		 * with extra page in between!?):
		 *   00004000 - 6615e000-66162000
		 *
		 *     4053C000  75 02 00 7C  00 00 00 00  05 00 05 00  29 01 00 7C    |u..|........)..||
		 *     4053C010 <00 A0 01 66> 2A 01 00 7C <00 C0 01 66> 2B 01 00 7C    |...f*..|...f+..||
		 *     4053C020 <00 60 02 66> 0F 01 00 7C  0A 00 00 00  08 01 00 7C    |.`.f...|.......||
		 *     4053C030  00 F0 03 00  09 01 00 7C  00 F0 03 00  00 01 00 7C    |.......|.......||
		 *     4053C040 <00 E0 15 66> 01 01 00 7C  08 70 00 00  10 01 00 7C    |...f...|.p.....||
		 *     4053C050  FF FF FF 00  D0 01 00 7C  00 00 00 00  D4 01 00 7C    |.......|.......||
		 *     4053C060  00 00 00 00  0C 01 00 7C  00 00 00 00  0E 01 00 7C    |.......|.......||
		 *     4053C070  02 00 00 00  0D 01 00 7C  04 04 00 00  0B 01 00 7C    |.......|.......||
		 *     4053C080  00 00 00 FF  0A 01 00 7C  00 00 00 FF  11 01 00 7C    |.......|.......||
		 *     4053C090  00 00 00 00  14 01 00 7C  00 00 00 00  15 01 00 7C    |.......|.......||
		 *     4053C0A0  00 00 00 00  16 01 00 7C  00 00 00 00  17 01 00 7C    |.......|.......||
		 *     4053C0B0  00 00 00 00  18 01 00 7C  00 00 00 00  19 01 00 7C    |.......|.......||
		 *     4053C0C0  00 00 00 00  1A 01 00 7C  00 00 00 00  1B 01 00 7C    |.......|.......||
		 *     4053C0D0  00 00 00 00  1C 01 00 7C  00 00 00 00  1D 01 00 7C    |.......|.......||
		 *     4053C0E0  00 00 00 00  1E 01 00 7C  00 00 00 00  1F 01 00 7C    |.......|.......||
		 *     4053C0F0  00 00 00 00  24 01 00 7C  00 00 00 00  25 01 00 7C    |....$..|....%..||
		 *     4053C100  00 00 00 00  27 01 00 7C  00 00 00 00  28 01 00 7C    |....'..|....(..||
		 *     4053C110  00 00 00 00  5E 01 00 7B  00 00 00 00  61 01 00 7B    |....^..{....a..{|
		 *     4053C120  00 00 00 00  65 01 00 7B  00 00 00 00  66 01 00 7B    |....e..{....f..{|
		 *     4053C130  00 00 00 00  6E 01 00 7B  00 00 00 00  6F 01 00 7C    |....n..{....o..||
		 *     4053C140  00 00 00 00  65 01 00 7B  00 00 00 00  54 01 00 7B    |....e..{....T..{|
		 *     4053C150  00 00 00 00  55 01 00 7B  00 00 00 00  53 01 00 7B    |....U..{....S..{|
		 *     4053C160  00 00 00 00  68 01 00 7B  00 00 00 00  60 01 00 7B    |....h..{....`..{|
		 *     4053C170  00 00 00 00  50 01 00 7B  00 00 00 00  56 01 00 7B    |....P..{....V..{|
		 *     4053C180  00 00 00 00  57 01 00 7B  00 00 00 00  58 01 00 7B    |....W..{....X..{|
		 *     4053C190  00 00 00 00  59 01 00 7B  00 00 00 00  52 01 00 7B    |....Y..{....R..{|
		 *     4053C1A0  00 00 00 00  51 01 00 7B  00 00 00 00  56 01 00 7B    |....Q..{....V..{|
		 *     4053C1B0  00 00 00 00  7F 01 00 7C  00 00 00 00  7F 01 00 7C    |.......|.......||
		 *     4053C1C0  00 00 00 00  7F 01 00 7C  00 00 00 00  7F 01 00 7C    |.......|.......||
		 *     4053C1D0  00 00 00 00  00 00 00 7F  00 00 00 7F  29 01 00 7C    |............)..||
		 *     4053C1E0 <00 80 0A 66> 2A 01 00 7C <00 A0 0A 66> 2B 01 00 7C    |...f*..|...f+..||
		 *     4053C1F0 <00 40 0B 66> E2 01 00 7C  00 00 00 00  E3 01 00 7C    |.@.f...|.......||
		 *     4053C200  00 00 00 00  E4 01 00 7C  00 00 00 00  E5 01 00 7C    |.......|.......||
		 *     4053C210  00 00 00 00  E6 01 00 7C  00 00 00 00  E7 01 00 7C    |.......|.......||
		 *     4053C220  00 00 00 00  C0 01 00 7C  00 00 00 00  C1 01 00 7C    |.......|.......||
		 *     4053C230  00 00 00 00  C2 01 00 7C  00 00 00 00  C3 01 00 7C    |.......|.......||
		 *     4053C240  00 00 00 00  C4 01 00 7C  00 00 00 00  C5 01 00 7C    |.......|.......||
		 *
		 * Some addresses seen in the state info (first 0x140 words of
		 * ISSUEIBCMDS):
		 *     00 A0 01 66 -> 6601a000  <-- 1st 0x1000 buffer
		 *     00 C0 01 66 -> 6601c000  <-- 1st 0x9000 buffer
		 *     00 60 02 66 -> 66026000  <-- 1st 0x81000 buffer
		 *     00 E0 15 66 -> 6615e000  <-- previous read surface (but already free'd at this point!)
		 *     00 80 0A 66 -> 660a8000  <-- 2nd 0x1000 buffer
		 *     00 A0 0A 66 -> 660aa000  <-- 2nd 0x9000 buffer
		 *     00 40 0B 66 -> 660b4000  <-- 2nd 0x81000 buffer
		 *
		 * From dumps of these memories before every ISSUEIBCMDS:
		 *     6601a000: - same contents across all dumps (all 00's and aa's)
		 *     6601c000: - same contents across all dumps (all 00's and aa's)
		 *     66026000: - same contents across all dumps (all 00's and aa's)
		 *     660a8000: - same contents across all dumps (all 00's and aa's)
		 *     660aa000: - same contents across all dumps (all 00's and aa's)
		 *     660b4000: - same contents across all dumps (all 00's and aa's)
		 *   (possibly these are just used for more advanced operations, or 3d only?)
		 *
		 */

		/* these buffer sizes are interesting for 2d.. not sure about 3d.. */
		switch(len) {
		case 0x5000:
//		case 0x1000:
//		case 0x9000:
//		case 0x81000:
			/* register buffer of interest */
			register_buffer((void *)param->hostptr, param->flags, len);
			break;
		}
	}
	printf("\t\tlen:\t\t%08x\n", len);
}

static void kgsl_ioctl_sharedmem_from_vmalloc_post(int fd,
		struct kgsl_sharedmem_from_vmalloc *param)
{
	struct buffer *buf = find_buffer((void *)param->hostptr, 0);
	uint32_t sect[2] = {
			param->gpuaddr, len_from_vma(param->hostptr)
	};
	if (buf)
		buf->gpuaddr = param->gpuaddr;
	printf("\t\tgpuaddr:\t%08x\n", param->gpuaddr);
	rd_write_section(RD_GPUADDR, sect, sizeof(sect));
}

static void kgsl_ioctl_sharedmem_free_pre(int fd,
		struct kgsl_sharedmem_free *param)
{
	printf("\t\tgpuaddr:\t%08x\n", param->gpuaddr);
	unregister_buffer(param->gpuaddr);
}

static void kgsl_ioctl_gpumem_alloc_pre(int fd,
		struct kgsl_gpumem_alloc *param)
{
	printf("\t\tflags:\t\t%08x\n", param->flags);
	printf("\t\tsize:\t\t%08x\n", param->size);
}

static void kgsl_ioctl_gpumem_alloc_post(int fd,
		struct kgsl_gpumem_alloc *param)
{
	struct buffer *buf;
	uint32_t sect[2] = {
			param->gpuaddr, param->size
	};
	rd_write_section(RD_GPUADDR, sect, sizeof(sect));
	printf("\t\tgpuaddr:\t%08lx\n", param->gpuaddr);
	/* NOTE: host addr comes from mmap'ing w/ gpuaddr as offset */
	buf = register_buffer(NULL, param->flags, param->size);
	buf->gpuaddr = param->gpuaddr;
}

static void kgsl_ioctl_pre(int fd, unsigned long int request, void *ptr)
{
	dump_ioctl(get_kgsl_info(fd), _IOC_WRITE, fd, request, ptr, 0);
	switch(_IOC_NR(request)) {
	case _IOC_NR(IOCTL_KGSL_RINGBUFFER_ISSUEIBCMDS):
		kgsl_ioctl_ringbuffer_issueibcmds_pre(fd, ptr);
		break;
	case _IOC_NR(IOCTL_KGSL_DRAWCTXT_CREATE):
		kgsl_ioctl_drawctxt_create_pre(fd, ptr);
		break;
	case _IOC_NR(IOCTL_KGSL_SHAREDMEM_FROM_VMALLOC):
		kgsl_ioctl_sharedmem_from_vmalloc_pre(fd, ptr);
		break;
	case _IOC_NR(IOCTL_KGSL_SHAREDMEM_FREE):
		kgsl_ioctl_sharedmem_free_pre(fd, ptr);
		break;
	case _IOC_NR(IOCTL_KGSL_GPUMEM_ALLOC):
		kgsl_ioctl_gpumem_alloc_pre(fd, ptr);
		break;
	}
}

static void kgsl_ioctl_post(int fd, unsigned long int request, void *ptr, int ret)
{
	dump_ioctl(get_kgsl_info(fd), _IOC_READ, fd, request, ptr, ret);
	switch(_IOC_NR(request)) {
	case _IOC_NR(IOCTL_KGSL_RINGBUFFER_ISSUEIBCMDS):
		kgsl_ioctl_ringbuffer_issueibcmds_post(fd, ptr);
		break;
	case _IOC_NR(IOCTL_KGSL_DRAWCTXT_CREATE):
		kgsl_ioctl_drawctxt_create_post(fd, ptr);
		break;
	case _IOC_NR(IOCTL_KGSL_DEVICE_GETPROPERTY):
		kgsl_ioctl_device_getproperty_post(fd, ptr);
		break;
	case _IOC_NR(IOCTL_KGSL_SHAREDMEM_FROM_VMALLOC):
		kgsl_ioctl_sharedmem_from_vmalloc_post(fd, ptr);
		break;
	case _IOC_NR(IOCTL_KGSL_GPUMEM_ALLOC):
		kgsl_ioctl_gpumem_alloc_post(fd, ptr);
		break;
	}
}

static void pmem_ioctl_pre(int fd, unsigned long int request, void *ptr)
{
	dump_ioctl(&pmem_info, _IOC_WRITE, fd, request, ptr, 0);
}

static void pmem_ioctl_post(int fd, unsigned long int request, void *ptr, int ret)
{
	dump_ioctl(&pmem_info, _IOC_READ, fd, request, ptr, ret);
}

int ioctl(int fd, unsigned long int request, ...)
{
	int ioc_size = _IOC_SIZE(request);
	int ret;
	PROLOG(ioctl);
	void *ptr;

	// XXX fbdev doesn't appear to play by the rules:
	ioc_size = 1;

	if (ioc_size) {
		va_list args;

		va_start(args, request);
		ptr = va_arg(args, void *);
		va_end(args);
	} else {
		ptr = NULL;
	}

	if (get_kgsl_info(fd))
		kgsl_ioctl_pre(fd, request, ptr);
	else if ((fd == pmem_gpu0) || (fd == pmem_gpu1))
		pmem_ioctl_pre(fd, request, ptr);
	else
		printf("> [%4d]         : <unknown> (%08lx)\n", fd, request);

	ret = orig_ioctl(fd, request, ptr);

	if (get_kgsl_info(fd))
		kgsl_ioctl_post(fd, request, ptr, ret);
	else if ((fd == pmem_gpu0) || (fd == pmem_gpu1))
		pmem_ioctl_post(fd, request, ptr, ret);
	else
		printf("< [%4d]         : <unknown> (%08lx) (%d)\n", fd, request, ret);

	return ret;
}

void * mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset)
{
	void *ret;
	PROLOG(mmap);

	if (get_kgsl_info(fd)) {
		printf("< [%4d]         : mmap: addr=%p, length=%d, prot=%x, flags=%x, offset=%08lx\n",
				fd, addr, length, prot, flags, offset);
	}

	ret = orig_mmap(addr, length, prot, flags, fd, offset);

	if (get_kgsl_info(fd)) {
		struct buffer *buf = find_buffer(NULL, offset);
		if (buf)
			buf->hostptr = ret;
		printf("< [%4d]         : mmap: -> (%p)\n", fd, ret);
	}

	return ret;
}
