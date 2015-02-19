
/*
benchmark for the library
    no real images, simply a generated video-like stream
    some parameters for the size and fill pattern
    only for linux because of the simpler timers
*/


#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "quickblob.h"

// gcc -O2 -pg -o blobmark blobmark.c quickblob.a

// licensed LGPL

struct misc
{
    int width, height;
    int fore, back, p;
    int target_seconds;
    int64_t start_sec, start_nsec;
    int64_t stop_sec, stop_nsec;
    int64_t frame_counter;
};

void log_blob_hook(void* user_struct, struct blob* b)
// benchmark, so do nothing
{
    return;
}

int init_pixel_stream_hook(void* user_struct, struct stream_state* stream)
// get the image ready for streaming
// set the width and height, start the clock
{
    struct timespec ts;
    struct misc* options = user_struct;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    options->start_sec = ts.tv_sec;
    options->start_nsec = ts.tv_nsec;
    
    stream->w = options->width;
    stream->h = options->height;
    return 0;
}

int next_row_hook(void* user_struct, struct stream_state* stream)
// y is given, load that row into stream->row
{
    struct misc* options = user_struct;
    int x;

    for (x=0; x < stream->w; x++)
    {
        options->p = (options->p + 1) % (options->fore + options->back);
        stream->row[x] = options->p < options->fore;
    }
    return 0;
}

int next_frame_hook(void* user_struct, struct stream_state* stream)
{
    struct timespec ts;
    struct misc* options = user_struct;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    options->stop_sec = ts.tv_sec;
    options->stop_nsec = ts.tv_nsec;
    if (options->target_seconds == -1)
        {return 1;}
    if ((options->stop_sec - options->start_sec) > options->target_seconds)
        {return 1;}

    options->frame_counter++;
    if (options->target_seconds == 0)
        {options->target_seconds = -1; return 0;}
    return 0;
}


int close_pixel_stream_hook(void* user_struct, struct stream_state* stream)
{
    return 0;
}

void use(void)
{
    printf("blobmark --  A basic video throughput benchmark\n\n");
    printf("Use: blobmark width height seconds fore-len back-len\n");
    printf("    width: integer of pixels\n");
    printf("    height: integer of pixels\n");
    printf("    seconds: integer of benchmark duration\n");
    printf("          0 will run a single frame\n");
    printf("    fore/back len: integer of pixel run\n");
    printf("          0 1 is best case (blank frames)\n");
    printf("          1 1 is worst case (tiny stripes)\n");
    printf("          10 10 or so is fair\n\n");
    printf("all arguments are required\n\n");
}

int main(int argc, char *argv[])
{
    int64_t runtime_ns;
    struct misc user_struct;
    user_struct.frame_counter = 0L;

    if (argc != 6)
        {use(); return 1;}

    /* todo, real arg parsing */
    user_struct.width = atoi(argv[1]);
    user_struct.height = atoi(argv[2]);
    user_struct.target_seconds = atoi(argv[3]);
    user_struct.fore = atoi(argv[4]);
    user_struct.back = atoi(argv[5]);

    printf("Running, please wait %i seconds...\n", user_struct.target_seconds);
    extract_image((void*)&user_struct);

    runtime_ns = (user_struct.stop_sec - user_struct.start_sec) * 1000000000L + (user_struct.stop_nsec - user_struct.start_nsec);
    printf("FPS: %f\n", (double)user_struct.frame_counter * 1e9 / (double)runtime_ns);
    return 0;
}

