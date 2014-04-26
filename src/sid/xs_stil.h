#ifndef XS_STIL_H
#define XS_STIL_H

#include <sys/types.h>

/* Types
 */
typedef struct {
    char *name, *author, *title, *info;
} stil_subnode_t;


typedef struct _stil_node_t {
    char *filename;
    int nsubTunes;
    stil_subnode_t **subTunes;
    struct _stil_node_t *prev, *next;
} stil_node_t;


typedef struct {
    stil_node_t *nodes, **pindex;
    size_t n;
} xs_stildb_t;


/* Functions
 */
int xs_stildb_read(xs_stildb_t *, char *);
void xs_stildb_index(xs_stildb_t *);
void xs_stildb_free(xs_stildb_t *);
stil_node_t *xs_stildb_get_node(xs_stildb_t *, char *);

#endif /* XS_STIL_H */
