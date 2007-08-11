#ifndef XS_TITLE_H
#define XS_TITLE_H

#include "xmms-sid.h"

#ifdef __cplusplus
extern "C" {
#endif

gchar *xs_make_titlestring(t_xs_tuneinfo *, gint);
#ifdef AUDACIOUS_PLUGIN
t_xs_tuple *xs_make_titletuple(t_xs_tuneinfo *p, gint subTune);
#endif

#ifdef __cplusplus
}
#endif
#endif /* XS_TITLE_H */
