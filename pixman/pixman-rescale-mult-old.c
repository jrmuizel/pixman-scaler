/* -*- Mode: c; c-basic-offset: 4; tab-width: 8; indent-tabs-mode: t; -*- */
/*
 * Copyright © 2009 Mozilla Corporation
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Mozilla Corporation not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Mozilla Corporation makes no
 * representations about the suitability of this software for any purpose.  It
 * is provided "as is" without express or implied warranty.
 *
 * MOZILLA CORPORATION DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT
 * SHALL MOZILLA CORPORATION BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
 * OF THIS SOFTWARE.
 *
 * Author: Jeff Muizelaar, Mozilla Corp.
 */

#if 0

/* This implements a box filter that supports non-integer box sizes */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdint.h>
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include "pixman-private.h"
#include "pixman-rescale.h"
#define abs(a) (a) > 0 ? (a) : -(a)
#define sign(x) ((x)>0 ? 1:-1)

static void fetch_scanline(void *closure, int y, uint32_t *scanline)
{
    pixman_image_t *pict = closure;
    bits_image_t *bits = &pict->bits;
    if (y > pict->bits.height-1)
        y = pict->bits.height-1;
    if (!pixman_image_has_source_clipping(pict)) {
        fetchProc32 fetch = ACCESS(pixman_fetchProcForPicture32)(bits);
        fetch(bits, 0, y, pict->bits.width, scanline);
    } else {
        int x;
        fetchPixelProc32 fetch = ACCESS(pixman_fetchPixelProcForPicture32)(bits);
        for (x=0; x<bits->width; x++) {
            if (pixman_region32_contains_point (bits->common.src_clip, x, y,NULL))
                scanline[x] = fetch (bits, x, y);
            else
                scanline[x] = 0;
        }
    }
}

/* we work in fixed point where 1. == 1 << 24 */
#define FIXED_SHIFT 24

static void downsample_row_box_filter(
		int n,
		uint32_t *src, uint32_t *dest,
                int coverage[], int pixel_coverage)
{
    /* we need an array of the pixel contribution of each destination pixel on the boundaries.
     * we invert the value to get the value on the other size of the box */
    /*

       value  = a * contribution * 1/box_size
       value += a * 1/box_size
       value += a * 1/box_size
       value += a * 1/box_size
       value += a * (1 - contribution) * 1/box_size
                a * (1/box_size - contribution * 1/box_size)

        box size is constant


       value = a * contribtion_a * 1/box_size + b * contribution_b * 1/box_size
               contribution_b = (1 - contribution_a)
                              = (1 - contribution_a_next)
    */

    /* box size = ceil(src_width/dest_width) */

    int x = 0;
    while (n--) {
        uint32_t a = 0;
        uint32_t r = 0;
        uint32_t g = 0;
        uint32_t b = 0;
        int box = 1 << FIXED_SHIFT;
        int start_coverage = coverage[x];
        a = ((*src >> 24) & 0xff) * start_coverage;
        r = ((*src >> 16) & 0xff) * start_coverage;
        g = ((*src >>  8) & 0xff) * start_coverage;
        b = ((*src >>  0) & 0xff) * start_coverage;
        src++;
        x++;
        box -= start_coverage;
        while (box >= pixel_coverage) {
            a += ((*src >> 24) & 0xff) * pixel_coverage;
            r += ((*src >> 16) & 0xff) * pixel_coverage;
            g += ((*src >>  8) & 0xff) * pixel_coverage;
            b += ((*src >>  0) & 0xff) * pixel_coverage;
            src++;
            x++;

            box -= pixel_coverage;
        }
        /* multiply by whatever is leftover
         * this ensures that we don't bias down.
         * i.e. start_coverage + n*pixel_coverage + box == 1 << 24 */
        if (box > 0) {
            a += ((*src >> 24) & 0xff) * box;
            r += ((*src >> 16) & 0xff) * box;
            g += ((*src >>  8) & 0xff) * box;
            b += ((*src >>  0) & 0xff) * box;
        }

        a >>= FIXED_SHIFT;
        r >>= FIXED_SHIFT;
        g >>= FIXED_SHIFT;
        b >>= FIXED_SHIFT;

        *dest = (a << 24) | (r << 16) | (g << 8) | b;
        dest++;
    }
}

static void downsample_columns_box_filter(
        int n,
        int start_coverage,
        int pixel_coverage,
        uint32_t *src, uint32_t *dest)
{
    int stride = n;
    while (n--) {
        uint32_t a = 0;
        uint32_t r = 0;
        uint32_t g = 0;
        uint32_t b = 0;
        uint32_t *column_src = src;
        int box = 1 << FIXED_SHIFT;
        a = ((*column_src >> 24) & 0xff) * start_coverage;
        r = ((*column_src >> 16) & 0xff) * start_coverage;
        g = ((*column_src >>  8) & 0xff) * start_coverage;
        b = ((*column_src >>  0) & 0xff) * start_coverage;
        column_src += stride;
        box -= start_coverage;
        while (box >= pixel_coverage) {
            a += ((*column_src >> 24) & 0xff) * pixel_coverage;
            r += ((*column_src >> 16) & 0xff) * pixel_coverage;
            g += ((*column_src >>  8) & 0xff) * pixel_coverage;
            b += ((*column_src >>  0) & 0xff) * pixel_coverage;
            column_src += stride;
            box -= pixel_coverage;
        }
        if (box > 0) {
            a += ((*column_src >> 24) & 0xff) * box;
            r += ((*column_src >> 16) & 0xff) * box;
            g += ((*column_src >>  8) & 0xff) * box;
            b += ((*column_src >>  0) & 0xff) * box;
        }
        a >>= FIXED_SHIFT;
        r >>= FIXED_SHIFT;
        g >>= FIXED_SHIFT;
        b >>= FIXED_SHIFT;

        *dest = (a << 24) | (r << 16) | (g << 8) | b;
        dest++;
        src++;
    }
}

#if 1
int compute_coverage(int coverage[], int src_length, int dest_length) {
    int i;
    /* num = src_length/dest_length
       total = sum(pixel) / num

       pixel * 1/num == pixel * dest_length / src_length
    */
    int full = 1<<24;
    /* the average contribution of each source pixel */
    int ratio = ((1 << 24)*(long long int)dest_length)/src_length;
    int finished = ((1 << 24)*(long long int)dest_length) - (((1 << 24)*(long long int)dest_length)/src_length)*src_length;
    int p=0;
    int p2;
    int remainder = full;
    /* because ((1 << 24)*(long long int)dest_length) won't always be divisible by src_length
     * we'll need someplace to put the other bits.
     *
     * We want to ensure a + n*ratio < 1<<24
     *
     * 1<<24
     * */
    for (i=0; i<src_length; i++) {
        if (remainder < ratio) {
            p = ratio - remainder;
            remainder = full - p;
        } else {
            p = ratio;
            remainder -= ratio;
        }

        if ((((i+1)*ratio) % full) < ratio) {
            p2 = (((i+1)*ratio) % full);
        } else {
            p2 = ratio;
        }

        //printf("%d %d %d %d %d\n", i, p, remainder, (i+1)*ratio, p2);
        //assert(p == p2);

        coverage[i] = p;
    }
    //assert(remainder == 0);
    return ratio;
}
#endif


PIXMAN_EXPORT
int downscale_box_mult_old_filter(pixman_image_t *pict, unsigned origWidth, unsigned origHeight,
		signed scaledWidth, signed scaledHeight,
		uint16_t startColumn, uint16_t startRow,
		uint16_t width, uint16_t height,
		uint32_t *src, int srcStride,
		uint32_t *dest, int dstStride)
{
   // printf("%d %d %d %d\n", scaledWidth, scaledHeight, origWidth, origHeight);
        int yd1 = 0;
        int d;

        /* we need to allocate enough room for ceil(src_height/dest_height) scanlines */
        uint32_t *temp_buf = pixman_malloc_abc ((origHeight + scaledHeight-1)/scaledHeight, scaledWidth, sizeof(uint32_t));
	if (!temp_buf)
            return -1;

	//XXX: I suppose we should check whether this will succeed
        uint32_t *scanline = pixman_malloc_abc (origWidth, 1, sizeof(uint32_t));

        int *x_coverage = pixman_malloc_abc (origWidth, 1, sizeof(int));
        int *y_coverage = pixman_malloc_abc (origHeight, 1, sizeof(int));
        int pixel_coverage_x = compute_coverage(x_coverage, origWidth, scaledWidth);
        int pixel_coverage_y = compute_coverage(y_coverage, origHeight, scaledHeight);

        int y = 0;
        for (d = 0; d < startRow + height; d++)
	{
            int columns = 0;
            int box = 1 << FIXED_SHIFT;
            int start_coverage_y = y_coverage[y];
            fetch_scanline(pict, y, scanline);
            downsample_row_box_filter(width, scanline, temp_buf + width * columns,
                    x_coverage, pixel_coverage_x);
            columns++;
            y++;
            box -= start_coverage_y;

            while (box >= pixel_coverage_y) {
                fetch_scanline(pict, y, scanline);
                downsample_row_box_filter(width, scanline, temp_buf + width * columns,
                    x_coverage, pixel_coverage_x);
                columns++;
                y++;
                box -= pixel_coverage_y;
            }
            if (box > 0) {
                fetch_scanline(pict, y, scanline);
                downsample_row_box_filter(width, scanline, temp_buf + width * columns,
                    x_coverage, pixel_coverage_x);
                columns++;
            }
            downsample_columns_box_filter(width, start_coverage_y, pixel_coverage_y, temp_buf, dest + (yd1 - startRow)*dstStride/4);
            yd1++;
        }
        free(temp_buf);
        return 0;
}

#endif
