/*    xmms - esound output plugin
 *    Copyright (C) 1999      Galex Yen
 *      
 *      this program is free software
 *      
 *      Description:
 *              This program is an output plugin for xmms v0.9 or greater.
 *              The program uses the esound daemon to output audio in order
 *              to allow more than one program to play audio on the same
 *              device at the same time.
 *
 *              Contains code Copyright (C) 1998-1999 Mikael Alm, Olle Hallnas,
 *              Thomas Nillson and 4Front Technologies
 *
 */

#include "esdout.h"

#include <glib.h>
#include <audacious/i18n.h>


OutputPlugin esd_op = {
    NULL,
    NULL,
    "ESD Output Plugin",
    esdout_init,
    NULL,
    esdout_about,
    esdout_configure,
    esdout_get_volume,
    esdout_set_volume,
    esdout_open,
    esdout_write,
    esdout_close,
    esdout_flush,
    esdout_pause,
    esdout_free,
    esdout_playing,
    esdout_get_output_time,
    esdout_get_written_time,
    esdout_tell
};

OutputPlugin *esd_oplist[] = { &esd_op, NULL };

DECLARE_PLUGIN(esd, NULL, NULL, NULL, esd_oplist, NULL, NULL, NULL, NULL);
