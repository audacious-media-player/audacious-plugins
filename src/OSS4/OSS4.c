/*  BMP - Cross-platform multimedia player
 *  Copyright (C) 2003-2004  BMP development team.
 *
 *  Based on XMMS:
 *  Copyright (C) 1998-2003  XMMS development team.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "OSS4.h"

#include <glib.h>
#include <audacious/plugin.h>
#include <audacious/i18n.h>
#include <stdlib.h>

OSSConfig oss_cfg;

static void oss_about(void)
{
    static GtkWidget *dialog;

    if (dialog != NULL)
        return;

    dialog = audacious_info_dialog(_("About OSSv4 Driver"),
		    _("Audacious OSSv4 Driver\n\n"
			    "Based on the OSSv3 Output plugin,\n" 
			    "Ported to OSSv4's VMIX by Cristi Magherusan <majeru@gentoo.ro>\n\n"
			    "This program is free software; you can redistribute it and/or modify\n"
			    "it under the terms of the GNU General Public License as published by\n"
			    "the Free Software Foundation; either version 2 of the License, or\n"
			    "(at your option) any later version.\n"
			    "\n"
			    "This program is distributed in the hope that it will be useful,\n"
			    "but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
			    "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n"
			    "GNU General Public License for more details.\n"
			    "\n"
			    "You should have received a copy of the GNU General Public License\n"
			    "along with this program; if not, write to the Free Software\n"
			    "Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301,\n"
			    "USA.\n\n"
			    "Note: For any issues regarding this plugin (including patches and \n"
			    "suggestions) please contact the maintainer, and NO other \n"
			    "Audacious developers\n\n"), _("Ok"), FALSE, NULL, NULL);
    g_signal_connect(G_OBJECT(dialog), "destroy",
                     G_CALLBACK(gtk_widget_destroyed), &dialog);
}

static OutputPluginInitStatus oss_init(void)
{
    mcs_handle_t *db;

    memset(&oss_cfg, 0, sizeof(OSSConfig));

    oss_cfg.audio_device = 0;
    oss_cfg.buffer_size = 3000;
    oss_cfg.prebuffer = 25;
    oss_cfg.use_alt_audio_device = FALSE;
    oss_cfg.alt_audio_device = NULL;

    if ((db = aud_cfg_db_open())) {
        aud_cfg_db_get_int(db, "OSS", "audio_device", &oss_cfg.audio_device);
        aud_cfg_db_get_int(db, "OSS", "buffer_size", &oss_cfg.buffer_size);
        aud_cfg_db_get_int(db, "OSS", "prebuffer", &oss_cfg.prebuffer);
        aud_cfg_db_get_bool(db, "OSS", "save_volume", &oss_cfg.save_volume);
        aud_cfg_db_get_bool(db, "OSS", "use_alt_audio_device",
                            &oss_cfg.use_alt_audio_device);
        aud_cfg_db_get_string(db, "OSS", "alt_audio_device",
                              &oss_cfg.alt_audio_device);
        aud_cfg_db_get_int(db, "OSS", "saved_volume", &vol);
        aud_cfg_db_close(db);
    }
    
    //volume gets saved anyway, but is ignored unless "saved_volume" is true
    if(!oss_cfg.save_volume)
        vol = 0x6464;  //maximum
            
    if (!oss_hardware_present())
    {
        return OUTPUT_PLUGIN_INIT_NO_DEVICES;
    }
    else
    {
        return OUTPUT_PLUGIN_INIT_FOUND_DEVICES;
    }
}

static void oss_cleanup(void)
{
    mcs_handle_t *db;
    db = aud_cfg_db_open();
    aud_cfg_db_set_int(db, "OSS", "saved_volume", vol);
    aud_cfg_db_close(db);

    if (oss_cfg.alt_audio_device) {
        g_free(oss_cfg.alt_audio_device);
        oss_cfg.alt_audio_device = NULL;
    }
}

static OutputPlugin oss4_op = {
    .description = "OSS4 Output Plugin",
    .probe_priority = 1,
    .init = oss_init,
    .cleanup = oss_cleanup,
    .about = oss_about,
    .configure = oss_configure,
    .get_volume = oss_get_volume,
    .set_volume = oss_set_volume,
    .open_audio = oss_open,
    .write_audio = oss_write,
    .close_audio = oss_close,
    .flush = oss_flush,
    .pause = oss_pause,
    .buffer_free = oss_free,
    .buffer_playing = oss_playing,
    .output_time = oss_get_output_time,
    .written_time = oss_get_written_time,
    .tell_audio = oss_tell
};

static OutputPlugin *oss4_oplist[] = { &oss4_op, NULL };

DECLARE_PLUGIN(OSS4, NULL, NULL, NULL, oss4_oplist, NULL, NULL, NULL, NULL);
