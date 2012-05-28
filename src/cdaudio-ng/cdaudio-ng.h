/*
 * Audacious CD Digital Audio plugin
 *
 * Copyright (c) 2007 Calin Crisan <ccrisan@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; under version 3 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses>.
 */

#ifndef CDAUDIO_NG_H
#define CDAUDIO_NG_H

#include <libaudcore/core.h>

#define DEF_STRING_LEN			256

#define MIN_DISC_SPEED 2
#define MAX_DISC_SPEED 24

typedef struct {
    bool_t use_cdtext;
    bool_t use_cddb;
    char * device;
    char * cddb_server;
    char * cddb_path;
    int cddb_port;
    bool_t cddb_http;
    int disc_speed;
    bool_t use_proxy;
    char * proxy_host;
    int proxy_port;
    char * proxy_username;
    char * proxy_password;
} cdng_cfg_t;

extern cdng_cfg_t cdng_cfg;

void configure_show_gui (void);

#endif // CDAUDIO_NG_H
