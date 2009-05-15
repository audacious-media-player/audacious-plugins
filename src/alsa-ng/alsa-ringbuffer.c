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
#include <string.h>
#include "alsa-ringbuffer.h"
#include "alsa-debug.h"

#ifdef ALSAPLUG_RINGBUFFER_DEBUG
/*
 * An internal assertion function to make sure that the
 * ringbuffer structure is consistient.
 *
 * WARNING: This function will call abort() if the ringbuffer
 * is found to be inconsistient.
 */
static void _alsaplug_ringbuffer_assert(alsaplug_ringbuf_t* rb) {

    unsigned int realused;

    _ENTER;

    _DEBUG("rb->buf=%p, rb->end=%p, rb->wp=%p, rb->rp=%p, rb->free=%u, rb->used=%u, rb->size=%u",
            rb->buf, rb->end, rb->wp, rb->rp, rb->free, rb->used, rb->size);

    if (0 == rb->size) {
        _ERROR("Buffer size is 0");
        abort();
    }

    if (NULL == rb->buf) {
        _ERROR("Buffer start is NULL");
        abort();
    }

    if (rb->used+rb->free != rb->size) {
        _ERROR("rb->free and rb->used do not add up to rb->size");
        abort();
    }

    if (rb->buf+(rb->size-1) != rb->end) {
        _ERROR("rb->buf and rb->end not rb->size bytes apart");
        abort();
    }

    if ((rb->wp < rb->buf) || (rb->wp > rb->end)) {
        _ERROR("Write pointer outside buffer space");
        abort();
    }

    if ((rb->rp < rb->buf) || (rb->rp > rb->end)) {
        _ERROR("Read pointer outside buffer space");
        abort();
    }

    if (rb->rp <= rb->wp) {
        realused = rb->wp - rb->rp;
    } else {
        realused = (rb->end - rb->rp) + 1 + (rb->wp-rb->buf);
    }

    if (rb->used != realused) {
        _ERROR("Usage count is inconsistient (is %d, should be %d)", rb->used, realused);
        abort();
    }

    _LEAVE;
}
#endif

/*
 * Reset a ringbuffer structure (i.e. discard
 * all data inside of it)
 */
void alsaplug_ringbuffer_reset(alsaplug_ringbuf_t* rb) {

    _ENTER;

    _ALSAPLUG_RINGBUFFER_LOCK(rb->lock);

    rb->wp = rb->buf;
    rb->rp = rb->buf;
    rb->free = rb->size;
    rb->used = 0;
    rb->end = rb->buf+(rb->size-1);

    _ALSAPLUG_RINGBUFFER_UNLOCK(rb->lock);

    _LEAVE;
}

/* 
 * Initialize a ringbuffer structure (including
 * memory allocation.
 *
 * Return -1 on error
 */
int alsaplug_ringbuffer_init(alsaplug_ringbuf_t* rb, unsigned int size) {

    _ENTER;

    if (0 == size) {
        _LEAVE -1;
    }

    if (NULL == (rb->buf = malloc(size))) {
        _LEAVE -1;
    }
    rb->size = size;

    if (NULL == (rb->lock = g_mutex_new())) {
        _LEAVE -1;
    }

    rb->_free_lock = 1;

    alsaplug_ringbuffer_reset(rb);

    ASSERT_RB(rb);

    _LEAVE 0;
}

/* 
 * Initialize a ringbuffer structure (including
 * memory allocation.
 * The mutex to be used is passed in the function call.
 * The mutex must not be held while calling this function.
 *
 * Return -1 on error
 */
int alsaplug_ringbuffer_init_with_lock(alsaplug_ringbuf_t* rb, unsigned int size, alsaplug_ringbuffer_mutex_t* lock) {

    _ENTER;

    if (0 == size) {
        _LEAVE -1;
    }

    rb->lock = lock;
    rb->_free_lock = 0;

    if (NULL == (rb->buf = malloc(size))) {
        _LEAVE -1;
    }
    rb->size = size;
    alsaplug_ringbuffer_reset(rb);

    ASSERT_RB(rb);

    _LEAVE 0;
}

/*
 * Write size bytes at buf into the ringbuffer.
 * Return -1 on error (not enough space in buffer)
 */
int alsaplug_ringbuffer_write(alsaplug_ringbuf_t* rb, void* buf, unsigned int size) {

    int ret = -1;
    int endfree;

    _ENTER;

    _ALSAPLUG_RINGBUFFER_LOCK(rb->lock);

    ASSERT_RB(rb);

    if (rb->free < size) {
        ret = -1;
        goto out;
    }

    endfree = (rb->end - rb->wp)+1;
    if (endfree < size) {
        /*
         * There is enough space in the buffer, but not in
         * one piece. We need to split the copy into two parts.
         */
        memcpy(rb->wp, buf, endfree);
        memcpy(rb->buf, buf+endfree, size-endfree);
        rb->wp = rb->buf + (size-endfree);
    } else if (endfree > size) {
        /*
         * There is more space than needed at the end
         */
        memcpy(rb->wp, buf, size);
        rb->wp += size;
    } else {
        /*
         * There is exactly the space needed at the end.
         * We need to wrap around the read pointer.
         */
        memcpy(rb->wp, buf, size);
        rb->wp = rb->buf;
    }

    rb->free -= size;
    rb->used += size;

    ret = 0;

out:
    ASSERT_RB(rb);
    _ALSAPLUG_RINGBUFFER_UNLOCK(rb->lock);

    _LEAVE ret;
}

/*
 * Read size byes from buffer into buf.
 * Return -1 on error (not enough data in buffer)
 */
int alsaplug_ringbuffer_read(alsaplug_ringbuf_t* rb, void* buf, unsigned int size) {

    int ret;

    _ENTER;

    _ALSAPLUG_RINGBUFFER_LOCK(rb->lock);
    ret = alsaplug_ringbuffer_read_locked(rb, buf, size);
    _ALSAPLUG_RINGBUFFER_UNLOCK(rb->lock);

    _LEAVE ret;
}

/*
 * Read size bytes from buffer into buf, assuming the buffer lock
 * is already held.
 * Return -1 on error (not enough data in buffer)
 */
int alsaplug_ringbuffer_read_locked(alsaplug_ringbuf_t* rb, void* buf, unsigned int size) {

    int endused;

    _ENTER;

    ASSERT_RB(rb);

    if (rb->used < size) {
        /* Not enough bytes in buffer */
        _LEAVE -1;
    }

    if (rb->rp < rb->wp) {
        /*
        Read pointer is behind write pointer, all the data is available in one chunk
        */
        memcpy(buf, rb->rp, size);
        rb->rp += size;
    } else {
        /*
         * Read pointer is before write pointer
         */
        endused = (rb->end - rb->rp)+1;

        if (size < endused) {
            /*
             * Data is available in one chunk
             */
            memcpy(buf, rb->rp, size);
            rb->rp += size;
        } else {
            /*
             * There is enough data in the buffer, but it is fragmented.
             */
            memcpy(buf, rb->rp, endused);
            memcpy(buf+endused, rb->buf, size-endused);
            rb->rp = rb->buf + (size-endused);
        }
    }

    rb->free += size;
    rb->used -= size;

    ASSERT_RB(rb);

    _LEAVE 0;
}

/*
 * Return the amount of free space currently in the rb
 */
unsigned int alsaplug_ringbuffer_free(alsaplug_ringbuf_t* rb) {

    unsigned int f;

    _ENTER;

    _ALSAPLUG_RINGBUFFER_LOCK(rb->lock);
    f = alsaplug_ringbuffer_free_locked(rb);
    _ALSAPLUG_RINGBUFFER_UNLOCK(rb->lock);

    _LEAVE f;
}

/*
 * Return the amount of free space currently in the rb.
 * Assume the rb lock is already being held.
 */
unsigned int alsaplug_ringbuffer_free_locked(alsaplug_ringbuf_t* rb) {

    _ENTER;

    _LEAVE rb->free;
}


/*
 * Return the amount of used space currently in the rb
 */
unsigned int alsaplug_ringbuffer_used(alsaplug_ringbuf_t* rb) {

    unsigned int u;

    _ENTER;

    _ALSAPLUG_RINGBUFFER_LOCK(rb->lock);
    u = alsaplug_ringbuffer_used_locked(rb);
    _ALSAPLUG_RINGBUFFER_UNLOCK(rb->lock);

    _LEAVE u;
}

/*
 * Return the amount of used space currently in the rb.
 * Assume the rb lock is already being held.
 */
unsigned int alsaplug_ringbuffer_used_locked(alsaplug_ringbuf_t* rb) {

    _ENTER;

    _LEAVE rb->used;
}


/*
 * destroy a ringbuffer
 */
void alsaplug_ringbuffer_destroy(alsaplug_ringbuf_t* rb) {

    _ENTER;
    free(rb->buf);
    if (rb->_free_lock) {
        g_mutex_free(rb->lock);
    }

    _LEAVE;
}
