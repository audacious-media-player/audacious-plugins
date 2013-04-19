#ifndef XS_SUPPORT_H
#define XS_SUPPORT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <glib.h>
#include <libaudcore/vfs.h>


/* VFS replacement functions
 */
guint16 xs_fread_be16(VFSFile *);
guint32 xs_fread_be32(VFSFile *);


/* Misc functions
 */
gint    xs_pstrcpy(gchar **, const gchar *);
gint    xs_pstrcat(gchar **, const gchar *);
void    xs_findnext(const gchar *, size_t *);
void    xs_findeol(const gchar *, size_t *);
void    xs_findnum(const gchar *, size_t *);

#ifdef __cplusplus
}
#endif
#endif /* XS_SUPPORT_H */
