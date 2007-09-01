#ifndef XS_SUPPORT_H
#define XS_SUPPORT_H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>
#include <stdio.h>

#ifdef AUDACIOUS_PLUGIN
#include <audacious/plugin.h>
#include <audacious/output.h>
#include <audacious/util.h>
#include <audacious/tuple.h>
#define HAVE_MEMSET
#else
#include <xmms/plugin.h>
#include <xmms/util.h>
#include <xmms/titlestring.h>
#endif

#ifdef HAVE_ASSERT_H
#include <assert.h>
#else
#define assert(x) /* stub */
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#else
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#endif

#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif


/* Metadata structures */
#ifdef AUDACIOUS_PLUGIN
#define t_xs_tuple Tuple
#else
#define t_xs_tuple TitleInput
#endif

/* VFS replacement functions
 */
#ifdef __AUDACIOUS_NEWVFS__
#include <audacious/vfs.h>
#define t_xs_file VFSFile
#define xs_fopen(a,b) vfs_fopen(a,b)
#define xs_fclose(a) vfs_fclose(a)
#define xs_fgetc(a) vfs_getc(a)
#define xs_fread(a,b,c,d) vfs_fread(a,b,c,d)
#define xs_feof(a) vfs_feof(a)
#define xs_ferror(a) (0)
#define xs_ftell(a) vfs_ftell(a)
#define xs_fseek(a,b,c) vfs_fseek(a,b,c)
#else
#define t_xs_file FILE
t_xs_file *xs_fopen(const gchar *, const gchar *);
gint	xs_fclose(t_xs_file *);
gint	xs_fgetc(t_xs_file *);
size_t	xs_fread(void *, size_t, size_t, t_xs_file *);
gint	xs_feof(t_xs_file *);
gint	xs_ferror(t_xs_file *);
glong	xs_ftell(t_xs_file *);
gint	xs_fseek(t_xs_file *, glong, gint);
#endif
guint16 xs_fread_be16(t_xs_file *);
guint32 xs_fread_be32(t_xs_file *);
gint	xs_fload_buffer(gchar *, guint8 **, size_t *);


/* Misc functions
 */
gchar	*xs_strncpy(gchar *, gchar *, size_t);
gint	xs_pstrcpy(gchar **, const gchar *);
gint	xs_pstrcat(gchar **, const gchar *);
void	xs_pnstrcat(gchar *, size_t, gchar *);
gchar	*xs_strrchr(gchar *, gchar);
void	xs_findnext(gchar *, size_t *);
void	xs_findeol(gchar *, size_t *);
void	xs_findnum(gchar *, size_t *);

#ifdef HAVE_MEMSET
#define	xs_memset memset
#else
void	*xs_memset(void *, int, size_t);
#endif

#ifdef __cplusplus
}
#endif
#endif /* XS_SUPPORT_H */
