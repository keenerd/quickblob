#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void show(struct blob* b)
{
    printf("%i:%i (%i, %i) %.2f, %.2f %X\n", b->x1, b->x2, b->size, b->color, b->center_x, b->center_y, b);
}

void show_sib(struct blob* b)
{
    if (b == NULL)
        {printf("NULL\n"); return;}
    printf("%X %i:%i @%i (%i) %X %X\n", b, b->x1, b->x2, b->y, b->size, b->sib_p, b->sib_n);
}

void show_link(struct blob* b)
{
    if (b == NULL)
        {printf("NULL\n"); return;}
    printf("%X (%X %X) (%X %X)\n", b, b->prev, b->next, b->sib_p, b->sib_n);
}

void show_status(struct blob* bl_start, struct stream_state* stream)
{
    struct blob* b;
    int i=0, j=0;
    printf("stream %i %i %i\n", stream->x, stream->y, stream->wrap);
    
    b = bl_start;
    while (b)
    {
	if (b->size == 0)
            {i++;}
        else
            {j++;}
	b = b->next;
    }
    printf("blobs %i %i %i\n", i, j, i+j);
}

void show_blobs(struct blob* bl_start)
{
    struct blob* b;
    b = bl_start;
    while (b)
    {
        if (b->x1 != -1 && b->size != 0)
            {show_sib(b);}
        b = b->next;
    }
}

void show_dead_sibs(struct blob* bl_start)
{
    struct blob* b;
    b = bl_start;
    while (b)
    {
        if ((b->sib_p != NULL && b->sib_p->size == 0) ||
            (b->sib_n != NULL && b->sib_n->size == 0))
            {printf("DEAD %X\n", b);}
        b = b->next;
    }
}
