#ifndef XS_LENGTH_H
#define XS_LENGTH_H

#include <sys/types.h>

#include "xs_md5.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Types
 */
typedef struct _sldb_node_t {
    xs_md5hash_t    md5Hash;    /* 128-bit MD5 hash-digest */
    int            nlengths;   /* Number of lengths */
    int            *lengths;   /* Lengths in seconds */
    struct _sldb_node_t *prev, *next;
} sldb_node_t;


typedef struct {
    sldb_node_t     *nodes,
                    **pindex;
    size_t          n;
} xs_sldb_t;


/* Functions
 */
int            xs_sldb_read(xs_sldb_t *, const char *);
void           xs_sldb_index(xs_sldb_t *);
void            xs_sldb_free(xs_sldb_t *);
sldb_node_t *   xs_sldb_get(xs_sldb_t *, const char *);

#ifdef __cplusplus
}
#endif
#endif /* XS_LENGTH_H */
