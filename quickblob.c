#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "quickblob.h"
//#include "debugstuff.c"

// licensed LGPL

/*
ARCHITECTURE
    image is streamed row by row
    blobs are tracked as line segments on a row
    segments are stored in an ordered quad-linked list
    unused segments are linked to a stack
    3rd & 4th links are for dealing with V shaped blobs
        these occupy multiple segments, but were previously connected
        sub-lists of sibling blobs

TODO
    report all pixels (store old segments, dynamic malloc more)
        use for stats such as blob eccentricity
    binary foreground only mode (1/4 ram)
    concentric grayscale identification
    remove wrap flag
    make into a dynamic library
    -fvisibility=internal
    #define SYMEXPORT __attribute__((visibility("default")))
    split blob, nest public struct inside private struct
    switch from pointers to indexes
        not worth it, saves ram but at best 6% faster
*/


struct blob_list
{
    struct blob* head;
    int length;
    struct blob** empties;  // stack
    int empty_i;
};

static void blank(struct blob* b)
{
    b->size = 0;
    b->color = -1;
    b->x1 = -1;
    b->x2 = -1;
    b->y = -1;
    b->prev = NULL;
    b->next = NULL;
    b->sib_p = NULL;
    b->sib_n = NULL;
    b->center_x = 0.0;
    b->center_y = 0.0;
    b->bb_x1 = b->bb_y1 = b->bb_x2 = b->bb_y2 = -1;
}

static int init_pixel_stream(void* user_struct, struct stream_state* stream)
{
    memset(stream, 0, sizeof(struct stream_state));
    if (init_pixel_stream_hook(user_struct, stream))
        {return 1;}
    stream->row = NULL;
    // row is a bunch of 1x8 bit grayscale samples
    stream->row = (unsigned char*) malloc(stream->w * sizeof(unsigned char));
    stream->x = 0;
    stream->y = -1;  // todo, make x and y init the same
    stream->wrap = 0;
    return 0;
}

static int close_pixel_stream(void* user_struct, struct stream_state* stream)
{
    close_pixel_stream_hook(user_struct, stream);  // TODO return status
    free(stream->row);
    stream->row = NULL;
    return 0;
}

static int malloc_blobs(struct blob_list* blist)
{
    blist->head = (struct blob*) malloc(blist->length * sizeof(struct blob));
    if (blist->head == NULL)
        {return 1;}
    blist->empties = (struct blob**) malloc(blist->length * sizeof(struct blob*));
    if (blist->empties == NULL)
        {return 1;}
    return 0;
}

static int init_blobs(struct blob_list* blist)
{
    int i, len;
    struct blob* head;
    len = blist->length;
    blist->empty_i = 0;
    head = blist->head;
    for (i=0; i < len; i++)
        {blank(&(head[i]));}
    head[0].next = &head[1];
    head[1].prev = &head[0];
    for (i=1; i < len; i++)
    {
        blist->empties[blist->empty_i] = &(head[i]);
        blist->empty_i++;
    }
    return 0;
}

static void blob_unlink(struct blob* b2)
// remove from linked list
// (don't use on head, am lazy)
{
    struct blob* b1 = NULL;
    struct blob* b3 = NULL;
    struct blob* s1 = NULL;
    struct blob* s3 = NULL;
    b1 = b2->prev;
    b3 = b2->next;
    if (b1 != NULL)
        {b1->next = b3;}
    if (b3 != NULL)
        {b3->prev = b1;}
    b2->prev = NULL;
    b2->next = NULL;
    // unlink sibs
    s1 = b2->sib_p;
    s3 = b2->sib_n;
    if (s1 != NULL)
        {s1->sib_n = s3;}
    if (s3 != NULL)
        {s3->sib_p = s1;}
    b2->sib_p = NULL;
    b2->sib_n = NULL;
}

static void blob_insert(struct blob* bl_start, struct blob* b2)
// assume elements are ordered and unique
// never call with head of list, requires bl_start->prev
// will never change head since that is empty element
{
    struct blob* b1;
    struct blob* b3;
    b1 = bl_start->prev;
    b3 = b1->next;
    while (b1->next != NULL)
    {
        b3 = b1->next;
        // equality for fast reinsertion of empties
        if ((b1->x1 <= b2->x1) && (b2->x1 <= b3->x1))
        {
            b1->next = b2;
            b2->prev = b1;
            b2->next = b3;
            b3->prev = b2;
            return;
        }
        b1 = b1->next;
    }
    // append to end
    if (b1->x1 > b2->x1)
        {printf("insert error\n"); return;}  // TODO: should raise an error
    b1->next = b2;
    b2->prev = b1;
}

static int next_row(void* user_struct, struct stream_state* stream)
{
    if (stream->y >= stream->h)
        {return 1;}
    stream->wrap = 0;
    stream->x = 0;
    stream->y++;
    return next_row_hook(user_struct, stream);
}

static int next_frame(void* user_struct, struct stream_state* stream)
{
    stream->wrap = 0;
    stream->x = 0;
    stream->y = -1;
    return next_frame_hook(user_struct, stream);
}

static int scan_segment(struct stream_state* stream, struct blob* b)
// sets color/x1/x2 fields, returns 1 on error
// must call next_row() to continue scanning
{
    if (stream->wrap)
        {return 1;}
    b->x1 = stream->x;
    b->color = stream->row[stream->x];
    for (;;)  // awkward, but the two exit points are too similar
    {
        if (stream->x >= stream->w)
            {stream->wrap = 1; break;}
        if (b->color != (stream->row[stream->x]))
            {break;}
        stream->x++;
    }
    b->x2 = stream->x - 1;
    return 0;
}

static void bbox_update(struct blob* b, int x1, int x2, int y1, int y2)
{
    if (b->bb_x1 < 0)
        {b->bb_x1 = x1;}
    if (x1 < b->bb_x1)
        {b->bb_x1 = x1;}
    if (x2 > b->bb_x2)
        {b->bb_x2 = x2;}

    if (b->bb_y1 < 0)
        {b->bb_y1 = y1;}
    if (y1 < b->bb_y1)
        {b->bb_y1 = y1;}
    if (y2 > b->bb_y2)
        {b->bb_y2 = y2;}
}

static void blob_update(struct blob* b, int x1, int x2, int y)
{
    int s2;
    s2  = 1 + x2 - x1;
    b->x1 = x1;
    b->x2 = x2;
    b->y = y;
    b->center_x = ((b->center_x * b->size) + (x1+x2)*s2/2)/(b->size + s2);
    b->center_y = ((b->center_y * b->size) + (y * s2))/(b->size + s2);
    b->size += s2;
    bbox_update(b, x1, x2, y, y);
}

static void sib_link(struct blob* b1, struct blob* b2)
// merge sort two sibling chains into one
{
    struct blob* s1 = b1;
    struct blob* s2 = b2;
    struct blob* s_tmp;
    // find heads of each sibling list
    while (s1->sib_p != NULL)
        {s1 = s1->sib_p;}
    while (s2->sib_p != NULL)
        {s2 = s2->sib_p;}
    if (s1 == s2)
        {return;}
    while (s1 != NULL && s2 != NULL)
    {
        // cut code in half by forcing order
        if (s2->x1 < s1->x1)
        {
            s_tmp = s1;
            s1 = s2;
            s2 = s_tmp;
            continue;
        }
        // so s1 must come before s2
        // but what about s1->sib_n ?
        if ((s1->sib_n != NULL) && (s1->sib_n->x1 < s2->x1))
            {s1 = s1->sib_n; continue;}
        // normal case, interleave s1 and s2
        // pointer shell game!
        s_tmp = s1->sib_n;
        s1->sib_n = s2;
        s1->sib_n->sib_p = s1;
        s2 = s_tmp;
        s1 = s1->sib_n;
    }
}

static void blob_merge(struct blob* b1, struct blob* b2)
// merge b2 into b1, does not deal with sibs
{
    b1->center_x = ((b1->center_x * b1->size) + (b2->center_x * b2->size)) / (b1->size + b2->size);
    b1->center_y = ((b1->center_y * b1->size) + (b2->center_y * b2->size)) / (b1->size + b2->size);
    b1->size += b2->size;
    bbox_update(b1, b2->bb_x1, b2->bb_x2, b2->bb_y1, b2->bb_y2);
}

static int range_overlap(int a1, int a2, int b1, int b2)
// returns 1 for overlap, 0 for none
// pads by one for diagonals
{
    // could be less checks, but this is simple
    // b1 <= a1 <= b2
    if ((b1-1) <= a1 && a1 <= (b2+1))
        {return 1;}
    // b1 <= a2 <= b2
    if ((b1-1) <= a2 && a2 <= (b2+1))
        {return 1;}
    // a1 <= b1 <= a2
    if ((a1-1) <= b1 && b1 <= (a2+1))
        {return 1;}
    // a1 <= b2 <= a2
    if ((a1-1) <= b2 && b2 <= (a2+1))
        {return 1;}
    return 0;
}
 
static int blob_overlap(struct blob* b, int x1, int x2)
// returns 1 for hit, 0 for miss, -1 for abort
{
    if (b->x1 == -1)
        {return 0;}
    if (x1 == -1)
        {return 0;}
    if (x2 < (b->x1 - 1))
        {return -1;}
    return range_overlap(x1, x2, b->x1, b->x2);
}

static void blob_reap(struct blob_list* blist, struct blob* b)
{
    blob_unlink(b);
    blank(b);
    blist->empties[blist->empty_i] = b;
    blist->empty_i++;
}

static void sib_cleanup(struct blob_list* blist, struct blob* b)
// seems to do too much
{
    struct blob* s1;
    struct blob* s3;
    if (b->sib_p == NULL && b->sib_n == NULL)
        {return;}
    // paranoid...
    //if (b->sib_p != NULL && b->y > b->sib_p->y)
    //    {return;}  // should raise an error
    //if (b->sib_n != NULL && b->y > b->sib_n->y)
    //    {return;}  // should raise an error
    s1 = b->sib_p;
    s3 = b->sib_n;
    if (s1 != NULL && range_overlap(b->x1, b->x2, s1->x1, s1->x2))
        {blob_merge(s1, b); blob_reap(blist, b); return;}
    if (s3 != NULL && range_overlap(b->x1, b->x2, s3->x1, s3->x2))
        {blob_merge(s3, b); blob_reap(blist, b); return;}
    if (s1 != NULL)
        {blob_merge(s1, b); blob_reap(blist, b); return;}
    if (s3 != NULL)
        {blob_merge(s3, b); blob_reap(blist, b); return;}
}

static void sib_find(struct blob* blob_start, struct blob* blob_now)
{
    struct blob* b = NULL;
    int i;
    b = blob_start;
    while (b)
    {
        if (b->color != blob_now->color)
            {b = b->next; continue;}
        if (b->y == blob_now->y)
            {b = b->next; continue;}
        i = blob_overlap(b, blob_now->x1, blob_now->x2);
        if (i == -1)
            {break;}
        if (i == 1)
            {sib_link(b, blob_now);}
        b = b->next;
    }
}

static void flush_incremental(void* user_struct, struct blob_list* blist, struct blob* blob_now)
// merges/prints/reaps everything before blob_now
{
    struct blob* b;
    struct blob* b2;
    // pass 1, merge old sibs
    b = blob_now;
    while (b->sib_p)
    {
        b = b->sib_p;
        if (b->y != blob_now->y-1)
            {continue;}
        if (b->x1 == -1)
            {break;}
        if (b->x2 > blob_now->x2)
            {continue;}
        // merge b into b2
        b2 = b->sib_n;
        blob_merge(b2, b);
        blob_reap(blist, b);
        b = b2;
    }
    // pass 2, log isolated blobs
    b = blob_now;
    while (b->prev)
    {
        b = b->prev;
        if (b->y != blob_now->y-1)
            {continue;}
        if (b->x1 == -1)
            {break;}
        if (b->sib_n || b->sib_p)
            {continue;}
        b2 = b->next;
        log_blob_hook(user_struct, b);
        blob_reap(blist, b);
        b = b2;
    }
}

static void flush_old_blobs(void* user_struct, struct blob_list* blist, int y)
// merges (or prints) and reaps, y is current row
{
    struct blob* b; 
    struct blob* b2;
    b = blist->head->next;
    while (b)
    {
        if (b->size == 0)
            {b = b->next; continue;}
        if (b->x1 == -1)
            {b = b->next; continue;}
        if (b->y == y)
            {b = b->next; continue;}
        // use previous so the scan does not restart every reap
        b2 = b;
        if (b->prev != NULL)
            {b2 = b->prev;}
        if (b->sib_p == NULL && b->sib_n == NULL)
            {log_blob_hook(user_struct, b); blob_reap(blist, b);}
        else
            {sib_cleanup(blist, b);}
        b = b2->next;
    }
}

static struct blob* empty_blob(struct blob_list* blist)
{
    struct blob* b;
    blist->empty_i--;
    b = blist->empties[blist->empty_i];
    blist->empties[blist->empty_i] = NULL;
    return b;
}

int extract_image(void* user_struct)
{
    struct stream_state stream;
    struct blob_list blist;
    struct blob* blob_now = NULL;
    struct blob* blob_prev = NULL;

    if (init_pixel_stream(user_struct, &stream))
        {printf("init malloc error!\n"); return 1;}
    if (stream.row == NULL)
        {printf("row malloc error!\n"); return 1;}
    blist.length = stream.w + 5;
    if (malloc_blobs(&blist))
        {printf("blob malloc error!\n"); return 1;}

    while (!next_frame(user_struct, &stream))
    {
        init_blobs(&blist);
        while (!next_row(user_struct, &stream))
        {
            blob_prev = blist.head->next;
            while (!stream.wrap)
            {
                blob_now = empty_blob(&blist);
                if (scan_segment(&stream, blob_now))
                    {blob_reap(&blist, blob_now); continue;}
                blob_update(blob_now, blob_now->x1, blob_now->x2, stream.y);
                // update structure
                sib_find(blist.head->next, blob_now);
                blob_insert(blob_prev, blob_now);
                flush_incremental(user_struct, &blist, blob_now);
                blob_prev = blob_now;
            }
            flush_old_blobs(user_struct, &blist, stream.y);
            //show_status(blist.head, &stream);
            //show_dead_sibs(blist.head);
            //show_blobs(blist.head);
            //printf("----------\n");
        }
        flush_old_blobs(user_struct, &blist, stream.h - 1);
    }

    close_pixel_stream(user_struct, &stream);
    free(blist.head);
    free(blist.empties);
    blist.head = NULL;
    blist.empties = NULL;
    return 0; 
}

