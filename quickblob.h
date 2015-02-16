#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// gcc -O2 -c quickblob.c -o quickblob.o
// ar rvs quickblob.a quickblob.o

// licensed LGPL

/*
ABOUT
    image is streamed row by row
        stream_state keeps track of this
    blobs are assembled incrementally
    complete blobs are passed to log_blob_hook
    see more details in quickblob.c
*/

/* some structures you'll be working with */

struct blob
// you'll probably only need size, color, center_x and center_y
{
    int size;
    int color;
    // track current line segment
    int x1;
    int x2;
    int y;
    // basic linked list
    struct blob* prev;
    struct blob* next;
    // siblings are connected segments
    struct blob* sib_p;
    struct blob* sib_n;
    // incremental average
    double center_x;
    double center_y;
    // bounding box
    int bb_x1, bb_y1, bb_x2, bb_y2;
    // single linked list for tracking all old pixels
    // struct blob* old;
};

struct stream_state
// make a struct to hold an state required by the image loader
// and reference in the handle pointer
{
    int w, h, x, y;
    int wrap;  // don't touch this
    unsigned char* row;
    void* handle;
};

/* these are the functions you need to define
 * you get void pointer for passing around useful data */

void log_blob_hook(void* user_struct, struct blob* b);
// the blob struct will be for a completely finished blob
// you'll probably want to printf() important parts
// or write back to something in user_struct

int init_pixel_stream_hook(void* user_struct, struct stream_state* stream);
// you need to set several variables:
// stream->w = image width
// stream->h = image hight
// return status (0 for success)

int close_pixel_stream_hook(void* user_struct, struct stream_state* stream);
// free up anything you allocated in init_pixel_stream_hook
// return status (0 for success)

int next_row_hook(void* user_struct, struct stream_state* stream);
// load the (grayscale) row at stream->y into the (8 bit) stream->row array
// return status (0 for success)

int next_frame_hook(void* user_struct, struct stream_state* stream);
// basically a no-op in the library, but useful for applications
// return status (0 for success, otherwise breaks the video loop)

/* callable functions */

int extract_image(void* user_struct);

