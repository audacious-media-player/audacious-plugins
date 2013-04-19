#include "xs_player.h"
#include "xs_sidplay2.h"


/* List of emulator engines
 */
static const xs_engine_t xs_enginelist[] = {
    {XS_ENG_SIDPLAY2,
     xs_sidplayfp_probe,
     xs_sidplayfp_init, xs_sidplayfp_close,
     xs_sidplayfp_initsong, xs_sidplayfp_fillbuffer,
     xs_sidplayfp_load, xs_sidplayfp_delete,
     xs_sidplayfp_getinfo, xs_sidplayfp_updateinfo,
     xs_sidplayfp_flush
    },
};

static const gint xs_nenginelist = (sizeof(xs_enginelist) / sizeof(xs_enginelist[0]));


gboolean xs_init_emu_engine(int *configured, xs_status_t *status)
{
    gint engine;
    gboolean initialized;

    engine = 0;
    initialized = FALSE;
    while (engine < xs_nenginelist && !initialized) {
        if (xs_enginelist[engine].plrIdent == *configured) {
            if (xs_enginelist[engine].plrInit(status)) {
                initialized = TRUE;
                status->sidPlayer = (xs_engine_t *) & xs_enginelist[engine];
            }
        }
        engine++;
    }

    engine = 0;
    while (engine < xs_nenginelist && !initialized) {
        if (xs_enginelist[engine].plrInit(status)) {
            initialized = TRUE;
            status->sidPlayer = (xs_engine_t *) &xs_enginelist[engine];
            *configured = xs_enginelist[engine].plrIdent;
        } else
            engine++;
    }

    return initialized;
}

