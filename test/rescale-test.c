#include <stdlib.h>
#include <time.h>
#include <stdio.h>
#include <sched.h>
#include <sys/time.h>
#include <math.h>

#include "pixman.h"
#include "eng.h"
#include "minpng.h"

#include "../pixman/pixman-rescale.h"
#define WIDTH 400
#define HEIGHT 400

int randint(int min, int max)
{
    int q = min + random() % (max-min+1);
    assert(q >= min);
    assert(q <= max);
}

int main()
{
    uint32_t *src = malloc (WIDTH * HEIGHT * 4);
    uint32_t *dest = malloc (WIDTH * HEIGHT * 4);
    pixman_image_t *src_img;
    pixman_image_t *dest_img;
    int i, j;

    for (i = 0; i < WIDTH * HEIGHT; ++i)
        src[i] = 0xff7f0000; /* red */

    for (i = 0; i < WIDTH * HEIGHT; ++i)
        dest[i] = 0xff0000ff; /* blue */

    int orig_width = WIDTH;
    int orig_height = HEIGHT;
    src_img = pixman_image_create_bits (PIXMAN_a8r8g8b8,
            //WIDTH, HEIGHT,
            orig_width, orig_height,
            src,
            WIDTH * 4);

    dest_img = pixman_image_create_bits (PIXMAN_a8r8g8b8,
            WIDTH, HEIGHT,
            dest,
            WIDTH * 4);

    int k;
    for (k=0; k<900; k++) {
        int scaled_width = randint(1, WIDTH);
        int scaled_height = randint(1, HEIGHT);
        int x = randint(0, scaled_width-1);
        int y = randint(0, scaled_height-1);
        // keep these above 0 for now
        int width = randint(1, scaled_width - x);
        int height = randint(1, scaled_height - y);
#if 0
        printf("w:%d h:%d\n", width, height);
        printf("sw:%d sh:%d\n", scaled_width, scaled_height);
        printf("x:%d y:%d\n", x, y);
#endif
        downscale_image(src_img, LANCZOS,
		scaled_width, scaled_height,
		x, y,
		width, height,
                dest, WIDTH * 4);

    //write_png("1.png", dest, WIDTH, HEIGHT);
    int lan_sum = sum(dest, WIDTH * HEIGHT * 4);
    //printf("%x\n", lan_sum);

    for (i = 0; i < WIDTH * HEIGHT; ++i)
        dest[i] = 0xff0000ff; /* blue */
#if 0
int downscale_box_filter(pixman_image_t *pict, unsigned origWidth, unsigned origHeight,
		signed scaledWidth, signed scaledHeight,
		uint16_t startColumn, uint16_t startRow,
		uint16_t width, uint16_t height,
		uint32_t *src, int srcStride,
		uint32_t *dest, int dstStride)
#endif
    downscale_image(src_img, INTEGER_BOX,
		scaled_width, scaled_height,
		x, y,
		width, height,
                dest, WIDTH * 4);

    int box_sum = sum(dest, WIDTH * HEIGHT * 4);
    //printf("%x\n", box_sum);
    write_png("box.png", dest, WIDTH, HEIGHT);

    for (i = 0; i < WIDTH * HEIGHT; ++i)
        dest[i] = 0xff0000ff; /* blue */

    downscale_image(src_img, BOX,
		scaled_width, scaled_height,
		x, y,
		width, height,
                dest, WIDTH * 4);

    int mult_sum = sum(dest, WIDTH * HEIGHT * 4);
    //printf("%x\n", mult_sum);
    write_png("mult.png", dest, WIDTH, HEIGHT);
        for (i = 0; i < WIDTH * HEIGHT; ++i)
        dest[i] = 0xff0000ff; /* blue */
    
        if (!(mult_sum == lan_sum && box_sum == lan_sum)) {
            printf("%x %x %x\n", mult_sum, lan_sum, box_sum);
        }
        assert(mult_sum == lan_sum && box_sum == lan_sum);

    }

}
