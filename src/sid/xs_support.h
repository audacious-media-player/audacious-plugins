#ifndef XS_SUPPORT_H
#define XS_SUPPORT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <glib.h>

#include <audacious/plugin.h>
#include <audacious/i18n.h>

#include <assert.h>
#include <string.h>


/* VFS replacement functions
 */
#define xs_file_t VFSFile
#define xs_fopen(a,b) vfs_fopen(a,b)
#define xs_fclose(a) vfs_fclose(a)
#define xs_fgetc(a) vfs_getc(a)
#define xs_fread(a,b,c,d) vfs_fread(a,b,c,d)
#define xs_feof(a) vfs_feof(a)
#define xs_ferror(a) (0)
#define xs_ftell(a) vfs_ftell(a)
#define xs_fseek(a,b,c) vfs_fseek(a,b,c)

guint16 xs_fread_be16(xs_file_t *);
guint32 xs_fread_be32(xs_file_t *);
gint    xs_fload_buffer(const gchar *, guint8 **, size_t *);


/* Misc functions
 */
gchar    *xs_strncpy(gchar *, const gchar *, size_t);
gint    xs_pstrcpy(gchar **, const gchar *);
gint    xs_pstrcat(gchar **, const gchar *);
void    xs_pnstrcat(gchar *, size_t, const gchar *);
gchar    *xs_strrchr(gchar *, const gchar);
void    xs_findnext(const gchar *, size_t *);
void    xs_findeol(const gchar *, size_t *);
void    xs_findnum(const gchar *, size_t *);

#ifdef __cplusplus
}
#endif
#endif /* XS_SUPPORT_H */
