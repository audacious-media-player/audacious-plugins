#ifndef XS_SUPPORT_H
#define XS_SUPPORT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <glib.h>
#include <stdio.h>

#ifdef AUDACIOUS_PLUGIN
#include <audacious/plugin.h>
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


/* Standard gettext macros
 */
#ifdef ENABLE_NLS
#  include <libintl.h>
#  undef _
#  define _(String) dgettext (PACKAGE, String)
#  ifdef gettext_noop
#    define N_(String) gettext_noop (String)
#  else
#    define N_(String) (String)
#  endif
#else
#  define _LIBINTL_H
#  define textdomain(String) (String)
#  define gettext(String) (String)
#  define dgettext(Domain,Message) (Message)
#  define dcgettext(Domain,Message,Type) (Message)
#  define bindtextdomain(Domain,Directory) (Domain)
#  define _(String) (String)
#  define N_(String) (String)
#endif


/* VFS replacement functions
 */
#ifdef AUDACIOUS_PLUGIN
#define xs_file_t VFSFile
#define xs_fopen(a,b) vfs_fopen(a,b)
#define xs_fclose(a) vfs_fclose(a)
#define xs_fgetc(a) vfs_getc(a)
#define xs_fread(a,b,c,d) vfs_fread(a,b,c,d)
#define xs_feof(a) vfs_feof(a)
#define xs_ferror(a) (0)
#define xs_ftell(a) vfs_ftell(a)
#define xs_fseek(a,b,c) vfs_fseek(a,b,c)
#else
#define xs_file_t FILE
#define xs_fopen(a,b) fopen(a,b)
#define xs_fclose(a) fclose(a)
#define xs_fgetc(a) fgetc(a)
#define xs_fread(a,b,c,d) fread(a,b,c,d)
#define xs_feof(a) feof(a)
#define xs_ferror(a) ferror(a)
#define xs_ftell(a) ftell(a)
#define xs_fseek(a,b,c) fseek(a,b,c)
#endif
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
