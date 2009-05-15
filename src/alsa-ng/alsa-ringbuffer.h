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

#ifndef _ALSAPLUG_RINGBUFFER_H
#define _ALSAPLUG_RINGBUFFER_H

#include <glib.h>

typedef GMutex alsaplug_ringbuffer_mutex_t;
#define _ALSAPLUG_RINGBUFFER_LOCK(L) g_mutex_lock(L)
#define _ALSAPLUG_RINGBUFFER_UNLOCK(L) g_mutex_unlock(L)

#include <stdlib.h>

#ifdef ALSAPLUG_RINGBUFFER_DEBUG
#define ASSERT_RB(buf) _alsaplug_ringbuffer_assert(buf)
#else
#define ASSERT_RB(buf)
#endif

typedef struct {
    alsaplug_ringbuffer_mutex_t* lock;
    char _free_lock;
    char* buf;
    char* end;
    char* wp;
    char* rp;
    unsigned int free;
    unsigned int used;
    unsigned int size;
} alsaplug_ringbuf_t;

int alsaplug_ringbuffer_init(alsaplug_ringbuf_t* rb, unsigned int size);
int alsaplug_ringbuffer_init_with_lock(alsaplug_ringbuf_t* rb, unsigned int size, alsaplug_ringbuffer_mutex_t* lock);
int alsaplug_ringbuffer_write(alsaplug_ringbuf_t* rb, void* buf, unsigned int size);
int alsaplug_ringbuffer_read(alsaplug_ringbuf_t* rb, void* buf, unsigned int size);
int alsaplug_ringbuffer_read_locked(alsaplug_ringbuf_t* rb, void* buf, unsigned int size);
void alsaplug_ringbuffer_reset(alsaplug_ringbuf_t* rb);
unsigned int alsaplug_ringbuffer_free(alsaplug_ringbuf_t* rb);
unsigned int alsaplug_ringbuffer_free_locked(alsaplug_ringbuf_t* rb);
unsigned int alsaplug_ringbuffer_used(alsaplug_ringbuf_t* rb);
unsigned int alsaplug_ringbuffer_used_locked(alsaplug_ringbuf_t* rb);
void alsaplug_ringbuffer_destroy(alsaplug_ringbuf_t* rb);

#endif
