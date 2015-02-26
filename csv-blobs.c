#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "quickblob.h"

#include <png.h>

// gcc -O2 -lIL -o csv-blobs csv-blobs.c quickblob.a

// licensed LGPL

/*
ABOUT
    example of how to use QuickBlob with libpng
    you define a few functions for your choice of image library
        take a look at quickblob.h for more details

TODO
    make quickblob into dynamic library
*/

struct misc
{
    char* filename;
    int   threshold;
    int   show_bb;
    int   frame;
    FILE* fp;
    png_structp png_ptr;
    png_infop info_ptr;
};

void log_blob_hook(void* user_struct, struct blob* b)
// center_x, center_y and size are the cumulative stats for a blob
{
    struct misc* options = user_struct;
    printf("%.2f,%.2f,%i,%i", b->center_x, b->center_y, b->size, b->color);
    if (options->show_bb)
        {printf(",%i,%i,%i,%i", b->bb_x1, b->bb_y1, b->bb_x2, b->bb_y2);}
    printf("\n");
}

int init_pixel_stream_hook(void* user_struct, struct stream_state* stream)
// get the image ready for streaming
// set the width and height
// mostly deals with libpng junk (for comparison, DevIL init was 5 lines)
{
    struct misc* opt = user_struct;
    unsigned int width, height;
    int bit_depth, color_type, interlace_type;
    unsigned char header[8];

    // check file
    opt->fp = fopen(opt->filename, "rb");
    if (!opt->fp)
        {fprintf(stderr, "could not open file\n"); return 1;}
    fread(header, 1, 8, opt->fp);
    if (png_sig_cmp(header, 0, 8))
        {fprintf(stderr, "file is not a png\n"); return 1;}

    // init
    opt->png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!opt->png_ptr)
        {fprintf(stderr, "create_read failure\n"); return 1;}
    opt->info_ptr = png_create_info_struct(opt->png_ptr);
    if (!opt->info_ptr)
        {fprintf(stderr, "create_info failure\n"); return 1;}
    if (setjmp(png_jmpbuf(opt->png_ptr)))
        {fprintf(stderr, "init_io failure\n"); return 1;}
    png_init_io(opt->png_ptr, opt->fp);
    png_set_sig_bytes(opt->png_ptr, 8);
    png_read_info(opt->png_ptr, opt->info_ptr);
    png_get_IHDR(opt->png_ptr, opt->info_ptr, &width, &height,
        &bit_depth, &color_type, &interlace_type, NULL, NULL);
    if (interlace_type != PNG_INTERLACE_NONE)
        {fprintf(stderr, "interlaced PNGs not supported\n"); return 1;}
    stream->w = (int)width;
    stream->h = (int)height - 1;

    //number_of_passes = png_set_interlace_handling(png_ptr);

    // force to 8 bit grayscale
    if (bit_depth == 16)
        {png_set_strip_16(opt->png_ptr);}
    if (color_type == PNG_COLOR_TYPE_PALETTE)
        {png_set_palette_to_rgb(opt->png_ptr);}
    if (color_type == PNG_COLOR_TYPE_GRAY_ALPHA || color_type == PNG_COLOR_TYPE_RGB_ALPHA)
        {png_set_strip_alpha(opt->png_ptr);}
    if (color_type != PNG_COLOR_TYPE_GRAY)
        {png_set_rgb_to_gray(opt->png_ptr, 1, -1.0, -1.0);}
    if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)
        {png_set_expand_gray_1_2_4_to_8(opt->png_ptr);}
    png_set_crc_action(opt->png_ptr, PNG_CRC_QUIET_USE, PNG_CRC_QUIET_USE);
    // only one of these?
    //png_start_read_image(opt->png_ptr);
    png_read_update_info(opt->png_ptr, opt->info_ptr);

    if (setjmp(png_jmpbuf(opt->png_ptr)))
        {fprintf(stderr, "read_image failure\n"); return 1;}

    printf("%i,%i,%i,%i\n", stream->w, stream->h, stream->w * stream->h, -1);
    return 0;
}

int next_row_hook(void* user_struct, struct stream_state* stream)
// y is given, load that row into stream->row
{
    struct misc* options = user_struct;
    int x;
    // libpng requires dead reckoning, y is implicit
    png_read_row(options->png_ptr, stream->row, NULL);
    if (options->threshold < 0)
        {return 0;}
    for (x=0; x < stream->w; x++)
        {stream->row[x] = (stream->row[x] >= options->threshold) * 255;}
    return 0;
}

int next_frame_hook(void* user_struct, struct stream_state* stream)
// single-image application, so this is a no-op
{
    struct misc* options = user_struct;
    options->frame++;
    return options->frame;
}

int close_pixel_stream_hook(void* user_struct, struct stream_state* stream)
// free anything malloc'd during the init
{
    struct misc* opt = user_struct;
    //png_read_end(opt->png_ptr, NULL);
    png_destroy_read_struct(&opt->png_ptr, &opt->info_ptr, (png_infopp)NULL);
    fclose(opt->fp);
    return 0;
}

void use(void)
{
    printf("csv-blobs --  Find and count unconnected blobs in an image\n\n");
    printf("Use: csv-blobs [-t threshold] [--bbox] image.png\n");
    printf("    threshold - optional arg for 2-level processing\n\n");
    printf("x_center, y_center, pixel_size, color, printed to stdout\n");
    printf("    --bbox adds bounding box information\n");
    printf("    color -1 is used for image size metadata\n\n");
}

int main(int argc, char *argv[])
{
    int i;
    struct misc user_struct;
    user_struct.threshold = -1;
    user_struct.frame = -1;

    if (argc <= 1 || argc >= 6)
        {use(); return 1;}

    /* todo, real arg parsing */
    for (i=0; i<argc; i++)
    {
        if (strcmp(argv[i], "-h") == 0)
            {use(); return 0;}
        if (strcmp(argv[i], "--help") == 0)
            {use(); return 0;}
        if (strcmp(argv[i], "--bbox") == 0)
            {user_struct.show_bb = 1;}
        if (strcmp(argv[i], "-t") == 0)
            {user_struct.threshold = atoi(argv[i+1]);}
    }

    user_struct.filename = argv[argc-1];

    printf("X,Y,size,color");
    if (user_struct.show_bb)
        {printf(",bb_x1,bb_y1,bb_x2,bb_y2");}
    printf("\n");

    extract_image((void*)&user_struct);
    return 0;
}

