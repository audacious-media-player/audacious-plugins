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
    .description = "ESD Output Plugin",
    .probe_priority = 2,
    .init = esdout_init,
    .about = esdout_about,
    .configure = esdout_configure,
    .get_volume = esdout_get_volume,
    .set_volume = esdout_set_volume,
    .open_audio = esdout_open,
    .write_audio = esdout_write,
    .close_audio = esdout_close,
    .flush = esdout_flush,
    .pause = esdout_pause,
    .buffer_free = esdout_free,
    .buffer_playing = esdout_playing,
    .output_time = esdout_get_output_time,
    .written_time = esdout_get_written_time,
    .tell_audio = esdout_tell
};

OutputPlugin *esd_oplist[] = { &esd_op, NULL };

DECLARE_PLUGIN(esd, NULL, NULL, NULL, esd_oplist, NULL, NULL, NULL, NULL);
