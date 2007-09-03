#ifndef XS_SLSUP_H
#define XS_SLSUP_H

#include "xmms-sid.h"
#include "xs_stil.h"
#include "xs_length.h"

#ifdef __cplusplus
extern "C" {
#endif

gint		xs_stil_init(void);
void		xs_stil_close(void);
t_xs_stil_node *xs_stil_get(gchar *pcFilename);

gint		xs_songlen_init(void);
void		xs_songlen_close(void);
t_xs_sldb_node *xs_songlen_get(const gchar *);


#ifdef __cplusplus
}
#endif
#endif /* XS_SLSUP_H */
