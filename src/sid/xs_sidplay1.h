#ifndef XS_SIDPLAY1_H
#define XS_SIDPLAY1_H

#include "xs_player.h"
#include "xs_support.h"
#include "xs_slsup.h"

#ifdef __cplusplus
extern "C" {
#endif

gboolean    xs_sidplay1_probe(xs_file_t *);
void        xs_sidplay1_close(xs_status_t *);
gboolean    xs_sidplay1_init(xs_status_t *);
gboolean    xs_sidplay1_initsong(xs_status_t *);
guint        xs_sidplay1_fillbuffer(xs_status_t *, gchar *, guint);
gboolean    xs_sidplay1_load(xs_status_t *, const gchar *);
void        xs_sidplay1_delete(xs_status_t *);
xs_tuneinfo_t*    xs_sidplay1_getinfo(const gchar *);
gboolean    xs_sidplay1_updateinfo(xs_status_t *);

#ifdef __cplusplus
}
#endif
#endif /* XS_SIDPLAY1_H */
