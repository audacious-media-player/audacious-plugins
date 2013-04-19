#ifndef XS_SIDPLAYFP_H
#define XS_SIDPLAYFP_H

#include "xs_player.h"
#include "xs_support.h"
#include "xs_slsup.h"

#ifdef __cplusplus
extern "C" {
#endif

gboolean    xs_sidplayfp_probe(VFSFile *);
void        xs_sidplayfp_close(xs_status_t *);
gboolean    xs_sidplayfp_init(xs_status_t *);
gboolean    xs_sidplayfp_initsong(xs_status_t *);
guint        xs_sidplayfp_fillbuffer(xs_status_t *, gchar *, guint);
gboolean    xs_sidplayfp_load(xs_status_t *, const gchar *);
void        xs_sidplayfp_delete(xs_status_t *);
xs_tuneinfo_t*    xs_sidplayfp_getinfo(const gchar *);
gboolean    xs_sidplayfp_updateinfo(xs_status_t *);

#ifdef __cplusplus
}
#endif
#endif /* XS_SIDPLAYFP_H */
