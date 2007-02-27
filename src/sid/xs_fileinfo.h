#ifndef XS_FILEINFO_H
#define XS_FILEINFO_H

#include "xmms-sid.h"

#ifdef __cplusplus
extern "C" {
#endif

gint	xs_stil_init(void);
void	xs_stil_close(void);
void	xs_fileinfo_update(void);
void	xs_fileinfo(gchar *);

#ifdef __cplusplus
}
#endif
#endif /* XS_FILEINFO_H */
