#ifndef XS_CONFIG_H
#define XS_CONFIG_H

#include "xmms-sid.h"

#ifdef __cplusplus
extern "C" {
#endif


/* Configuration structure
 */
enum XS_CHANNELS {
    XS_CHN_MONO = 1,
    XS_CHN_STEREO = 2
};


enum XS_CLOCK {
    XS_CLOCK_PAL = 1,
    XS_CLOCK_NTSC,
    XS_CLOCK_VBI,
    XS_CLOCK_CIA,
    XS_CLOCK_ANY
};


enum XS_SIDMODEL {
    XS_SIDMODEL_UNKNOWN = 0,
    XS_SIDMODEL_6581,
    XS_SIDMODEL_8580,
    XS_SIDMODEL_ANY
};


extern struct xs_cfg_t {
    /* General audio settings */
    gint        audioChannels;
    gint        audioFrequency;

    /* Emulation settings */
    gboolean    mos8580;            /* TRUE = 8580, FALSE = 6581 */
    gboolean    forceModel;
    gint        clockSpeed;         /* PAL (50Hz) or NTSC (60Hz) */
    gboolean    forceSpeed;         /* TRUE = force to given clockspeed */

    gboolean    emulateFilters;

    /* Playing settings */
    gboolean    playMaxTimeEnable,
                playMaxTimeUnknown; /* Use max-time only when song-length is unknown */
    gint        playMaxTime;        /* MAX playtime in seconds */

    gboolean    playMinTimeEnable;
    gint        playMinTime;        /* MIN playtime in seconds */

    gboolean    songlenDBEnable;
    gchar       *songlenDBPath;     /* Path to Songlengths.txt */

    /* Miscellaneous settings */
    gboolean    stilDBEnable;
    gchar       *stilDBPath;        /* Path to STIL.txt */
    gchar       *hvscPath;          /* Path-prefix for HVSC */

    gboolean    subAutoEnable,
                subAutoMinOnly;
    gint        subAutoMinTime;
} xs_cfg;

XS_MUTEX_H(xs_cfg);


/* Configuration-file
 */
enum {
    CTYPE_INT = 1,
    CTYPE_FLOAT,
    CTYPE_STR,
    CTYPE_BOOL
};

enum {
    WTYPE_BGROUP = 1,
    WTYPE_SPIN,
    WTYPE_SCALE,
    WTYPE_BUTTON,
    WTYPE_TEXT,
    WTYPE_COMBO
};


typedef struct {
    gint    itemType;   /* Type of item (CTYPE_*) */
    void    *itemData;  /* Pointer to variable */
    gchar   *itemName;  /* Name of configuration item */
} xs_cfg_item_t;


typedef struct {
    gint    widType;
    gint    itemType;
    gchar   *widName;
    void    *itemData;
    gint    itemSet;
} xs_wid_item_t;


/* Functions
 */
void    xs_init_configuration(void);

#ifdef __cplusplus
}
#endif
#endif    /* XS_CONFIG_H */
