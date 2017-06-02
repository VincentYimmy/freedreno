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

#include "test-util-2d.h"

#define DEFAULT_BLIT_MASK (C2D_SOURCE_RECT_BIT | C2D_TARGET_RECT_BIT |	\
			   C2D_NO_PIXEL_ALPHA_BIT | C2D_NO_BILINEAR_BIT | \
			   C2D_NO_ANTIALIASING_BIT | C2D_ALPHA_BLEND_NONE)

static const struct {
	const char *name;
	uint32_t fmt;
} formats[] = {
#define FMT(x) { #x, x }
		FMT(xRGB),
		FMT(ARGB),
		FMT(RGBx),
		FMT(RGBA),
		FMT(BGRx),
		FMT(BGRA),
		FMT(xBGR),
		FMT(ABGR),
		FMT(A8),
		//FMT(C2D_COLOR_FORMAT_444_RGB),
		FMT(C2D_COLOR_FORMAT_565_RGB),
		FMT(C2D_COLOR_FORMAT_888_RGB),
};

static const char *fmtname(uint32_t format)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(formats); i++)
		if (formats[i].fmt == (format & 0xff))
			return formats[i].name;
	return "????";
}

static void test_copy(uint32_t w, uint32_t h, uint32_t sformat, uint32_t dformat)
{
	PixmapPtr src, dest;
	C2D_OBJECT blit = {};
//	C2D_RECT rect;
	c2d_ts_handle curTimestamp;

	RD_START("ubwc2d", "%dx%d format:%s->%s", w, h,
			fmtname(sformat), fmtname(dformat));

	dest = create_pixmap(w, h, dformat);
	src  = create_pixmap(w, h, sformat | C2D_FORMAT_UBWC_COMPRESSED);

//	rect.x = 1;
//	rect.y = 2;
//	rect.width = w - 2;
//	rect.height = h - 3;
//	CHK(c2dFillSurface(dest->id, 0xff556677, &rect));
//
//	rect.x = 0;
//	rect.y = 0;
//	rect.width = 13;
//	rect.height = 17;
//	CHK(c2dFillSurface(src->id, 0xff223344, &rect));

	blit.surface_id = src->id;
	blit.config_mask = DEFAULT_BLIT_MASK;
	blit.next = NULL;

	blit.source_rect.x = FIXED(1);
	blit.source_rect.y = FIXED(2);
	blit.source_rect.width = FIXED(13-1);
	blit.source_rect.height = FIXED(17-2);

	blit.target_rect.x = FIXED((w - 13) / 2);
	blit.target_rect.y = FIXED((h - 17) / 2);
	blit.target_rect.width = FIXED(13);
	blit.target_rect.height = FIXED(17);
	CHK(c2dDraw(dest->id, 0, NULL, 0, 0, &blit, 1));
	CHK(c2dFlush(dest->id, &curTimestamp));
	CHK(c2dWaitTimestamp(curTimestamp));

	free_pixmap(src);
	free_pixmap(dest);

	RD_END();

//	dump_pixmap(dest, "copy-%04dx%04d-%08x.bmp", w, h, dformat);
}

int main(int argc, char **argv)
{
	int i, j;

	TEST_START();

//	for (i = 0; i < ARRAY_SIZE(formats); i++) {
//		for (j = 0; j < ARRAY_SIZE(formats); j++) {
//			TEST(test_copy(63, 67, formats[i].fmt, formats[j].fmt));
//		}
//	}

	TEST(test_copy(63, 65, xRGB, RGBx));
	TEST(test_copy(127, 260, xRGB, xRGB));
	TEST(test_copy(62, 66, ARGB, C2D_COLOR_FORMAT_565_RGB));
	TEST(test_copy(59, 69, C2D_COLOR_FORMAT_565_RGB, ARGB));

	TEST(test_copy(  32,   32, xRGB, xRGB));
	TEST(test_copy(  32,   64, xRGB, xRGB));
	TEST(test_copy(  32,  128, xRGB, xRGB));
	TEST(test_copy(  32,  256, xRGB, xRGB));
	TEST(test_copy(  32,  512, xRGB, xRGB));
	TEST(test_copy(  32, 1024, xRGB, xRGB));
	TEST(test_copy(  32,   32, xRGB, xRGB));
	TEST(test_copy(  64,   32, xRGB, xRGB));
	TEST(test_copy( 128,   32, xRGB, xRGB));
	TEST(test_copy( 256,   32, xRGB, xRGB));
	TEST(test_copy( 512,   32, xRGB, xRGB));
	TEST(test_copy(1024,   32, xRGB, xRGB));

	TEST_END();

	return 0;
}

