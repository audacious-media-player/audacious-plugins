/*
 * Audacious: Cross platform multimedia player
 * Copyright (c) 2005-2014 Audacious Team
 *
 * Preferences GUI by Kamil Markiewicz
 */


#include "prefs.h"

#include <audacious/misc.h>

#define MPG_CFGID "mpg123"

AudaciousMPG123Config mpg123cfg;

static const char * const mpg123_defaults[] = {
    "full_scan", "FALSE",
NULL};


bool_t mpg123_cfg_load (void)
{
    aud_config_set_defaults (MPG_CFGID, mpg123_defaults);

    mpg123cfg.full_scan = aud_get_bool (MPG_CFGID, "full_scan");

    return TRUE;
}


void mpg123_cfg_save (void)
{
    aud_set_bool (MPG_CFGID, "full_scan", mpg123cfg.full_scan);
}
