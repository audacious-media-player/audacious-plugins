#ifndef XS_SIDPLAYFP_H
#define XS_SIDPLAYFP_H

#include "xs_player.h"
#include "xs_support.h"
#include "xs_slsup.h"

bool_t    xs_sidplayfp_probe(VFSFile *);
void        xs_sidplayfp_close(xs_status_t *);
bool_t    xs_sidplayfp_init(xs_status_t *);
bool_t    xs_sidplayfp_initsong(xs_status_t *);
unsigned        xs_sidplayfp_fillbuffer(xs_status_t *, char *, unsigned);
bool_t    xs_sidplayfp_load(xs_status_t *, const char *);
void        xs_sidplayfp_delete(xs_status_t *);
xs_tuneinfo_t*    xs_sidplayfp_getinfo(const char *);
bool_t    xs_sidplayfp_updateinfo(xs_status_t *);

#endif /* XS_SIDPLAYFP_H */
