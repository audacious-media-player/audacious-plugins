#ifndef XS_CONFIG_H
#define XS_CONFIG_H

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
    int     audioChannels;
    int     audioFrequency;

    /* Emulation settings */
    bool    mos8580;            /* true = 8580, false = 6581 */
    bool    forceModel;
    int     clockSpeed;         /* PAL (50Hz) or NTSC (60Hz) */
    bool    forceSpeed;         /* true = force to given clockspeed */

    bool    emulateFilters;

    /* Playing settings */
    bool    playMaxTimeEnable,
            playMaxTimeUnknown; /* Use max-time only when song-length is unknown */
    int     playMaxTime;        /* MAX playtime in seconds */

    bool    playMinTimeEnable;
    int     playMinTime;        /* MIN playtime in seconds */

    /* Miscellaneous settings */
    bool    subAutoEnable,
            subAutoMinOnly;
    int     subAutoMinTime;
} xs_cfg;


/* Functions
 */
void xs_init_configuration();

struct PluginPreferences;
extern const PluginPreferences sid_prefs;

#endif    /* XS_CONFIG_H */
