#ifndef XS_SIDPLAY2_H
#define XS_SIDPLAY2_H

#include "xs_player.h"
#include "xs_support.h"
#include "xs_slsup.h"

#ifdef __cplusplus
extern "C" {
#endif

gboolean    xs_sidplay2_probe(xs_file_t *);
void        xs_sidplay2_close(xs_status_t *);
gboolean    xs_sidplay2_init(xs_status_t *);
gboolean    xs_sidplay2_initsong(xs_status_t *);
guint        xs_sidplay2_fillbuffer(xs_status_t *, gchar *, guint);
gboolean    xs_sidplay2_load(xs_status_t *, const gchar *);
void        xs_sidplay2_delete(xs_status_t *);
xs_tuneinfo_t*    xs_sidplay2_getinfo(const gchar *);
gboolean    xs_sidplay2_updateinfo(xs_status_t *);
void        xs_sidplay2_flush(xs_status_t *);

#ifdef __cplusplus
}
#endif
#endif /* XS_SIDPLAY2_H */
