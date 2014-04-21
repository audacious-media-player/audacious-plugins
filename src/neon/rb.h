/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef _RB_H
#define _RB_H

#include <pthread.h>

typedef pthread_mutex_t rb_mutex_t;

#define _RB_LOCK(L) pthread_mutex_lock (L)
#define _RB_UNLOCK(L) pthread_mutex_unlock (L)

struct ringbuf
{
    rb_mutex_t * lock;
    char * buf;
    char * end;
    char * wp;
    char * rp;
    int free;
    int used;
    int size;
};

void init_rb_with_lock (struct ringbuf * rb, int size, rb_mutex_t * lock);
void write_rb (struct ringbuf * rb, void * buf, int size);
int read_rb (struct ringbuf * rb, void * buf, int size);
int read_rb_locked (struct ringbuf * rb, void * buf, int size);
void reset_rb (struct ringbuf * rb);
unsigned free_rb (struct ringbuf * rb);
unsigned free_rb_locked (struct ringbuf * rb);
unsigned used_rb (struct ringbuf * rb);
unsigned used_rb_locked (struct ringbuf * rb);
void destroy_rb (struct ringbuf * rb);

#endif
