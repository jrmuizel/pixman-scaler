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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include "pixman-private.h"
#include "pixman-rescale.h"

/*
When box filtering with an integer sized box we only ever have two box sizes.
Proof:
    Assume w1 > w2.
    r = w1/w2

    The size of the two boxes is
    r1 = ceil(r), r2 = floor(r)

    we want to find non-negative integers p and q such that:
    w1 = p*r1 + q*r2

    if r1 == r2 then
        r is an integer and thus w2 evenly divides w1
        therefor p = w2 and q = 0
    otherwise r1 != r2 and
        r1 = r2 + 1
        w1 = p*(r2 + 1) + q * r2
           = p*r2 + q*r2 + p
           = (p + q)*r2 + p

        we then choose a value of p such that:
            w1 = r2*w2 + p
            thus p = w1 mod w2
            and  q = w2 - p which is > 0 because
                            p = w1 mod w2

            subing into:
            (p + q)*r2 + p
            gives:
            w1 = (p + w2 - p)*r2 + p
            w1 = w2*r2 + w1 mod w2

*/
#define AVOID_DIVIDE

/* downsample_row_box_filter is a Digital Differential Analyzer (like Bresenham's line algorithm)
 * 'e' is the current fraction or error
 */
void downsample_row_box_filter(
		int n,
		uint32_t *src, uint32_t *dest,
                long dx2, long e, long src_dx)
{
    /* the filter sums only two different counts of samples.
     * we compute the inverses for these two different counts
     * so that we can do multiplications instead of divisions */
    int div_inv[2];
    int div = src_dx/dx2;
    /* we can index on the bottom bit of the divisor because the two different counts
     * are +1 */
    div_inv[div & 1] = (1<<24)/div;
    div += 1;
    div_inv[div & 1] = (1<<24)/div;

    while (n--) {
        uint32_t a, r, g, b;
        int div;
        int count = 1;

        /* we always have at least one source pixel per destination pixel
         * because we are downscaling */
        a = (*src >> 24) & 0xff;
        r = (*src >> 16) & 0xff;
        g = (*src >>  8) & 0xff;
        b = (*src >>  0) & 0xff;
        e -= dx2;
        src++;

        /* accumulate the rest of the source pixels */
        while (e > 0) {
            e -= dx2;
            a += (*src >> 24) & 0xff;
            r += (*src >> 16) & 0xff;
            g += (*src >>  8) & 0xff;
            b += (*src >>  0) & 0xff;
            count++;
            src++;
        }

#ifdef AVOID_DIVIDE
        div = div_inv[count & 1];

        a = (a * div + 0x10000) >> 24;
        r = (r * div + 0x10000) >> 24;
        g = (g * div + 0x10000) >> 24;
        b = (b * div + 0x10000) >> 24;
#else
        a /= count;
        r /= count;
        g /= count;
        b /= count;
#endif

        *dest = (a << 24) | (r << 16) | (g << 8) | b;
        dest++;

        e += src_dx;
    }
}

void downsample_columns_box_filter(
        int n,
        int columns,
        uint32_t *src, uint32_t *dest)
{
    int stride = n;
    int div = (1<<24)/columns;
    while (n--)
    {
        uint32_t a, r, g, b;
        uint32_t *column_src = src;
        int h;

        a = (*column_src >> 24) & 0xff;
        r = (*column_src >> 16) & 0xff;
        g = (*column_src >>  8) & 0xff;
        b = (*column_src >>  0) & 0xff;

        for (h = 1; h < columns; h++)
        {
            a += (*column_src >> 24) & 0xff;
            r += (*column_src >> 16) & 0xff;
            g += (*column_src >>  8) & 0xff;
            b += (*column_src >>  0) & 0xff;
            column_src += stride;
        }

#ifdef AVOID_DIVIDE
        a = (a * div + 0x10000) >> 24;
        r = (r * div + 0x10000) >> 24;
        g = (g * div + 0x10000) >> 24;
        b = (b * div + 0x10000) >> 24;
#else
        a /= columns;
        r /= columns;
        g /= columns ;
        b /= columns;
#endif
        *dest = (a << 24) | (r << 16) | (g << 8) | b;
        dest++;
        src++;
    }
}

#define ROUND

PIXMAN_EXPORT
int downscale_integer_box_filter (const struct rescale_fetcher *fetcher, void *closure, unsigned orig_width, unsigned orig_height,
		signed scaled_width, signed scaled_height,
		uint16_t start_column, uint16_t start_row,
		uint16_t width, uint16_t height,
		uint32_t *dest, int dst_stride)
{
    uint32_t *temp_buf;
    uint32_t *scanline;
    int src_offset = 0;
    int src_dy, e, dx2;
    int dest_dx, src_dx, ex, dx2x;
    int y = 0;
    int d;

    /* setup our parameters for runing our DDA in the y direction */
    /* multiplying by 2 is for rounding purposes. i.e. to move the center */
    src_dy = orig_height * 2; /* dy *= 2 */
    e = (orig_height*2) - scaled_height; /* e = dy*2 - dx */
    dx2 = scaled_height*2; /* dx2 = dx*2 */

    /* setup our parameters for doing DDA in the x direction */
    dest_dx = scaled_width;
    src_dx = orig_width;
    ex = (src_dx*2) - dest_dx;
    dx2x = dest_dx*2;
    src_dx *= 2;

    /* Compute the src_offset and associated error value:
     * There are two ways to do this: */
    /* 1. Directly
    int nsrc_offset = (src_dx*startColumn + dest_dx)/(dest_dx*2);
    long nex = (src_dx*startColumn + dest_dx)%(dest_dx*2) - dest_dx*2 + src_dx; */
    /* 2. Iteratively */
    for (d = 0; d < start_column; d++) {
        while (ex > 0) {
            src_offset++;
            ex -= dx2x;
        }
        ex += src_dx;
    }

    /* seek to the begining */
    for (d = 0; d < start_row; d++) {
        while (e > 0) {
            e -= dx2;
            y++;
        }
        e += src_dy;
    }

    /* we need to allocate enough room for ceil(src_height/dest_height) scanlines */
    temp_buf = pixman_malloc_abc ((orig_height + scaled_height-1)/scaled_height, scaled_width, sizeof(uint32_t));

    scanline = fetcher->init (closure);

    if (!scanline || !temp_buf) {
        return -1;
    }

    for (d = start_row; d < start_row + height; d++)
    {
        int columns = 0;
        while (e > 0)
        {
            fetcher->fetch_scanline (closure, y, &scanline);

            /* XXX: We could turn this multiplication into addition. It
             * probably doesn't matter though  */
            downsample_row_box_filter (width, scanline + src_offset, temp_buf + width * columns,
                                       dx2x, ex, src_dx);

            e -= dx2;
            columns++;
            y++;
        }

        downsample_columns_box_filter (width, columns, temp_buf, dest);

        dest += dst_stride/4;
        e += src_dy;
    }

    fetcher->fini (closure, scanline);
    free (temp_buf);

    return 0;
}

