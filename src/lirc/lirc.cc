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
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
*/

#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>
#include <stdlib.h>

#include <glib.h>
#include <lirc/lirc_client.h>

#include <libaudcore/drct.h>
#include <libaudcore/i18n.h>
#include <libaudcore/runtime.h>
#include <libaudcore/playlist.h>
#include <libaudcore/plugin.h>
#include <libaudcore/preferences.h>
#include <libaudcore/hook.h>

class LIRCPlugin : public GeneralPlugin
{
public:
    static const char about[];
    static const char * const defaults[];
    static const PreferencesWidget widgets[];
    static const PluginPreferences prefs;

    static constexpr PluginInfo info = {
        N_("LIRC Plugin"),
        PACKAGE,
        about,
        & prefs
    };

    constexpr LIRCPlugin () : GeneralPlugin (info, false) {}

    bool init ();
    void cleanup ();
};

EXPORT LIRCPlugin aud_plugin_instance;

static gboolean lirc_input_callback (GIOChannel * source, GIOCondition condition, void * data);

const char * const LIRCPlugin::defaults[] = {
 "enable_reconnect", "TRUE",
 "reconnect_timeout", "5",
 nullptr};

int lirc_fd = -1;
struct lirc_config *config = nullptr;
int tracknr = 0;
int mute = 0;                  /* mute flag */
int mute_vol = 0;              /* holds volume before mute */

unsigned input_tag;

char track_no[64];
int track_no_pos;
int tid;

void init_lirc ()
{
    int flags;

    if ((lirc_fd = lirc_init ((char *) "audacious", 1)) == -1)
    {
        AUDERR ("could not init LIRC support\n");
        return;
    }
    if (lirc_readconfig (nullptr, &config, nullptr) == -1)
    {
        lirc_deinit ();
        AUDERR ("could not read LIRC config file\n");
        return;
    }

    input_tag =
        g_io_add_watch (g_io_channel_unix_new (lirc_fd), G_IO_IN,
                        lirc_input_callback, nullptr);

    fcntl (lirc_fd, F_SETOWN, getpid ());
    flags = fcntl (lirc_fd, F_GETFL, 0);
    if (flags != -1)
    {
        fcntl (lirc_fd, F_SETFL, flags | O_NONBLOCK);
    }
}

bool LIRCPlugin::init ()
{
    aud_config_set_defaults ("lirc", defaults);
    init_lirc ();
    track_no_pos = 0;
    tid = 0;
    return true;
}

gboolean reconnect_lirc (void * data)
{
    AUDERR ("trying to reconnect...\n");
    aud_plugin_instance.init ();
    return (lirc_fd == -1);
}

void LIRCPlugin::cleanup ()
{
    if (config)
    {
        if (input_tag)
            g_source_remove (input_tag);

        config = nullptr;
    }
    if (lirc_fd != -1)
    {
        lirc_deinit ();
        lirc_fd = -1;
    }
}

gboolean jump_to (void * data)
{
    auto playlist = Playlist::active_playlist ();
    playlist.set_position (atoi (track_no) - 1);
    track_no_pos = 0;
    tid = 0;
    return false;
}

static gboolean lirc_input_callback (GIOChannel * source, GIOCondition condition, void * data)
{
    char *code;
    char *c;
    int output_time, v;
    int ret;
    char *ptr;
    int balance;
#if 0
    gboolean show_pl;
#endif
    int n;
    char *utf8_title_markup;

    while ((ret = lirc_nextcode (&code)) == 0 && code != nullptr)
    {
        while ((ret = lirc_code2char (config, code, &c)) == 0 && c != nullptr)
        {
            if (g_ascii_strcasecmp ("PLAY", c) == 0)
                aud_drct_play ();
            else if (g_ascii_strcasecmp ("STOP", c) == 0)
                aud_drct_stop ();
            else if (g_ascii_strcasecmp ("PAUSE", c) == 0)
                aud_drct_pause ();
            else if (g_ascii_strcasecmp ("PLAYPAUSE", c) == 0)
                aud_drct_play_pause ();
            else if (g_ascii_strncasecmp ("NEXT", c, 4) == 0)
            {
                ptr = c + 4;
                while (g_ascii_isspace (*ptr))
                    ptr++;
                n = atoi (ptr);

                if (n <= 0)
                    n = 1;
                for (; n > 0; n--)
                {
                    aud_drct_pl_next ();
                }
            }
            else if (g_ascii_strncasecmp ("PREV", c, 4) == 0)
            {
                ptr = c + 4;
                while (g_ascii_isspace (*ptr))
                    ptr++;
                n = atoi (ptr);

                if (n <= 0)
                    n = 1;
                for (; n > 0; n--)
                {
                    aud_drct_pl_prev ();
                }
            }
            else if (g_ascii_strcasecmp ("SHUFFLE", c) == 0)
                aud_toggle_bool (nullptr, "shuffle");
            else if (g_ascii_strcasecmp ("REPEAT", c) == 0)
                aud_toggle_bool (nullptr, "repeat");
            else if (g_ascii_strncasecmp ("FWD", c, 3) == 0)
            {
                ptr = c + 3;
                while (g_ascii_isspace (*ptr))
                    ptr++;
                n = atoi (ptr) * 1000;

                if (n <= 0)
                    n = 5000;
                output_time = aud_drct_get_time ();
                aud_drct_seek (output_time + n);
            }
            else if (g_ascii_strncasecmp ("BWD", c, 3) == 0)
            {
                ptr = c + 3;
                while (g_ascii_isspace (*ptr))
                    ptr++;
                n = atoi (ptr) * 1000;

                if (n <= 0)
                    n = 5000;
                output_time = aud_drct_get_time ();
                aud_drct_seek (output_time - n);
            }
            else if (g_ascii_strncasecmp ("VOL_UP", c, 6) == 0)
            {
                ptr = c + 6;
                while (g_ascii_isspace (*ptr))
                    ptr++;
                n = atoi (ptr);
                if (n <= 0)
                    n = 5;

                v = aud_drct_get_volume_main ();
                if (v > (100 - n))
                    v = 100 - n;
                aud_drct_set_volume_main (v + n);
            }
            else if (g_ascii_strncasecmp ("VOL_DOWN", c, 8) == 0)
            {
                ptr = c + 8;
                while (g_ascii_isspace (*ptr))
                    ptr++;
                n = atoi (ptr);
                if (n <= 0)
                    n = 5;

                v = aud_drct_get_volume_main ();
                if (v < n)
                    v = n;
                aud_drct_set_volume_main (v - n);
            }
            else if (g_ascii_strcasecmp ("QUIT", c) == 0)
            {
                aud_quit ();
            }
            else if (g_ascii_strcasecmp ("MUTE", c) == 0)
            {
                if (mute == 0)
                {
                    mute = 1;
                    /* store the master volume so
                       we can restore it on unmute. */
                    mute_vol = aud_drct_get_volume_main ();
                    aud_drct_set_volume_main (0);
                }
                else
                {
                    mute = 0;
                    aud_drct_set_volume_main (mute_vol);
                }
            }
            else if (g_ascii_strncasecmp ("BAL_LEFT", c, 8) == 0)
            {
                ptr = c + 8;
                while (g_ascii_isspace (*ptr))
                    ptr++;
                n = atoi (ptr);
                if (n <= 0)
                    n = 5;

                balance = aud_drct_get_volume_balance ();
                balance -= n;
                if (balance < -100)
                    balance = -100;
                aud_drct_set_volume_balance (balance);
            }
            else if (g_ascii_strncasecmp ("BAL_RIGHT", c, 9) == 0)
            {
                ptr = c + 9;
                while (g_ascii_isspace (*ptr))
                    ptr++;
                n = atoi (ptr);
                if (n <= 0)
                    n = 5;

                balance = aud_drct_get_volume_balance ();
                balance += n;
                if (balance > 100)
                    balance = 100;
                aud_drct_set_volume_balance (balance);
            }
            else if (g_ascii_strcasecmp ("BAL_CENTER", c) == 0)
            {
                balance = 0;
                aud_drct_set_volume_balance (balance);
            }
            else if (g_ascii_strcasecmp ("LIST", c) == 0)
            {
#if 0
                show_pl = aud_drct_pl_win_is_visible ();
                show_pl = (show_pl) ? 0 : 1;
                aud_drct_pl_win_toggle (show_pl);
#endif
            }
            else if (g_ascii_strcasecmp ("PLAYLIST_CLEAR", c) == 0)
            {
                aud_drct_stop ();
                Playlist::active_playlist ().remove_all_entries ();
            }
            else if (g_ascii_strncasecmp ("PLAYLIST_ADD ", c, 13) == 0)
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
                    tid = g_timeout_add (1500, jump_to, nullptr);
                    utf8_title_markup = g_markup_printf_escaped ("%s", track_no);
                    hook_call ("aosd toggle", utf8_title_markup);
                    g_free (utf8_title_markup);
                }
            }
            else
            {
                AUDERR ("unknown command \"%s\"\n", c);
            }
        }
        g_free (code);
        if (ret == -1)
            break;
    }
    if (ret == -1)
    {
        /* something went badly wrong */
        AUDERR ("disconnected from LIRC\n");
        aud_plugin_instance.cleanup ();
        if (aud_get_bool ("lirc", "enable_reconnect"))
        {
            int reconnect_timeout = aud_get_int ("lirc", "reconnect_timeout");
            AUDERR ("will try reconnect every %d seconds...\n", reconnect_timeout);
            g_timeout_add (1000 * reconnect_timeout, reconnect_lirc, nullptr);
        }
    }

    return true;
}

const char LIRCPlugin::about[] =
 N_("A simple plugin to control Audacious using the LIRC remote control daemon\n\n"
    "Adapted for Audacious by:\n"
    "Tony Vroon <chainsaw@gentoo.org>\n"
    "Joonas Harjumäki <jharjuma@gmail.com>\n\n"
    "Based on the XMMS LIRC plugin by:\n"
    "Carl van Schaik <carl@leg.uct.ac.za>\n"
    "Christoph Bartelmus <xmms@bartelmus.de>\n"
    "Andrew O. Shadoura <bugzilla@tut.by>\n\n"
    "For more information about LIRC, see http://lirc.org.");

const PreferencesWidget LIRCPlugin::widgets[] = {
    WidgetLabel (N_("<b>Connection</b>")),
    WidgetCheck (N_("Reconnect to LIRC server"),
        WidgetBool ("lirc", "enable_reconnect")),
    WidgetSpin (N_("Wait before reconnecting:"),
        WidgetInt ("lirc", "reconnect_timeout"),
        {1, 120, 1, N_("seconds")},
        WIDGET_CHILD)
};

const PluginPreferences LIRCPlugin::prefs = {{widgets}};
