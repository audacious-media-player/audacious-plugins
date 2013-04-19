#include "xs_player.h"
#include "xs_sidplay2.h"


/* List of emulator engines
 */
static const xs_engine_t xs_enginelist[] = {
    {XS_ENG_SIDPLAY2,
     xs_sidplay2_probe,
     xs_sidplay2_init, xs_sidplay2_close,
     xs_sidplay2_initsong, xs_sidplay2_fillbuffer,
     xs_sidplay2_load, xs_sidplay2_delete,
     xs_sidplay2_getinfo, xs_sidplay2_updateinfo,
     xs_sidplay2_flush
    },
};

static const gint xs_nenginelist = (sizeof(xs_enginelist) / sizeof(xs_enginelist[0]));


gboolean xs_init_emu_engine(int *configured, xs_status_t *status)
{
    gint engine;
    gboolean initialized;

    XSDEBUG("initializing emulator engine #%i...\n", *configured);

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

    XSDEBUG("init#1: %s, %i\n", initialized ? "OK" : "FAILED", engine);

    engine = 0;
    while (engine < xs_nenginelist && !initialized) {
        if (xs_enginelist[engine].plrInit(status)) {
            initialized = TRUE;
            status->sidPlayer = (xs_engine_t *) &xs_enginelist[engine];
            *configured = xs_enginelist[engine].plrIdent;
        } else
            engine++;
    }

    XSDEBUG("init#2: %s, %i\n", initialized ? "OK" : "FAILED", engine);
    return initialized;
}

