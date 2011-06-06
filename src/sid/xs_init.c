/*
   XMMS-SID - SIDPlay input plugin for X MultiMedia System (XMMS)

   Plugin initialization point

   Programmed and designed by Matti 'ccr' Hamalainen <ccr@tnsp.org>
   (C) Copyright 1999-2007 Tecnic Software productions (TNSP)

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License along
   with this program; if not, write to the Free Software Foundation, Inc.,
   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/
#include "xmms-sid.h"
#include "xs_config.h"
#include "xs_fileinfo.h"

static const gchar *xs_sid_fmts[] = { "sid", "psid", NULL };

AUD_INPUT_PLUGIN
(
    .name = XS_PACKAGE_STRING,          /* Plugin description */
    .init = xs_init,                    /* Initialization */
    .cleanup = xs_close,                /* Cleanup */
#if 0
    .about = xs_about,                  /* Show aboutbox */
    .configure = xs_configure,          /* Show/edit configuration */
#endif

    .play = xs_play_file,               /* Play given file */
    .stop = xs_stop,                    /* Stop playing */
    .pause = xs_pause,                  /* Pause playing */

#if 0
    .file_info_box = xs_fileinfo,       /* Show file-information dialog */
#endif
    .probe_for_tuple = xs_probe_for_tuple,

    .extensions = xs_sid_fmts,          /* File ext assist */
    .have_subtune = TRUE
)
