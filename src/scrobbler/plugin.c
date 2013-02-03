#include "settings.h"

#include <glib.h>
#include <audacious/i18n.h>

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#include <audacious/debug.h>
#include <audacious/misc.h>
#include <audacious/playlist.h>
#include <audacious/plugin.h>
#include <audacious/plugins.h>
#include <libaudcore/audstrings.h>
#include <libaudcore/hook.h>
#include <libaudgui/libaudgui.h>
#include <libaudgui/libaudgui-gtk.h>

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <wchar.h>
#include <sys/time.h>

#include <curl/curl.h>

#include "plugin.h"
#include "scrobbler.h"
#include "fmt.h"

typedef struct submit_t
{
    int dosubmit, pos_c, len;
} submit_t;

static int sc_going, ge_going;

static pthread_mutex_t m_scrobbler = PTHREAD_MUTEX_INITIALIZER;

guint track_timeout = 0;

static gboolean ishttp(const char *a)
{
    g_return_val_if_fail(a != NULL, FALSE);
    return str_has_prefix_nocase(a, "http://") || str_has_prefix_nocase(a, "https://");
}

static void aud_hook_playback_begin(gpointer hook_data, gpointer user_data)
{
    gint playlist = aud_playlist_get_playing();
    gint pos = aud_playlist_get_position(playlist);

    char *filename = aud_playlist_entry_get_filename(playlist, pos);
    bool_t is_http_source = ishttp(filename);
    str_unref(filename);

    Tuple *tuple = aud_playlist_entry_get_tuple(playlist, pos, FALSE);
    if (!tuple)
        return;

    int len = tuple_get_int(tuple, FIELD_LENGTH, NULL) / 1000;

    /* Make up a length when we submit streaming to now playing.
     * We will change it before we actually scrobble the track. */
    if (len < 1 && is_http_source)
        len = 240;

    if (len < 30)
    {
        AUDDBG("Length less than 30 seconds; not submitting\n");
        tuple_unref(tuple);
        return;
    }

    sc_idle(&m_scrobbler);

    sc_addentry(&m_scrobbler, tuple, len, is_http_source);
    tuple_unref(tuple);

    if (!track_timeout)
        track_timeout = g_timeout_add_seconds(1, sc_timeout, &m_scrobbler);
}

static void aud_hook_playback_end(gpointer aud_hook_data, gpointer user_data)
{
    sc_playback_end();
    sc_idle(&m_scrobbler);

    if (track_timeout)
    {
        g_source_remove(track_timeout);
        track_timeout = 0;
    }
}

void start(void) {
    sc_going = 1;

    gchar * username = aud_get_string ("audioscrobbler", "username");
    gchar * password = aud_get_string ("audioscrobbler", "password");
    gchar * sc_url = aud_get_string ("audioscrobbler", "sc_url");

    if ((!username || !password) || (!*username || !*password))
    {
        AUDDBG("username/password not found - not starting last.fm support");
        sc_going = 0;
    }
    else
        sc_init(username, password, sc_url);

    g_free (username);
    g_free (password);
    g_free (sc_url);

    hook_associate("playback begin", aud_hook_playback_begin, NULL);
    hook_associate("playback stop", aud_hook_playback_end, NULL);

    AUDDBG("plugin started");
    sc_idle(&m_scrobbler);
}

void stop(void) {
    if (!sc_going && !ge_going)
        return;
    pthread_mutex_lock(&m_scrobbler);

    if (sc_going)
        sc_cleaner();
    sc_going = 0;
    ge_going = 0;
    pthread_mutex_unlock(&m_scrobbler);

    hook_dissociate("playback begin", aud_hook_playback_begin);
    hook_dissociate("playback stop", aud_hook_playback_end);
}

static gboolean init(void)
{
    PluginHandle *new_scrobbler = aud_plugin_lookup_basename("scrobbler2");
    if (new_scrobbler != NULL && aud_plugin_get_enabled(new_scrobbler)) {
        aud_interface_show_error(N_("This is an old, legacy version of the Scrobbler plugin.\n"
                                    "To use it, disable the Scrobbler 2.0 plugin."));
        return FALSE;
    }


    start();
    return TRUE;
}

static void cleanup(void)
{
    stop();
}

void setup_proxy(CURL *curl)
{
    if (! aud_get_bool (NULL, "use_proxy"))
        curl_easy_setopt(curl, CURLOPT_PROXY, "");
    else
    {
        gchar * proxy_host = aud_get_string (NULL, "proxy_host");
        gchar * proxy_port = aud_get_string (NULL, "proxy_port");

        curl_easy_setopt(curl, CURLOPT_PROXY, proxy_host);
        curl_easy_setopt(curl, CURLOPT_PROXYPORT, atol(proxy_port));

        if (! aud_get_bool (NULL, "proxy_use_auth"))
        {
            gchar * user = aud_get_string (NULL, "proxy_user");
            gchar * pass = aud_get_string (NULL, "proxy_pass");
            gchar * userpwd = g_strdup_printf ("%s:%s", user, pass);

            curl_easy_setopt(curl, CURLOPT_PROXYUSERPWD, userpwd);

            g_free (user);
            g_free (pass);
            g_free (userpwd);
        }

        g_free (proxy_host);
        g_free (proxy_port);
    }
}

static const char about[] =
 N_("Audacious AudioScrobbler Plugin\n\n"
    "Originally created by Audun Hove <audun@nlc.no> and Pipian <pipian@pipian.com>");

AUD_GENERAL_PLUGIN
(
 .name = N_("Scrobbler"),
 .domain = PACKAGE,
 .about_text = about,
 .prefs = & preferences,
 .init = init,
 .cleanup = cleanup
)
