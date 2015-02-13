#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "quickblob.h"

#include <IL/il.h>

// gcc -O2 -lIL -o csv-blobs csv-blobs.c quickblob.a

// licensed LGPL

/*
ABOUT
    example of how to use QuickBlob with DevIL
    you define a few functions for your choice of image library
        take a look at quickblob.h for more details
    DevIL starts at lower left corner, works up
        reported Y coords might be backwards from what you expect

TODO
    find an O(1) [memory] image streaming library for this example
    make quickblob into dynamic library
*/

struct misc
{
    char* filename;
    int   threshold;
    int   show_bb;
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
{
    struct misc* options = user_struct;
    ILboolean status;

    ilInit();
    ilEnable(IL_ORIGIN_SET);
    // yeah yeah, check the malloc for a single int
    stream->handle = (ILuint*) malloc(sizeof(ILuint));
    ilGenImages(1, (ILuint*)(stream->handle));

    status = ilLoadImage(options->filename);
    if (status == IL_FALSE)
        {return 1;}

    stream->w = ilGetInteger(IL_IMAGE_WIDTH);
    stream->h = ilGetInteger(IL_IMAGE_HEIGHT);
    printf("%i,%i,%i,%i\n", stream->w, stream->h, stream->w * stream->h, -1);
    return 0;
}

int next_row_hook(void* user_struct, struct stream_state* stream)
// y is given, load that row into stream->row
{
    struct misc* options = user_struct;
    int x;
    // CopyPixels is smart enough to convert rgb -> lum
    ilCopyPixels(0, stream->y, 0,
                 stream->w, 1, 1,
                 IL_LUMINANCE, IL_UNSIGNED_BYTE, (ILubyte*)stream->row);
    if (options->threshold < 0)
        {return 0;}
    for (x=0; x < stream->w; x++)
        {stream->row[x] = (stream->row[x] >= options->threshold) * 255;}
    return 0;
}

int close_pixel_stream_hook(void* user_struct, struct stream_state* stream)
// free anything malloc'd during the init
{
    ilDeleteImages(1, (ILuint*)(stream->handle));
    free(stream->handle);
    return 0;
}

void use(void)
{
    printf("csv-blobs --  Find and count unconnected blobs in an image\n\n");
    printf("Use: csv-blobs [-t threshold] [--bbox] image.file\n");
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
}

