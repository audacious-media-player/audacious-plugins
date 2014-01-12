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

/*
 * Ringbuffer implementation
 *
 * GPL
 */
#include <assert.h>
#include <string.h>
#include <glib.h>

#include "rb.h"

/*
 * Reset a ringbuffer structure (i.e. discard all data inside of it)
 */
void reset_rb (struct ringbuf * rb)
{
    _RB_LOCK (rb->lock);

    rb->wp = rb->buf;
    rb->rp = rb->buf;
    rb->free = rb->size;
    rb->used = 0;
    rb->end = rb->buf + (rb->size - 1);

    _RB_UNLOCK (rb->lock);
}

/*
 * Initialize a ringbuffer structure (including memory allocation.
 * The mutex to be used is passed in the function call.
 * The mutex must not be held while calling this function.
 */
void init_rb_with_lock (struct ringbuf * rb, unsigned size, rb_mutex_t * lock)
{
    assert (size > 0);

    rb->lock = lock;
    rb->buf = g_malloc (size);
    rb->size = size;
    reset_rb (rb);
}

/*
 * Write size bytes at buf into the ringbuffer.
 */
void write_rb (struct ringbuf * rb, void * buf, unsigned size)
{
    _RB_LOCK (rb->lock);

    assert (size <= rb->free);

    int endfree = (rb->end - rb->wp) + 1;

    if (endfree < size)
    {
        /* There is enough space in the buffer, but not in one piece.
         * We need to split the copy into two parts. */
        memcpy (rb->wp, buf, endfree);
        memcpy (rb->buf, (char *) buf + endfree, size - endfree);
        rb->wp = rb->buf + (size-endfree);
    }
    else if (endfree > size)
    {
        /* There is more space than needed at the end */
        memcpy (rb->wp, buf, size);
        rb->wp += size;
    }
    else
    {
        /* There is exactly the space needed at the end.
         * We need to wrap around the read pointer. */
        memcpy (rb->wp, buf, size);
        rb->wp = rb->buf;
    }

    rb->free -= size;
    rb->used += size;

    _RB_UNLOCK (rb->lock);
}

/*
 * Read size byes from buffer into buf.
 * Return -1 on error (not enough data in buffer)
 */
int read_rb (struct ringbuf * rb, void * buf, unsigned size)
{
    _RB_LOCK (rb->lock);
    int ret = read_rb_locked (rb, buf, size);
    _RB_UNLOCK (rb->lock);

    return ret;
}

/*
 * Read size bytes from buffer into buf, assuming the buffer lock is already held.
 * Return -1 on error (not enough data in buffer)
 */
int read_rb_locked (struct ringbuf * rb, void * buf, unsigned size)
{
    if (rb->used < size)
    {
        /* Not enough bytes in buffer */
        return -1;
    }

    if (rb->rp < rb->wp)
    {
        /* Read pointer is behind write pointer, all the data is available in one chunk */
        memcpy (buf, rb->rp, size);
        rb->rp += size;
    }
    else
    {
        /* Read pointer is before write pointer */
        int endused = (rb->end - rb->rp) +1;

        if (size < endused)
        {
            /* Data is available in one chunk */
            memcpy (buf, rb->rp, size);
            rb->rp += size;
        }
        else
        {
            /* There is enough data in the buffer, but it is fragmented. */
            memcpy (buf, rb->rp, endused);
            memcpy ((char *) buf + endused, rb->buf, size - endused);
            rb->rp = rb->buf + (size-endused);
        }
    }

    rb->free += size;
    rb->used -= size;

    return 0;
}

/*
 * Return the amount of free space currently in the rb
 */
unsigned free_rb (struct ringbuf * rb)
{
    _RB_LOCK (rb->lock);
    unsigned f = free_rb_locked (rb);
    _RB_UNLOCK (rb->lock);

    return f;
}

/*
 * Return the amount of free space currently in the rb.
 * Assume the rb lock is already being held.
 */
unsigned free_rb_locked (struct ringbuf * rb)
{
    return rb->free;
}

/*
 * Return the amount of used space currently in the rb
 */
unsigned used_rb (struct ringbuf * rb)
{
    _RB_LOCK (rb->lock);
    unsigned u = used_rb_locked (rb);
    _RB_UNLOCK (rb->lock);

    return u;
}

/*
 * Return the amount of used space currently in the rb.
 * Assume the rb lock is already being held.
 */
unsigned used_rb_locked (struct ringbuf * rb)
{
    return rb->used;
}

/*
 * destroy a ringbuffer
 */
void destroy_rb (struct ringbuf * rb)
{
    g_free (rb->buf);
}
