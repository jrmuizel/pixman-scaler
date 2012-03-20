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
 *
 * Portions derived from Chromium:
 *
 * Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *    * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *    * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *    * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdint.h>
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <float.h>
#include <math.h>
#include "pixman-private.h"
#include "pixman-rescale.h"


#define FILTER_SHIFT 14
#define LANCZOS_LOBES 2

/* contains the filter for each destination pixel */
struct filter {
    int count; /* filter size */
    int16_t *values; /* filter coefficients */
    int offset; /* filter start offset */
};

static int clamp(int a)
{
    if (a > 255)
        return 255;
    if (a < 0)
        return 0;
    return a;
}

/* from ffmpeg */
static inline uint8_t clip_uint8(int a)
{
    if (a&(~255)) /* check if any of the high bits are set */
        return (-a)>>31; /* clip to either 0 or 255 */
    else
        return a;
}

#if 0
#include <pmmintrin.h>
#include <emmintrin.h>
static void
downsample_row_convolve_sse2(int n,
		uint32_t *src, uint32_t *dest,
                const struct filter *filter, int src_size)
{
    int pixel = 0;
    while (n--) {
        int i;
        __m128i accum = _mm_setzero_si128 ();
        __m128i zero  = _mm_setzero_si128 ();
        for (i=0; i<filter[pixel].count; i++) {
            __m128i v_src = _mm_cvtsi32_si128 (src[filter[pixel].offset + i]);
            v_src = _mm_unpacklo_epi16 (_mm_unpacklo_epi8 (v_src, zero), zero);

            __m128i coeff = _mm_cvtsi32_si128 (filter[pixel].values[i]);
            /* duplicate the filter coefficient */
            coeff = _mm_shuffle_epi32 (coeff, _MM_SHUFFLE (0, 0, 0, 0));

            /* multiply and accumulate the result:
             * 0000vvvv_0000vvvv_0000vvvv_0000vvvv * 000000aa_000000rr_000000gg_000000bb */
            __m128i result = _mm_madd_epi16 (v_src, coeff);
            accum = _mm_add_epi32 (accum, result);
        }

        /* scale the accumulator down */
        accum = _mm_srai_epi32 (accum, FILTER_SHIFT);

        /* pack 000000aa_000000rr_000000gg_000000bb -> 00000000_00000000_00000000_aarrggbb */
        accum = _mm_packs_epi32 (accum, accum);

        //XXX: this should be need to saturate properly but doesn't seem to make a difference
        accum = _mm_max_epi16 (accum, zero);

        accum = _mm_packus_epi16 (accum, accum);

        *dest = _mm_cvtsi128_si32 (accum);
        dest++;
        pixel++;
    }
}
#endif

static void
downsample_row_convolve(int n,
		uint32_t *src, uint32_t *dest,
                struct filter *filter, int src_size)
{
    int pixel = 0;
    while (n--)
    {
        int32_t a = 0;
        int32_t r = 0;
        int32_t g = 0;
        int32_t b = 0;
        int i;

        for (i=0; i<filter[pixel].count; i++) {
            a += ((src[filter[pixel].offset + i] >> 24) & 0xff) * filter[pixel].values[i];
            r += ((src[filter[pixel].offset + i] >> 16) & 0xff) * filter[pixel].values[i];
            g += ((src[filter[pixel].offset + i] >>  8) & 0xff) * filter[pixel].values[i];
            b += ((src[filter[pixel].offset + i] >>  0) & 0xff) * filter[pixel].values[i];
        }

        a >>= FILTER_SHIFT;
        r >>= FILTER_SHIFT;
        g >>= FILTER_SHIFT;
        b >>= FILTER_SHIFT;

        a = clamp (a);
        r = clamp (r);
        g = clamp (g);
        b = clamp (b);

        *dest = (a << 24) | (r << 16) | (g << 8) | b;
        dest++;
        pixel++;
    }
}

/* src1 and src2 are two contiguous regions of samples. We need two to handle
 * the ring buffer that is used by the caller
 *
 * instead of src1, src1_length, src2, filter_length we could pass:
 * ring_buf, start_index, ring_buf_length, filter_length,
 * this is implicitly coded in the parameters we do pass:
 * src1 = ring_buf + start_index
 * src1_length = min(ring_buf_length - start_index, filter_length)
 * src2 = ring_buf */
static void
downsample_columns_convolve(int n,
        uint32_t *src1, int src1_length,
        uint32_t *src2,
        uint32_t *dest,
        int16_t *filter, int filter_length)
{
    int stride = n;
    while (n--)
    {
        int32_t a = 0;
        int32_t r = 0;
        int32_t g = 0;
        int32_t b = 0;
        int i;
        uint32_t *column_src;

        /* we do the accumulation in two steps because the source lines are in a ring buffer
         * so won't be contiguous */
        column_src = src1;
        for (i=0; i<src1_length; i++) /* loop till the end of the ring buffer */
        {
            a += ((*column_src >> 24) & 0xff) * filter[i];
            r += ((*column_src >> 16) & 0xff) * filter[i];
            g += ((*column_src >>  8) & 0xff) * filter[i];
            b += ((*column_src >>  0) & 0xff) * filter[i];
            column_src += stride;
        }

        /* accumulate the remaining samples starting at the begining of the ring buffer */
        column_src = src2;
        for (; i<filter_length; i++)
        {
            a += ((*column_src >> 24) & 0xff) * filter[i];
            r += ((*column_src >> 16) & 0xff) * filter[i];
            g += ((*column_src >>  8) & 0xff) * filter[i];
            b += ((*column_src >>  0) & 0xff) * filter[i];
            column_src += stride;
        }

        a >>= FILTER_SHIFT;
        r >>= FILTER_SHIFT;
        g >>= FILTER_SHIFT;
        b >>= FILTER_SHIFT;

        a = clamp (a);
        r = clamp (r);
        g = clamp (g);
        b = clamp (b);

        *dest = (a << 24) | (r << 16) | (g << 8) | b;
        dest++;
        src1++;
        src2++;
    }
}


/* Evaluates the Lanczos filter of the given filter size window for the given
 * position.
 *
 * |filter_size| is the width of the filter (the "window"), outside of which
 * the value of the function is 0. Inside of the window, the value is the
 * normalized sinc function:
 *   lanczos(x) = sinc(x) * sinc(x / filter_size);
 * where
 *   sinc(x) = sin(pi*x) / (pi*x);
 */
float eval_lanczos(int filter_size, float x)
{
  if (x <= -filter_size || x >= filter_size)
    return 0.0f;  /* Outside of the window. */
  if (x > -FLT_EPSILON &&
      x < FLT_EPSILON)
    return 1.0f;  /* Special case the discontinuity at the origin. */
  else {
      float xpi = x * (M_PI);
      return (sin(xpi) / xpi) *  /* sinc(x) */
          sin(xpi / filter_size) / (xpi / filter_size);  /* sinc(x/filter_size) */
  }
}

/* dealing with the edges:
   some options:
   we could always have approximately the same number of samples in the filter and just pad the image out
   chromium seems to truncate the filter though...
   I don't really have a good reason to choose either approach...
   one way to get an idea is to see what other implementation do.
   - it looks like quartz pads
   - chromium truncates the filter
   - opera pads
*/

int floor_int(float a)
{
    return floor(a);
}

int ceil_int(float a)
{
    return ceil(a);
}

int float_to_fixed(float a)
{
    return a * (1 << FILTER_SHIFT);
}

/* this method does not do source clipping
 * but will do dest clipping */
struct filter *compute_lanczos_filter(int dest_subset_lo, int dest_subset_size, int src_size, float scale)
{
    /* this is half the number of pixels that the filter uses in filter space */
    int dest_support = LANCZOS_LOBES;
    float src_support = dest_support / scale;

    /* we need to compute a set of filters for each pixel */
    /* filter width */
    int i;
    struct filter *filter = malloc (dest_subset_size * sizeof(struct filter));
    int max_filter_size = ceil_int (src_support * 2) - floor_int (src_support * -2) + 1;
    float *filter_values = malloc (max_filter_size * sizeof(float)); /* this is a good candidate for stack allocation */
    int dest_subset_hi = dest_subset_lo + dest_subset_size; /* [lo, hi) */


    /* When we're doing a magnification, the scale will be larger than one. This
     * means the destination pixels are much smaller than the source pixels, and
     * that the range covered by the filter won't necessarily cover any source
     * pixel boundaries. Therefore, we use these clamped values (max of 1) for
     * some computations.
     */
    float clamped_scale = MIN (1., scale);

    /* Speed up the divisions below by turning them into multiplies. */
    float inv_scale = 1. / scale;

    /* Loop over all pixels in the output range. We will generate one set of
     * filter values for each one. Those values will tell us how to blend the
     * source pixels to compute the destination pixel.
     */
    int dest_subset_i;
    int pixel = 0;
    for (dest_subset_i = dest_subset_lo; dest_subset_i < dest_subset_hi; dest_subset_i++, pixel++) {

        float src_pixel = dest_subset_i * inv_scale;

        int src_begin = MAX (0, floor_int (src_pixel - src_support));
        int src_end = MIN (src_size - 1, ceil_int (src_pixel + src_support));

        /* Compute the unnormalized filter value at each location of the source
         * it covers. */
        float filter_sum = 0.; /* sum of the filter values for normalizing */
        int filter_size = 0;
        int cur_filter_pixel;
        int j = 0;

        assert(src_begin >= 0);

        for (cur_filter_pixel = src_begin; cur_filter_pixel <= src_end;
                cur_filter_pixel++)
        {
            /* Distance from the center of the filter, this is the filter coordinate
             * in source space. */
            float src_filter_pos = cur_filter_pixel - src_pixel;

            /* Since the filter really exists in dest space, map it there. */
            float dest_filter_pos = src_filter_pos * clamped_scale;

            /* Compute the filter value at that location. */
            float filter_value = eval_lanczos(LANCZOS_LOBES, dest_filter_pos);
            filter_sum += filter_value;
            filter_values[j] = filter_value;

            filter_size++;
            j++;
        }

        assert(src_end >= src_begin);
        assert(filter_size <= max_filter_size);

        {
            int16_t leftovers;
            /* XXX: eventually we should avoid doing malloc here and just malloc a big
             * chunk of max_filter_size * sizeof(int16_t) */
            int16_t *fixed_filter_values = malloc (filter_size * sizeof(int16_t));

            /* the filter must be normalized so that we don't affect the brightness of
             * the image. Convert to normalized fixed point */
            /* XXX: It might be better if we didn't have to do this in a separate pass */
            int16_t fixed_sum = 0; /* XXX: should we use a regular int here? */
            for (i=0; i<filter_size; i++)
            {
                int16_t cur_fixed = float_to_fixed(filter_values[i] / filter_sum);
                fixed_sum += cur_fixed;
                fixed_filter_values[i] = cur_fixed;
            }

            /* The conversion to fixed point will leave some rounding errors, which
             * we add back in to avoid affecting the brightness of the image. We
             * arbitrarily add this to the center of the filter array (this won't always
             * be the center of the filter function since it could get clipped on the
             * edges, but it doesn't matter enough to worry about that case). */
            leftovers = float_to_fixed(1.0f) - fixed_sum;
            fixed_filter_values[filter_size / 2] += leftovers;

            filter[pixel].values = fixed_filter_values;
            filter[pixel].count = filter_size;
            filter[pixel].offset = src_begin;
            assert (filter[pixel].offset >= 0);
            assert (filter[pixel].offset + filter[pixel].count - 1 < src_size);
        }
    }

    free (filter_values);
    return filter;
}

/* start_column and start_row are in destination space */
PIXMAN_EXPORT
int downscale_lanczos_filter(const struct rescale_fetcher *fetcher, void *closure, unsigned orig_width, unsigned orig_height,
		signed scaled_width, signed scaled_height,
		uint16_t start_column, uint16_t start_row,
		uint16_t width, uint16_t height,
		uint32_t *dest, int dst_stride)
{
    struct filter *x_filter, *y_filter;
    int filter_index;
    int src_index = 0;
    int i;

    /* XXX: this duplicates code in compute_lanczos_filter */
    int dest_support = LANCZOS_LOBES;
    float src_support = dest_support / ((float)scaled_height/orig_height);

    int max_filter_size = ceil_int (src_support * 2) - floor_int (src_support * -2) + 1;

    /* we use a ring buffer to store the downsampled rows because we want to
     * reuse the downsampled rows across different destination pixels */
    int ring_buf_size = max_filter_size;
    uint32_t *ring_buf = pixman_malloc_abc (ring_buf_size, scaled_width, sizeof(uint32_t));

    uint32_t *scanline = fetcher->init (closure);
    void (*fetch_scanline)(void *closure, int y, uint32_t **scanline) = fetcher->fetch_scanline;

    assert (start_column + width  <= scaled_width);
    assert (start_row    + height <= scaled_height);

    x_filter = compute_lanczos_filter (start_column, width,  orig_width,  (float)scaled_width/orig_width);
    y_filter = compute_lanczos_filter (start_row,    height, orig_height, (float)scaled_height/orig_height);

    if (!ring_buf || !scanline || !x_filter || !y_filter)
        return -1;

    for (filter_index = 0; filter_index < height; filter_index++)
    {
        int filter_size   = y_filter[filter_index].count;
        int filter_offset = y_filter[filter_index].offset;
        int src1_index, src1_size;
        uint32_t *src1;
        assert(filter_offset >= 0);

        /* read and downsample the rows needed to downsample the next column */
        while (src_index < filter_offset + filter_size)
        {
            fetch_scanline (closure, src_index, &scanline);
            /* downsample_row_convolve_sse2 (width, scanline, ring_buf + width * (src_index % ring_buf_size), x_filter, orig_width); */
            downsample_row_convolve(width, scanline, ring_buf + width * (src_index % ring_buf_size), x_filter, orig_width);
            src_index++;
        }

        src1_index = filter_offset % ring_buf_size;
        assert (src1_index >= 0);
        assert (filter_size <= ring_buf_size);
        src1 = ring_buf + width * src1_index;

        src1_size = MIN (ring_buf_size - src1_index, filter_size);
        assert (src1_size >= 0);
        assert (filter_size >= src1_size);

        downsample_columns_convolve (width, src1, src1_size, ring_buf, dest, y_filter[filter_index].values, y_filter[filter_index].count);
        dest += dst_stride/4;
    }

    free (ring_buf);
    fetcher->fini (closure, scanline);

    for (i=0; i<width; i++)
        free (x_filter[i].values);
    for (i=0; i<height; i++)
        free (y_filter[i].values);
    free (x_filter);
    free (y_filter);

    return 0;
}
