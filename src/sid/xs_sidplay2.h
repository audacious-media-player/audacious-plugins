#ifndef XS_SIDPLAY2_H
#define XS_SIDPLAY2_H

#include "xs_player.h"
#include "xs_support.h"
#include "xs_slsup.h"

#ifdef __cplusplus
extern "C" {
#endif

gboolean	xs_sidplay2_probe(t_xs_file *);
void		xs_sidplay2_close(t_xs_status *);
gboolean	xs_sidplay2_init(t_xs_status *);
gboolean	xs_sidplay2_initsong(t_xs_status *);
guint		xs_sidplay2_fillbuffer(t_xs_status *, gchar *, guint);
gboolean	xs_sidplay2_load(t_xs_status *, gchar *);
void		xs_sidplay2_delete(t_xs_status *);
t_xs_tuneinfo*	xs_sidplay2_getinfo(const gchar *);
gboolean	xs_sidplay2_updateinfo(t_xs_status *);
void		xs_sidplay2_flush(t_xs_status *);

#ifdef __cplusplus
}
#endif
#endif /* XS_SIDPLAY2_H */
