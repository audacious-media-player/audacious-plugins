#ifndef XS_CONFIG_H
#define XS_CONFIG_H

#include "xmms-sid.h"

#ifdef __cplusplus
extern "C" {
#endif


/* Configuration structure
 */
enum XS_EMUENGINE {
    XS_ENG_SIDPLAY2
};


enum XS_BUILDER {
    XS_BLD_RESID = 1,
    XS_BLD_HARDSID
};

enum XS_RESOLUTION {
    XS_RES_8BIT = 8,
    XS_RES_16BIT = 16
};

enum XS_CHANNELS {
    XS_CHN_MONO = 1,
    XS_CHN_STEREO = 2,
    XS_CHN_AUTOPAN = 3
};


enum XS_CLOCK {
    XS_CLOCK_PAL = 1,
    XS_CLOCK_NTSC,
    XS_CLOCK_VBI,
    XS_CLOCK_CIA,
    XS_CLOCK_ANY
};


enum XS_MPU {
    XS_MPU_BANK_SWITCHING = 1,
    XS_MPU_TRANSPARENT_ROM,
    XS_MPU_PLAYSID_ENVIRONMENT,
    XS_MPU_REAL
};


enum XS_SSC {
    XS_SSC_NONE = 1,
    XS_SSC_POPUP,
    XS_SSC_PATCH,
    XS_SSC_SEEK
};


enum XS_SIDMODEL {
    XS_SIDMODEL_UNKNOWN = 0,
    XS_SIDMODEL_6581,
    XS_SIDMODEL_8580,
    XS_SIDMODEL_ANY
};


typedef struct {
    gint x, y;
} xs_int_point_t;


typedef struct {
    gchar           *name;
    gint            type;

    /* Filter type 0 for SIDPlay 1 */
    gfloat          fs, fm, ft;

    /* Filter type 1-2 points */
    xs_int_point_t  points[XS_SIDPLAY2_NFPOINTS];
    gint            npoints;

    /* Distortion patch tunables */
    gfloat          rate, point, voice_nonlinearity,
                    cf_treshold;

    /* Filter Type 3 tunables */
    gfloat          baseresistance, offset, steepness,
                    minimumfetresistance;

    /* Filter Type 4 tunables */
    gfloat          k, b;
} xs_sid_filter_t;


extern struct xs_cfg_t {
    /* General audio settings */
    gint        audioBitsPerSample;
    gint        audioChannels;
    gint        audioFrequency;

    gboolean    oversampleEnable;
    gint        oversampleFactor;   /* Factor of oversampling */

    /* Emulation settings */
    gboolean    mos8580;            /* TRUE = 8580, FALSE = 6581 */
    gboolean    forceModel;
    gint        memoryMode;         /* See XS_MPU-constants */
    gint        clockSpeed;         /* PAL (50Hz) or NTSC (60Hz) */
    gboolean    forceSpeed;         /* TRUE = force to given clockspeed */

    gint        playerEngine;       /* Selected player engine */

    gboolean    emulateFilters;

    gint        sid2OptLevel;       /* SIDPlay2 emulation optimization */
    gint        sid2Builder;        /* SIDPlay2 "builder" aka SID-emu */
    xs_sid_filter_t    sid2Filter;  /* Current SIDPlay2 filter */
    xs_sid_filter_t    **sid2FilterPresets;
    gint        sid2NFilterPresets;


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

    gint        subsongControl;
    gboolean    detectMagic;

    gboolean    titleOverride;      /* TRUE if XMMS titles are overriden */
    gchar       *titleFormat;

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
