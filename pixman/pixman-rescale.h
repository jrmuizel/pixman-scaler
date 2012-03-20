
#include "pixman.h"
struct rescale_fetcher
{
    void (*fetch_scanline)(void *closure, int y, uint32_t **scanline_ptr);
    uint32_t *(*init)(void *closure);
    void (*fini)(void *closure, uint32_t *scanline);
};

enum downscale_type
{
    INTEGER_BOX,
    BOX,
    LANCZOS
};

int downscale_image(pixman_image_t *image, enum downscale_type type,
		int scaled_width, int scaled_height,
		int start_column, int start_row,
		int width, int height,
		uint32_t *dest, int dst_stride);

int downscale_integer_box_filter(
        const struct rescale_fetcher *fetcher, void *closure,
        unsigned orig_width, unsigned orig_height,
		signed scaledWidth, signed scaled_height,
		uint16_t start_column, uint16_t start_row,
		uint16_t width, uint16_t height,
		uint32_t *dest, int dst_stride);

int downscale_box_filter(
        const struct rescale_fetcher *fetcher, void *closure,
        unsigned orig_width, unsigned orig_height,
		signed scaled_width, signed scaled_height,
		uint16_t start_column, uint16_t start_row,
		uint16_t width, uint16_t height,
		uint32_t *dest, int dst_stride);

int downscale_lanczos_filter(
        const struct rescale_fetcher *fetcher, void *closure,
        unsigned orig_width, unsigned orig_height,
		signed scaledWidth, signed scaled_height,
		uint16_t start_column, uint16_t start_row,
		uint16_t width, uint16_t height,
		uint32_t *dest, int dst_stride);
