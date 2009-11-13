#ifndef XS_FILEINFO_H
#define XS_FILEINFO_H

#include "xmms-sid.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef AUDACIOUS_PLUGIN
void    xs_fileinfo_update(void);
#endif
void    xs_fileinfo(const gchar *);

#ifdef __cplusplus
}
#endif
#endif /* XS_FILEINFO_H */
