/* Audacious LIRC plugin - lirc.c

   Copyright (C) 2012 Joonas Harjumäki (jharjuma@gmail.com)

   Copyright (C) 2005 Audacious development team

   Copyright (c) 1998-1999 Carl van Schaik (carl@leg.uct.ac.za)

   Copyright (C) 2000 Christoph Bartelmus (xmms@bartelmus.de)

   some code was stolen from:
   IRman plugin for xmms by Charles Sielski (stray@teklabs.net)

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include "config.h"

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>
#include <stdlib.h>
#include <ctype.h>


#include <glib.h>
#include <gtk/gtk.h>
#include <lirc/lirc_client.h>

#include <audacious/drct.h>
#include <audacious/i18n.h>
#include <audacious/plugin.h>
#include <libaudcore/hook.h>
#include <audacious/misc.h>
#include <libaudgui/libaudgui-gtk.h>
#include <audacious/playlist.h>


#include "lirc.h"
#include "configure.h"

const char *plugin_name = "LIRC Plugin";

int lirc_fd = -1;
struct lirc_config *config = NULL;
gint tracknr = 0;
gint mute = 0;                  /* mute flag */
gint mute_vol = 0;              /* holds volume before mute */

guint input_tag;

char track_no[64];
int track_no_pos;
gint tid;


void init_lirc (void)
{
    int flags;

    if ((lirc_fd = lirc_init ("audacious", 1)) == -1)
    {
        fprintf (stderr, _("%s: could not init LIRC support\n"), plugin_name);
        return;
    }
    if (lirc_readconfig (NULL, &config, NULL) == -1)
    {
        lirc_deinit ();
        fprintf (stderr,
                 _("%s: could not read LIRC config file\n"
                   "%s: please read the documentation of LIRC\n"
                   "%s: how to create a proper config file\n"),
                 plugin_name, plugin_name, plugin_name);
        return;
    }

    input_tag =
        g_io_add_watch (g_io_channel_unix_new (lirc_fd), G_IO_IN,
                        lirc_input_callback, NULL);

    fcntl (lirc_fd, F_SETOWN, getpid ());
    flags = fcntl (lirc_fd, F_GETFL, 0);
    if (flags != -1)
    {
        fcntl (lirc_fd, F_SETFL, flags | O_NONBLOCK);
    }
    fflush (stdout);
}

gboolean init (void)
{
    load_cfg ();
    init_lirc ();
    track_no_pos = 0;
    tid = 0;
    return TRUE;
}

gboolean reconnect_lirc (gpointer data)
{
    fprintf (stderr, _("%s: trying to reconnect...\n"), plugin_name);
    init ();
    return (lirc_fd == -1);
}

void cleanup ()
{
    if (config)
    {
        if (input_tag)
            g_source_remove (input_tag);

        config = NULL;
    }
    if (lirc_fd != -1)
    {
        lirc_deinit ();
        lirc_fd = -1;
    }
    g_free (aosd_font);
}

gboolean jump_to (gpointer data)
{
    int playlist = aud_playlist_get_active ();
    aud_playlist_set_position (playlist, atoi (track_no) - 1);
    track_no_pos = 0;
    tid = 0;
    return FALSE;
}



gboolean lirc_input_callback (GIOChannel * source,
                              GIOCondition condition, gpointer data)
{
    char *code;
    char *c;
    gint playlist_time, playlist_pos, output_time, v;
    int ret;
    char *ptr;
    gint balance;
#if 0
    gboolean show_pl;
#endif
    int n;
    gchar *utf8_title_markup;

    while ((ret = lirc_nextcode (&code)) == 0 && code != NULL)
    {
        while ((ret = lirc_code2char (config, code, &c)) == 0 && c != NULL)
        {
            if (strcasecmp ("PLAY", c) == 0)
            {
                aud_drct_play ();
            }
            else if (strcasecmp ("STOP", c) == 0)
            {
                aud_drct_stop ();
            }
            else if (strcasecmp ("PAUSE", c) == 0)
            {
                aud_drct_pause ();
            }
            else if (strcasecmp ("PLAYPAUSE", c) == 0)
            {
                if (aud_drct_get_playing ())
                    aud_drct_pause ();
                else
                    aud_drct_play ();
            }
            else if (strncasecmp ("NEXT", c, 4) == 0)
            {
                ptr = c + 4;
                while (isspace (*ptr))
                    ptr++;
                n = atoi (ptr);

                if (n <= 0)
                    n = 1;
                for (; n > 0; n--)
                {
                    aud_drct_pl_next ();
                }
            }
            else if (strncasecmp ("PREV", c, 4) == 0)
            {
                ptr = c + 4;
                while (isspace (*ptr))
                    ptr++;
                n = atoi (ptr);

                if (n <= 0)
                    n = 1;
                for (; n > 0; n--)
                {
                    aud_drct_pl_prev ();
                }
            }

            else if (strcasecmp ("SHUFFLE", c) == 0)
            {
                aud_set_bool (NULL, "shuffle",
                              !aud_get_bool (NULL, "shuffle"));
            }
            else if (strcasecmp ("REPEAT", c) == 0)
            {
                aud_set_bool (NULL, "repeat", !aud_get_bool (NULL, "repeat"));
            }

            else if (strncasecmp ("FWD", c, 3) == 0)
            {
                ptr = c + 3;
                while (isspace (*ptr))
                    ptr++;
                n = atoi (ptr) * 1000;

                if (n <= 0)
                    n = 5000;
                output_time = aud_drct_get_time ();

                int playlist = aud_playlist_get_active ();
                playlist_pos = aud_playlist_get_position (playlist);
                playlist_time =
                    aud_playlist_entry_get_length (playlist, playlist_pos,
                                                   FALSE);
                if (playlist_time - output_time < n)
                    output_time = playlist_time - n;
                aud_drct_seek (output_time + n);
            }
            else if (strncasecmp ("BWD", c, 3) == 0)
            {
                ptr = c + 3;
                while (isspace (*ptr))
                    ptr++;
                n = atoi (ptr) * 1000;

                if (n <= 0)
                    n = 5000;
                output_time = aud_drct_get_time ();
                if (output_time < n)
                    output_time = n;
                aud_drct_seek (output_time - n);
            }
            else if (strncasecmp ("VOL_UP", c, 6) == 0)
            {
                ptr = c + 6;
                while (isspace (*ptr))
                    ptr++;
                n = atoi (ptr);
                if (n <= 0)
                    n = 5;

                aud_drct_get_volume_main (&v);
                if (v > (100 - n))
                    v = 100 - n;
                aud_drct_set_volume_main (v + n);
            }
            else if (strncasecmp ("VOL_DOWN", c, 8) == 0)
            {
                ptr = c + 8;
                while (isspace (*ptr))
                    ptr++;
                n = atoi (ptr);
                if (n <= 0)
                    n = 5;

                aud_drct_get_volume_main (&v);
                if (v < n)
                    v = n;
                aud_drct_set_volume_main (v - n);
            }
            else if (strcasecmp ("QUIT", c) == 0)
            {
                aud_drct_quit ();
            }
            else if (strcasecmp ("MUTE", c) == 0)
            {
                if (mute == 0)
                {
                    mute = 1;
                    /* store the master volume so
                       we can restore it on unmute. */
                    aud_drct_get_volume_main (&mute_vol);
                    aud_drct_set_volume_main (0);
                }
                else
                {
                    mute = 0;
                    aud_drct_set_volume_main (mute_vol);
                }
            }
            else if (strncasecmp ("BAL_LEFT", c, 8) == 0)
            {
                ptr = c + 8;
                while (isspace (*ptr))
                    ptr++;
                n = atoi (ptr);
                if (n <= 0)
                    n = 5;

                aud_drct_get_volume_balance (&balance);
                balance -= n;
                if (balance < -100)
                    balance = -100;
                aud_drct_set_volume_balance (balance);
            }
            else if (strncasecmp ("BAL_RIGHT", c, 9) == 0)
            {
                ptr = c + 9;
                while (isspace (*ptr))
                    ptr++;
                n = atoi (ptr);
                if (n <= 0)
                    n = 5;

                aud_drct_get_volume_balance (&balance);
                balance += n;
                if (balance > 100)
                    balance = 100;
                aud_drct_set_volume_balance (balance);
            }
            else if (strcasecmp ("BAL_CENTER", c) == 0)
            {
                balance = 0;
                aud_drct_set_volume_balance (balance);
            }
            else if (strcasecmp ("LIST", c) == 0)
            {
#if 0
                show_pl = aud_drct_pl_win_is_visible ();
                show_pl = (show_pl) ? 0 : 1;
                aud_drct_pl_win_toggle (show_pl);
#endif
            }
            else if (strcasecmp ("PLAYLIST_CLEAR", c) == 0)
            {
                aud_drct_stop ();
                int playlist = aud_playlist_get_active ();
                aud_playlist_entry_delete (playlist, 0,
                                           aud_playlist_entry_count
                                           (playlist));
            }
            else if (strncasecmp ("PLAYLIST_ADD ", c, 13) == 0)
            {
                aud_drct_pl_add (c + 13, -1);
            }
            else if ((strlen (c) == 1) && ((*c >= '0') || (*c <= '9')))
            {
                if (track_no_pos < 63)
                {
                    if (tid)
                        g_source_remove (tid);
                    track_no[track_no_pos++] = *c;
                    track_no[track_no_pos] = 0;
                    tid = g_timeout_add (1500, jump_to, NULL);
                    utf8_title_markup =
                        g_markup_printf_escaped
                        ("<span font_desc='%s'>%s</span>", aosd_font,
                         track_no);
                    hook_call ("aosd toggle", utf8_title_markup);
                }
            }
            else
            {
                fprintf (stderr, _("%s: unknown command \"%s\"\n"),
                         plugin_name, c);
            }
        }
        free (code);
        if (ret == -1)
            break;
    }
    if (ret == -1)
    {
        /* something went badly wrong */
        fprintf (stderr, _("%s: disconnected from LIRC\n"), plugin_name);
        cleanup ();
        if (b_enable_reconnect)
        {
            fprintf (stderr,
                     _("%s: will try reconnect every %d seconds...\n"),
                     plugin_name, reconnect_timeout);
            g_timeout_add (1000 * reconnect_timeout, reconnect_lirc, NULL);
        }
    }

    return TRUE;
}

static const char about[] =
 "A simple plugin to control Audacious using the LIRC remote control daemon\n\n"
 "Adapted for Audacious by:\n"
 "Tony Vroon <chainsaw@gentoo.org>\n"
 "Joonas Harjumäki <jharjuma@gmail.com>\n\n"
 "Based on the XMMS LIRC plugin by:\n"
 "Carl van Schaik <carl@leg.uct.ac.za>\n"
 "Christoph Bartelmus <xmms@bartelmus.de>\n"
 "Andrew O. Shadoura <bugzilla@tut.by>\n\n"
 "For more information about LIRC, see http://lirc.org.";

AUD_GENERAL_PLUGIN
(
    .name = N_("LIRC Plugin"),
    .domain = PACKAGE,
    .about_text = about,
    .init = init,
    .configure = configure,
    .cleanup = cleanup
)
