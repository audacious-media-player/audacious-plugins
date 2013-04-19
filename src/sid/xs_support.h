#ifndef XS_SUPPORT_H
#define XS_SUPPORT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/types.h>
#include <libaudcore/vfs.h>

/* VFS replacement functions
 */
uint16_t xs_fread_be16(VFSFile *);
uint32_t xs_fread_be32(VFSFile *);


/* Misc functions
 */
int xs_pstrcpy(char **, const char *);
int xs_pstrcat(char **, const char *);
void xs_findnext(const char *, size_t *);
void xs_findeol(const char *, size_t *);
void xs_findnum(const char *, size_t *);

#ifdef __cplusplus
}
#endif
#endif /* XS_SUPPORT_H */
