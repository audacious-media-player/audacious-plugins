/*
 * Audacious: Cross platform multimedia player
 * Copyright (c) 2005-2009 Audacious Team
 *
 * Driver for Game_Music_Emu library. See details at:
 * http://www.slack.net/~ant/libs/
 *
 * Preferences GUI by Giacomo Lozito
 */

#include "configure.h"
#include "plugin.h"

#include <libaudcore/runtime.h>

#define CON_CFGID "console"

AudaciousConsoleConfig audcfg;

const char * const ConsolePlugin::defaults[] = {
 "loop_length", "180",
 "resample", "FALSE",
 "resample_rate", "32000",
 "treble", "0",
 "bass", "0",
 "ignore_spc_length", "FALSE",
 "echo", "0",
 "inc_spc_reverb", "FALSE",
 nullptr};

bool ConsolePlugin::init ()
{
    aud_config_set_defaults (CON_CFGID, defaults);

    audcfg.loop_length = aud_get_int (CON_CFGID, "loop_length");
    audcfg.resample = aud_get_bool (CON_CFGID, "resample");
    audcfg.resample_rate = aud_get_int (CON_CFGID, "resample_rate");
    audcfg.treble = aud_get_int (CON_CFGID, "treble");
    audcfg.bass = aud_get_int (CON_CFGID, "bass");
    audcfg.ignore_spc_length = aud_get_bool (CON_CFGID, "ignore_spc_length");
    audcfg.echo = aud_get_int (CON_CFGID, "echo");
    audcfg.inc_spc_reverb = aud_get_bool (CON_CFGID, "inc_spc_reverb");

    return true;
}

void ConsolePlugin::cleanup ()
{
    aud_set_int (CON_CFGID, "loop_length", audcfg.loop_length);
    aud_set_bool (CON_CFGID, "resample", audcfg.resample);
    aud_set_int (CON_CFGID, "resample_rate", audcfg.resample_rate);
    aud_set_int (CON_CFGID, "treble", audcfg.treble);
    aud_set_int (CON_CFGID, "bass", audcfg.bass);
    aud_set_bool (CON_CFGID, "ignore_spc_length", audcfg.ignore_spc_length);
    aud_set_int (CON_CFGID, "echo", audcfg.echo);
    aud_set_bool (CON_CFGID, "inc_spc_reverb", audcfg.inc_spc_reverb);
}
