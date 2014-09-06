#ifndef XS_SLSUP_H
#define XS_SLSUP_H

#include "xmms-sid.h"

xs_tuneinfo_t *xs_tuneinfo_new(const char *pcFilename, int nsubTunes,
 int startTune, const char *sidName, const char *sidComposer,
 const char *sidCopyright, int loadAddr, int initAddr, int playAddr,
 int dataFileLen, const char *sidFormat, int sidModel);
void xs_tuneinfo_free(xs_tuneinfo_t *tune);

#endif /* XS_SLSUP_H */
