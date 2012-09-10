#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "quick_segment.h"

#include <IL/il.h>

// gcc -O2 -lIL -o csv-segment csv-segment.c quick_segment.a

// licensed LGPL

/*
ABOUT
    example of how to use quick_segment with DevIL
    you define a few functions for your choice of image library
        take a look at quick_segment.h for more details
    DevIL starts at lower left corner, works up
        reported Y coords might be backwards from what you expect

TODO
    find an O(1) [memory] image streaming library for this example
    make quick_segment into dynamic library
*/

struct misc
{
    char* filename;
    int   invert;
};

void log_blob_hook(void* user_struct, struct blob* b)
// center_x, center_y and size are the cumulative stats for a blob
{
    printf("%.2f,%.2f,%i\n", b->center_x, b->center_y, b->size);
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
    if (!options->invert)
        {return 0;}
    for (x=0; x < stream->w; x++)
        {stream->row[x] = 255 - stream->row[x];}
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
    printf("csv-segment --  Find and count unconnected blobs in an image\n\n");
    printf("Use: csv-segment white|black image.file\n");
    printf("     white|black is the color of the foreground\n");
    printf("     x_center, y_center, pixel_size printed to stdout\n\n");
}

int main(int argc, char *argv[])
{
    struct misc user_struct;

    if (argc != 3)
        {use(); return 2;}

    user_struct.invert = -1;
    if (strncmp(argv[1], "black", 5) == 0)
        {user_struct.invert = 0;}
    if (strncmp(argv[1], "white", 5) == 0)
        {user_struct.invert = 1;}
    if (user_struct.invert == -1)
        {use(); return 2;}

    user_struct.filename = argv[2];

    printf("X,Y,size\n");
    return segment_image((void*)&user_struct);
}

