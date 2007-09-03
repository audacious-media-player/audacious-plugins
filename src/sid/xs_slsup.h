#ifndef XS_SLSUP_H
#define XS_SLSUP_H

#include "xmms-sid.h"
#include "xs_stil.h"
#include "xs_length.h"

#ifdef __cplusplus
extern "C" {
#endif

gint		xs_stil_init(void);
void		xs_stil_close(void);
t_xs_stil_node *xs_stil_get(gchar *pcFilename);

gint		xs_songlen_init(void);
void		xs_songlen_close(void);
t_xs_sldb_node *xs_songlen_get(const gchar *);

t_xs_tuneinfo *xs_tuneinfo_new(const gchar * pcFilename,
		gint nsubTunes, gint startTune, const gchar * sidName,
		const gchar * sidComposer, const gchar * sidCopyright,
		gint loadAddr, gint initAddr, gint playAddr,
		gint dataFileLen, const gchar *sidFormat, gint sidModel);
void	xs_tuneinfo_free(t_xs_tuneinfo *);


#ifdef __cplusplus
}
#endif
#endif /* XS_SLSUP_H */
