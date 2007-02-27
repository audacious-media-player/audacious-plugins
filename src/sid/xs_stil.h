#ifndef XS_STIL_H
#define XS_STIL_H

#include "xmms-sid.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Types
 */
typedef struct {
	gchar	*pName,
		*pAuthor,
		*pTitle,
		*pInfo;
} t_xs_stil_subnode;


typedef struct _t_xs_stil_node {
	gchar			*pcFilename;
	gint			nsubTunes;
	t_xs_stil_subnode	**subTunes;
	
	struct _t_xs_stil_node	*pPrev, *pNext;
} t_xs_stil_node;


typedef struct {
	t_xs_stil_node	*pNodes,
			**ppIndex;
	size_t		n;
} t_xs_stildb;


/* Functions
 */
gint			xs_stildb_read(t_xs_stildb *, gchar *);
gint			xs_stildb_index(t_xs_stildb *);
void			xs_stildb_free(t_xs_stildb *);
t_xs_stil_node *	xs_stildb_get_node(t_xs_stildb *, gchar *);

#ifdef __cplusplus
}
#endif
#endif /* XS_STIL_H */
