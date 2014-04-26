#ifndef XS_SLSUP_H
#define XS_SLSUP_H

#include "xmms-sid.h"
#include "xs_stil.h"
#include "xs_length.h"

int xs_stil_init(void);
void xs_stil_close(void);
stil_node_t *xs_stil_get(char *filename);

int xs_songlen_init(void);
void xs_songlen_close(void);
sldb_node_t *xs_songlen_get(const char *filename);

xs_tuneinfo_t *xs_tuneinfo_new(const char *pcFilename, int nsubTunes,
 int startTune, const char *sidName, const char *sidComposer,
 const char *sidCopyright, int loadAddr, int initAddr, int playAddr,
 int dataFileLen, const char *sidFormat, int sidModel);
void xs_tuneinfo_free(xs_tuneinfo_t *tune);

#endif /* XS_SLSUP_H */
