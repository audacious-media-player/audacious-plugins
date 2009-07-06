#ifndef XS_LENGTH_H
#define XS_LENGTH_H

#include "xmms-sid.h"
#ifdef AUDACIOUS_PLUGIN
#include <libaudcore/md5.h>
#define XS_MD5HASH_LENGTH   AUD_MD5HASH_LENGTH
#define XS_MD5HASH_LENGTH_CH   AUD_MD5HASH_LENGTH_CH
#define xs_md5hash_t        aud_md5hash_t
#define xs_md5state_t       aud_md5state_t
#define xs_md5_init         aud_md5_init
#define xs_md5_append       aud_md5_append
#define xs_md5_finish       aud_md5_finish
#else
#include "xs_md5.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Types
 */
typedef struct _sldb_node_t {
    xs_md5hash_t    md5Hash;    /* 128-bit MD5 hash-digest */
    gint            nlengths;   /* Number of lengths */
    gint            *lengths;   /* Lengths in seconds */
    struct _sldb_node_t *prev, *next;
} sldb_node_t;


typedef struct {
    sldb_node_t     *nodes,
                    **pindex;
    size_t          n;
} xs_sldb_t;


/* Functions
 */
gint            xs_sldb_read(xs_sldb_t *, const gchar *);
gint            xs_sldb_index(xs_sldb_t *);
void            xs_sldb_free(xs_sldb_t *);
sldb_node_t *   xs_sldb_get(xs_sldb_t *, const gchar *);

#ifdef __cplusplus
}
#endif
#endif /* XS_LENGTH_H */
