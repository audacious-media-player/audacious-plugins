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
#include "rb.h"
#include "debug.h"

#ifdef RB_DEBUG
/*
 * An internal assertion function to make sure that the
 * ringbuffer structure is consistient.
 *
 * WARNING: This function will call abort() if the ringbuffer
 * is found to be inconsistient.
 */
static void _assert_rb(struct ringbuf* rb) {

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
void reset_rb(struct ringbuf* rb) {

    _ENTER;

    _RB_LOCK(rb->lock);

    rb->wp = rb->buf;
    rb->rp = rb->buf;
    rb->free = rb->size;
    rb->used = 0;
    rb->end = rb->buf+(rb->size-1);

    _RB_UNLOCK(rb->lock);

    _LEAVE;
}

/*
 * Initialize a ringbuffer structure (including
 * memory allocation.
 *
 * Return -1 on error
 */
int init_rb(struct ringbuf* rb, unsigned int size) {

    _ENTER;

    if (0 == size) {
        _LEAVE -1;
    }

    if (NULL == (rb->buf = malloc(size))) {
        _LEAVE -1;
    }
    rb->size = size;

#ifdef _RB_USE_GLIB
    if (NULL == (rb->lock = g_mutex_new())) {
        _LEAVE -1;
    }
#else
    if (NULL == (rb->lock = malloc(sizeof(pthread_mutex_t)))) {
        _LEAVE -1;
    }

    if (0 != pthread_mutex_init(rb->lock, NULL)) {
        free(rb->lock);
        _LEAVE -1;
    }
#endif
    rb->_free_lock = 1;

    reset_rb(rb);

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
int init_rb_with_lock(struct ringbuf* rb, unsigned int size, rb_mutex_t* lock) {

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
    reset_rb(rb);

    ASSERT_RB(rb);

    _LEAVE 0;
}

/*
 * Write size bytes at buf into the ringbuffer.
 * Return -1 on error (not enough space in buffer)
 */
int write_rb(struct ringbuf* rb, void* buf, unsigned int size) {

    int ret = -1;
    int endfree;

    _ENTER;

    _RB_LOCK(rb->lock);

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
        memcpy(rb->buf, (char *) buf + endfree, size - endfree);
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
    _RB_UNLOCK(rb->lock);

    _LEAVE ret;
}

/*
 * Read size byes from buffer into buf.
 * Return -1 on error (not enough data in buffer)
 */
int read_rb(struct ringbuf* rb, void* buf, unsigned int size) {

    int ret;

    _ENTER;

    _RB_LOCK(rb->lock);
    ret = read_rb_locked(rb, buf, size);
    _RB_UNLOCK(rb->lock);

    _LEAVE ret;
}

/*
 * Read size bytes from buffer into buf, assuming the buffer lock
 * is already held.
 * Return -1 on error (not enough data in buffer)
 */
int read_rb_locked(struct ringbuf* rb, void* buf, unsigned int size) {

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
            memcpy((char *) buf + endused, rb->buf, size - endused);
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
unsigned int free_rb(struct ringbuf* rb) {

    unsigned int f;

    _ENTER;

    _RB_LOCK(rb->lock);
    f = free_rb_locked(rb);
    _RB_UNLOCK(rb->lock);

    _LEAVE f;
}

/*
 * Return the amount of free space currently in the rb.
 * Assume the rb lock is already being held.
 */
unsigned int free_rb_locked(struct ringbuf* rb) {

    _ENTER;

    _LEAVE rb->free;
}


/*
 * Return the amount of used space currently in the rb
 */
unsigned int used_rb(struct ringbuf* rb) {

    unsigned int u;

    _ENTER;

    _RB_LOCK(rb->lock);
    u = used_rb_locked(rb);
    _RB_UNLOCK(rb->lock);

    _LEAVE u;
}

/*
 * Return the amount of used space currently in the rb.
 * Assume the rb lock is already being held.
 */
unsigned int used_rb_locked(struct ringbuf* rb) {

    _ENTER;

    _LEAVE rb->used;
}


/*
 * destroy a ringbuffer
 */
void destroy_rb(struct ringbuf* rb) {

    _ENTER;
    free(rb->buf);
    if (rb->_free_lock) {
#ifdef _RB_USE_GLIB
        g_mutex_free(rb->lock);
#else
        pthread_mutex_destroy(rb->lock);
        free(rb->lock);
#endif
    }

    _LEAVE;
}
