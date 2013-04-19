#ifndef XS_PLAYER_H
#define XS_PLAYER_H

#include "xmms-sid.h"
#include "xs_config.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct xs_status_t {
    gint        audioFrequency,     /* Audio settings */
                audioChannels;
    void        *sidEngine;         /* SID-emulation internal engine data */
    gboolean    isPaused,
                isInitialized;
    gboolean stop_flag;
    gint        currSong,           /* Current sub-tune */
                lastTime;

    xs_tuneinfo_t *tuneInfo;
} xs_status_t;


/* Global variables
 */
extern xs_status_t    xs_status;
XS_MUTEX_H(xs_status);

#ifdef __cplusplus
}
#endif
#endif /* XS_PLAYER_H */
