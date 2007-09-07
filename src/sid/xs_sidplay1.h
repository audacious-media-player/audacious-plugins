#ifndef XS_SIDPLAY1_H
#define XS_SIDPLAY1_H

#include "xs_player.h"
#include "xs_support.h"
#include "xs_slsup.h"

#ifdef __cplusplus
extern "C" {
#endif

gboolean	xs_sidplay1_probe(t_xs_file *);
void		xs_sidplay1_close(t_xs_status *);
gboolean	xs_sidplay1_init(t_xs_status *);
gboolean	xs_sidplay1_initsong(t_xs_status *);
guint		xs_sidplay1_fillbuffer(t_xs_status *, gchar *, guint);
gboolean	xs_sidplay1_load(t_xs_status *, gchar *);
void		xs_sidplay1_delete(t_xs_status *);
t_xs_tuneinfo*	xs_sidplay1_getinfo(const gchar *);
gboolean	xs_sidplay1_updateinfo(t_xs_status *);

#ifdef __cplusplus
}
#endif
#endif /* XS_SIDPLAY1_H */
