#include "xs_player.h"

#ifdef HAVE_SIDPLAY1
#include "xs_sidplay1.h"
#endif
#ifdef HAVE_SIDPLAY2
#include "xs_sidplay2.h"
#endif


/* List of emulator engines
 */
static const xs_engine_t xs_enginelist[] = {
#ifdef HAVE_SIDPLAY1
    {XS_ENG_SIDPLAY1,
     xs_sidplay1_probe,
     xs_sidplay1_init, xs_sidplay1_close,
     xs_sidplay1_initsong, xs_sidplay1_fillbuffer,
     xs_sidplay1_load, xs_sidplay1_delete,
     xs_sidplay1_getinfo, xs_sidplay1_updateinfo,
     NULL
    },
#endif
#ifdef HAVE_SIDPLAY2
    {XS_ENG_SIDPLAY2,
     xs_sidplay2_probe,
     xs_sidplay2_init, xs_sidplay2_close,
     xs_sidplay2_initsong, xs_sidplay2_fillbuffer,
     xs_sidplay2_load, xs_sidplay2_delete,
     xs_sidplay2_getinfo, xs_sidplay2_updateinfo,
     xs_sidplay2_flush
    },
#endif
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

