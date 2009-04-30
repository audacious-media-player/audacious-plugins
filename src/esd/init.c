/*      xmms - esound output plugin
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

#include <glib.h>
#include <stdlib.h>
#include <string.h>
#include <esd.h>
#include <audacious/plugin.h>
#include "esdout.h"


ESDConfig esd_cfg;
esd_info_t *all_info;
esd_player_info_t *player_info;


OutputPluginInitStatus
esdout_init(void)
{
    mcs_handle_t *db;
    char *env;
    int lp = 80 , rp = 80;
    int fd;

    memset(&esd_cfg, 0, sizeof(ESDConfig));
    esd_cfg.port = ESD_DEFAULT_PORT;
    esd_cfg.buffer_size = 3000;
    esd_cfg.prebuffer = 25;

    db = aud_cfg_db_open();

    if ((env = getenv("ESPEAKER")) != NULL) {
        char *temp = NULL;
        esd_cfg.use_remote = TRUE;
        esd_cfg.server = g_strdup(env);
        if (esd_cfg.server[0] == '[' &&
                NULL != (temp = strchr(esd_cfg.server + 1, ']')) &&
                temp[1] == ':') {
            *temp = '\0';
            memmove(esd_cfg.server, esd_cfg.server + 1, temp - esd_cfg.server);
            temp++;
        } else if (NULL != (temp = strchr(esd_cfg.server, ':'))) {
            if (strchr(temp + 1, ':')) temp = NULL;
        }
        if (temp != NULL) {
            *temp = '\0';
            esd_cfg.port = atoi(temp + 1);
            if (esd_cfg.port == 0)
                esd_cfg.port = ESD_DEFAULT_PORT;
        }
    }
    else {
        aud_cfg_db_get_bool(db, "ESD", "use_remote", &esd_cfg.use_remote);
        aud_cfg_db_get_string(db, "ESD", "remote_host", &esd_cfg.server);
        aud_cfg_db_get_int(db, "ESD", "remote_port", &esd_cfg.port);
    }
    aud_cfg_db_get_bool(db, "ESD", "use_oss_mixer", &esd_cfg.use_oss_mixer);
    aud_cfg_db_get_int(db, "ESD", "buffer_size", &esd_cfg.buffer_size);
    aud_cfg_db_get_int(db, "ESD", "prebuffer", &esd_cfg.prebuffer);
    
    /* restore volume levels */
    aud_cfg_db_get_int(db, "ESD", "volume_left", &lp);
    aud_cfg_db_get_int(db, "ESD", "volume_right", &rp);
    esdout_set_volume(lp, rp);
    
    aud_cfg_db_close(db);

    if (!esd_cfg.server)
        esd_cfg.server = g_strdup("localhost");

    fd = esd_open_sound(esd_cfg.hostname);
    if (fd >= 0)
    {
        esd_close(fd);
        return OUTPUT_PLUGIN_INIT_FOUND_DEVICES;
    }
    else
        return OUTPUT_PLUGIN_INIT_NO_DEVICES;
}
