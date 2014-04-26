#ifndef XS_PLAYER_H
#define XS_PLAYER_H

#include "xmms-sid.h"
#include "xs_config.h"

typedef struct xs_status_t {
    int        audioFrequency,     /* Audio settings */
                audioChannels;
    void        *sidEngine;         /* SID-emulation internal engine data */
    int        currSong;           /* Current sub-tune */

    xs_tuneinfo_t *tuneInfo;
} xs_status_t;


/* Global variables
 */
extern xs_status_t    xs_status;
extern pthread_mutex_t xs_status_mutex;

#endif /* XS_PLAYER_H */
