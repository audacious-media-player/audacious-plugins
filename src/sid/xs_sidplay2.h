#ifndef XS_SIDPLAYFP_H
#define XS_SIDPLAYFP_H

#include "xs_slsup.h"

#include <libaudcore/vfs.h>

bool xs_sidplayfp_probe(VFSFile *);
void xs_sidplayfp_close();
bool xs_sidplayfp_init();
bool xs_sidplayfp_initsong(int subtune);
unsigned xs_sidplayfp_fillbuffer(char *, unsigned);
bool xs_sidplayfp_load(const char *);
void xs_sidplayfp_delete();
xs_tuneinfo_t *xs_sidplayfp_getinfo(const char *);
bool xs_sidplayfp_updateinfo(xs_tuneinfo_t *i, int subtune);

#endif /* XS_SIDPLAYFP_H */
