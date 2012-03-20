#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>

#include "pixman-private.h"
#include "pixman-rescale.h"

/* Fetcher documentation:
 * Goals: we need a generic interface to fetch lines of ARGB32
 * - we want to support converting to argb32
 * - we also want to support having the rescaler read directly from the buffer
 *   if the buffer is of a format compatible with the rescaler */

static void fetch_convert(void *closure, int y, uint32_t **scanline_ptr)
{
    pixman_image_t *pict = closure;
    uint32_t *scanline = *scanline_ptr;
    pict->bits.fetch_scanline_raw_32(pict, 0, y, pict->bits.width, scanline, NULL, 0);

}

static uint32_t *fetch_convert_init(void *closure)
{
    pixman_image_t *pict = closure;
    return pixman_malloc_abc (pict->bits.width, 1, sizeof(uint32_t));
}

static void fetch_convert_fini(void *closure, uint32_t *scanline)
{
    free(scanline);
}


static void fetch_raw(void *closure, int y, uint32_t **scanline)
{
    pixman_image_t *pict = closure;
    bits_image_t *bits = &pict->bits;
    /* set the scanline to the begining of the yth row */
    *scanline = bits->bits + y*bits->rowstride;
}

static uint32_t *fetch_raw_init(void *closure)
{
    return closure;
}

static void fetch_raw_fini(void *closure, uint32_t *scanline)
{
}

const struct rescale_fetcher fetcher_convert = {
    fetch_convert,
    fetch_convert_init,
    fetch_convert_fini
};

const struct rescale_fetcher fetcher_raw = {
    fetch_raw,
    fetch_raw_init,
    fetch_raw_fini
};

/* downscale an pixman_image_t into an ARGB32 buffer
 *
 * scaled_width / original_width is the scale ratio
 * using scaled_width instead of just passing in the ratio avoids any rounding errors because
 * the ratio can be specified exactly
 *
 * start_column, start_row, width, height
 * specify a rectangle in the scaled source space
 *
 * dest, dst_stride, width, and height allow specifying
 * a rectangle in the destination buffer
 */
PIXMAN_EXPORT
int downscale_image(pixman_image_t *pict, enum downscale_type type,
		int scaled_width, int scaled_height,
		int start_column, int start_row,
		int width, int height,
		uint32_t *dest, int dst_stride)
{
    const struct rescale_fetcher *fetcher;
    int orig_height = pict->bits.height;
    int orig_width  = pict->bits.width;

    /* choose a fetcher */
    if (PIXMAN_FORMAT_BPP(pict->bits.format) == 32 &&
            PIXMAN_FORMAT_R(pict->bits.format) == 8 &&
            PIXMAN_FORMAT_G(pict->bits.format) == 8 &&
            PIXMAN_FORMAT_B(pict->bits.format) == 8)
    {
        fetcher = &fetcher_raw;
    } else
    {
        fetcher = &fetcher_convert;
    }

    /* choose and run downscaler */
    switch (type)
    {
        case LANCZOS:
            return downscale_lanczos_filter(fetcher, pict, orig_width, orig_height,
                scaled_width, scaled_height,
                start_column, start_row,
                width, height,
                dest, dst_stride);
        case BOX:
            return downscale_box_filter(fetcher, pict, orig_width, orig_height,
                scaled_width, scaled_height,
                start_column, start_row,
                width, height,
                dest, dst_stride);
        case INTEGER_BOX:
            return downscale_integer_box_filter(fetcher, pict, orig_width, orig_height,
                scaled_width, scaled_height,
                start_column, start_row,
                width, height,
                dest, dst_stride);
        default:
            return -1;
    }
}
