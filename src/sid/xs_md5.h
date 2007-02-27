#ifndef XS_MD5_H
#define XS_MD5_H

#include <glib.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Typedefs
 */
typedef struct md5_state_s {
	guint32 bits[2];	/* message length in bits, lsw first */
	guint32 buf[4];		/* digest buffer */
	guint8 in[64];		/* accumulate block */
} t_xs_md5state;

#define XS_MD5HASH_LENGTH	(16)
#define XS_MD5HASH_LENGTH_CH	(XS_MD5HASH_LENGTH * 2)

typedef guint8 t_xs_md5hash[XS_MD5HASH_LENGTH];


/* Functions
 */
void xs_md5_init(t_xs_md5state *ctx);
void xs_md5_append(t_xs_md5state *ctx, const guint8 *buf, guint len);
void xs_md5_finish(t_xs_md5state *ctx, t_xs_md5hash digest);


#ifdef __cplusplus
}
#endif
#endif /* XS_MD5_H */
