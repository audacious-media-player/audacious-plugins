#ifndef XS_MD5_H
#define XS_MD5_H

#include <glib.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Typedefs
 */
typedef struct md5_state_s {
    guint32 bits[2];    /* message length in bits, lsw first */
    guint32 buf[4];     /* digest buffer */
    guint8 in[64];      /* accumulate block */
} xs_md5state_t;

#define XS_MD5HASH_LENGTH       (16)
#define XS_MD5HASH_LENGTH_CH    (XS_MD5HASH_LENGTH * 2)

typedef guint8 xs_md5hash_t[XS_MD5HASH_LENGTH];


/* Functions
 */
void xs_md5_init(xs_md5state_t *ctx);
void xs_md5_append(xs_md5state_t *ctx, const guint8 *buf, guint len);
void xs_md5_finish(xs_md5state_t *ctx, xs_md5hash_t digest);


#ifdef __cplusplus
}
#endif
#endif /* XS_MD5_H */
