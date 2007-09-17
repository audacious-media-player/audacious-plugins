#ifndef _RB_H
#define _RB_H

#include <pthread.h>
#include <stdlib.h>

#ifdef RB_DEBUG
#define ASSERT_RB(buf) _assert_rb(buf)
#else
#define ASSERT_RB(buf)
#endif

struct ringbuf {
    pthread_mutex_t lock;
    char* buf;
    char* end;
    char* wp;
    char* rp;
    unsigned int free;
    unsigned int used;
    unsigned int size;
};

int init_rb(struct ringbuf* rb, unsigned int size);
int write_rb(struct ringbuf* rb, void* buf, unsigned int size);
int read_rb(struct ringbuf* rb, void* buf, unsigned int size);
int read_rb_locked(struct ringbuf* rb, void* buf, unsigned int size);
void reset_rb(struct ringbuf* rb);
unsigned int free_rb(struct ringbuf* rb);
unsigned int used_rb(struct ringbuf* rb);

#endif
