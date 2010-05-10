//all.h:

/*
 *      Copyright (C) Philipp 'ph3-der-loewe' Schafft    - 2008,
 *                    Daniel Duntemann <dauxx@dauxx.org> - 2009
 *
 *  This file is part of the XMMS RoarAudio output plugin a part of RoarAudio,
 *  a cross-platform sound system for both, home and professional use.
 *  See README for details.
 *
 *  This file is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 3
 *  as published by the Free Software Foundation.
 *
 *  RoarAudio is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this software; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 *
 */

#ifndef _ALL_H_
#define _ALL_H_

#include <roaraudio.h>

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <stdio.h>
#include <string.h>

#include <audacious/configdb.h>
#include <audacious/util.h>
#include <audacious/plugin.h>
#include <audacious/audctrl.h>

#include <libaudgui/libaudgui.h>
#include <libaudgui/libaudgui-gtk.h>


#define _(x) (x)

gint ctrlsocket_get_session_id(void);

OutputPluginInitStatus roar_init(void);
void roar_about(void);
void roar_configure(void);

void roar_get_volume(int *l, int *r);
void roar_set_volume(int l, int r);
void roar_mixer_init(void);
void roar_mixer_init_vol(int l, int r);

int roar_playing(void);
int roar_free(void);
void roar_write(void *ptr, int length);
void roar_close(void);
void roar_flush(int time);
void roar_pause(short p);
int roar_open(AFormat fmt, int rate, int nch);
int roar_get_output_time(void);
int roar_get_written_time(void);

int roar_update_metadata(void);
int roar_chk_metadata(void);

#define STATE_CONNECTED   1
#define STATE_PLAYING     2
#define STATE_NORECONNECT 4

struct xmms_roar_out {
 int                 state;
 char              * server;
 struct roar_connection con;
 struct roar_stream     stream;
 int                 data_fh;
 long unsigned int   written;
 long unsigned int   bps;
 int                 session;
 int                 pause;
 struct {
  int                server_type;
  int                port;
  int              * proxy_type;
  char             * proxy;
  char             * player_name;
 } cfg;
} g_inst;

#endif

//ll
