#include <stdlib.h>
#include <time.h>
#include <stdio.h>
#include <sched.h>
#include <sys/time.h>
#include <math.h>

#include "pixman.h"
//#include "eng.h"
#include "minpng.h"

#define WIDTH 400
#define HEIGHT 400

int randint(int min, int max)
{
    int q = min + random() % (max-min+1);
    assert(q >= min);
    assert(q <= max);
}

int sum(void *a, int b){
    return 0;
}

#include "../pixman/pixman-rescale.h"
//#include "in.h"
#if 1
extern char input[];
#endif
int main()
{
    int orig_width = 1024;
    int orig_height = 768;
    uint32_t *src = malloc (orig_width * orig_height * 4);
    uint32_t *dest = malloc (WIDTH * HEIGHT * 4);
    pixman_image_t *src_img;
    pixman_image_t *dest_img;
    int i, j;

    for (i = 0; i < orig_width * orig_height; ++i)
        src[i] = 0xff7f0000; /* red */

    for (i = 0; i < WIDTH * HEIGHT; ++i)
        dest[i] = 0xff0000ff; /* blue */

    src_img = pixman_image_create_bits (PIXMAN_a8r8g8b8,
            //WIDTH, HEIGHT,
            orig_width, orig_height,
            input,
            orig_width * 4);

    dest_img = pixman_image_create_bits (PIXMAN_a8r8g8b8,
            WIDTH, HEIGHT,
            dest,
            WIDTH * 4);

    int k;
        int scaled_width = 200;
        int scaled_height = 200;
        int x = 0;
        int y = 0;
        // keep these above 0 for now
        int width = scaled_width;
        int height = scaled_height;
        //printf("%d %d\n", width, height);
        //printf("%d %d\n", scaled_width, scaled_height);
        //printf("%d %d\n", x, y);
    clock_t start, end;
    start = clock();
    downscale_image(src_img, LANCZOS,
		scaled_width, scaled_height,
		x, y,
		width, height,
                dest, WIDTH * 4);
    end = clock();
    printf("t: %d\n", end - start);

    write_png("1.png", dest, WIDTH, HEIGHT);
    int lan_sum = sum(dest, WIDTH * HEIGHT * 4);
    printf("%x\n", lan_sum);

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
    start = clock();
    downscale_image(src_img, INTEGER_BOX,
		scaled_width, scaled_height,
		x, y,
		width, height,
                dest, WIDTH * 4);
    end = clock();
    printf("t: %d\n", end - start);

    int box_sum = sum(dest, WIDTH * HEIGHT * 4);
    printf("%x\n", box_sum);
    write_png("2.png", dest, WIDTH, HEIGHT);

    for (i = 0; i < WIDTH * HEIGHT; ++i)
        dest[i] = 0xff0000ff; /* blue */

    start = clock();
    downscale_image(src_img, BOX,
		scaled_width, scaled_height,
		x, y,
		width, height,
                dest, WIDTH * 4);
    end = clock();
    printf("t: %d\n", end - start);
    int mult_sum = sum(dest, WIDTH * HEIGHT * 4);
    printf("%x\n", mult_sum);
    write_png("3.png", dest, WIDTH, HEIGHT);
/*
    start = clock();
    downscale_box_mult_old_filter(src_img,
            orig_width, orig_height,
		scaled_width, scaled_height,
		x, y,
		width, height,
		src, orig_width * 4,
                dest, WIDTH * 4);
    end = clock();
    printf("t: %d\n", end - start);
*/

}
