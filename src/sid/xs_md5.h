/* Wrapper for GLib MD5 implementation */
/* John Lindgren, March 31, 2012 */
/* Public domain */

#ifndef XS_MD5_H
#define XS_MD5_H

#define XS_MD5HASH_LENGTH 16
#define XS_MD5HASH_LENGTH_CH 32

typedef struct {
    void * priv;
} xs_md5state_t;

typedef unsigned char xs_md5hash_t[XS_MD5HASH_LENGTH];

void xs_md5_init (xs_md5state_t * state);
void xs_md5_append (xs_md5state_t * state, const void * data, int length);
void xs_md5_finish (xs_md5state_t * state, xs_md5hash_t hash);

#endif
