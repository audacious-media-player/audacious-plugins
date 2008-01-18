#include "settings.h"
#include "config.h"

#include <glib.h>
#include <audacious/i18n.h>

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#include <audacious/plugin.h>
#include <audacious/ui_preferences.h>
#include <audacious/playlist.h>
#include <audacious/configdb.h>
#include <audacious/hook.h>
#include <audacious/strings.h>
#include <audacious/main.h>

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <wchar.h>
#include <sys/time.h>

#include "plugin.h"
#include "scrobbler.h"
#include "gerpok.h"
#include "gtkstuff.h"
#include "config.h"
#include "fmt.h"
#include "configure.h"

#define XS_SLEEP 1
#define HS_SLEEP 10

typedef struct submit_t
{
	int dosubmit, pos_c, len, gerpok;
} submit_t;

static void init(void);
static void cleanup(void);
static void *xs_thread(void *);
static void *hs_thread(void *);
static int sc_going, ge_going;
static GtkWidget *cfgdlg;
static gboolean submit;

static GMutex *m_scrobbler;
static GThread *pt_scrobbler;
static GThread *pt_handshake;

static GMutex *hs_mutex, *xs_mutex;
static GCond *hs_cond, *xs_cond;
guint track_timeout;

static GeneralPlugin scrobbler_gp =
{
	.description = "Scrobbler Plugin",
	.init = init,
	.about = about_show,
	.cleanup = cleanup
};

static gboolean ishttp(const char *a)
{
	g_return_val_if_fail(a != NULL, FALSE);
	return aud_str_has_prefix_nocase(a, "http://") || aud_str_has_prefix_nocase(a, "https://");
}

static void aud_hook_playback_begin(gpointer aud_hook_data, gpointer user_data)
{
	PlaylistEntry *entry = (PlaylistEntry *) aud_hook_data;

	g_return_if_fail(entry != NULL);

	if (entry->length < 30)
	{
		pdebug(" *** not submitting due to entry->length < 30", DEBUG);
		return;
	}

	if (ishttp(entry->filename))
	{
		pdebug(" *** not submitting due to HTTP source", DEBUG);
		return;
	}

	/* wake up the scrobbler thread to submit or queue */
	submit = TRUE;
	g_cond_signal(xs_cond);
}

static void aud_hook_playback_end(gpointer aud_hook_data, gpointer user_data)
{
    if (track_timeout) {
        g_source_remove(track_timeout);
        track_timeout = 0;
    }
}

void start(void) {
	char *username = NULL, *password = NULL;
	char *ge_username = NULL, *ge_password = NULL;
	ConfigDb *cfgfile;
	sc_going = 1;
	ge_going = 1;
	GError **moo = NULL;

	if ((cfgfile = aud_cfg_db_open()) != NULL) {
		aud_cfg_db_get_string(cfgfile, "audioscrobbler", "username",
				&username);
		aud_cfg_db_get_string(cfgfile, "audioscrobbler", "password",
				&password);
		aud_cfg_db_get_string(cfgfile, "audioscrobbler", "ge_username",
				&ge_username);
		aud_cfg_db_get_string(cfgfile, "audioscrobbler", "ge_password",
				&ge_password);
		aud_cfg_db_close(cfgfile);
	}

	if ((!username || !password) || (!*username || !*password))
	{
		pdebug("username/password not found - not starting last.fm support",
			DEBUG);
		sc_going = 0;
	}
	else
	{
		sc_init(username, password);

		g_free(username);
		g_free(password);
	}
	
	if ((!ge_username || !ge_password) || (!*ge_username || !*ge_password))
	{
		pdebug("username/password not found - not starting Gerpok support",
			DEBUG);
		ge_going = 0;
	}
	else
	{
		gerpok_sc_init(ge_username, ge_password);

		g_free(ge_username);
		g_free(ge_password);
	}

	m_scrobbler = g_mutex_new();
	hs_mutex = g_mutex_new();
	xs_mutex = g_mutex_new();
	hs_cond = g_cond_new();
	xs_cond = g_cond_new();

	if ((pt_scrobbler = g_thread_create(xs_thread, NULL, TRUE, moo)) == NULL)
	{
		pdebug(fmt_vastr("Error creating scrobbler thread: %s", moo), DEBUG);
		sc_going = 0;
		ge_going = 0;
		return;
	}

	if ((pt_handshake = g_thread_create(hs_thread, NULL, TRUE, moo)) == NULL)
	{
		pdebug(fmt_vastr("Error creating handshake thread: %s", moo), DEBUG);
		sc_going = 0;
		ge_going = 0;
		return;
	}

	aud_hook_associate("playback begin", aud_hook_playback_begin, NULL);
	aud_hook_associate("playback end", aud_hook_playback_end, NULL);

	pdebug("plugin started", DEBUG);
}

void stop(void) {
	if (!sc_going && !ge_going)
		return;
	pdebug("about to lock mutex", DEBUG);
	g_mutex_lock(m_scrobbler);
	pdebug("locked mutex", DEBUG);
	if (sc_going)
		sc_cleaner();
	if (ge_going)
		gerpok_sc_cleaner();
	sc_going = 0;
	ge_going = 0;
	g_mutex_unlock(m_scrobbler);
	pdebug("joining threads", DEBUG);

	/* wake up waiting threads */
	pdebug("send signal to xs and hs", DEBUG);
	g_cond_signal(xs_cond);
	g_cond_signal(hs_cond);

	pdebug("wait xs", DEBUG);
	g_thread_join(pt_scrobbler);

	pdebug("wait hs", DEBUG);
	g_thread_join(pt_handshake);

	g_cond_free(hs_cond);
	g_cond_free(xs_cond);
	g_mutex_free(hs_mutex);
	g_mutex_free(xs_mutex);
	g_mutex_free(m_scrobbler);

	aud_hook_dissociate("playback begin", aud_hook_playback_begin);
	aud_hook_dissociate("playback end", aud_hook_playback_end);
}

static void init(void)
{
    start();
    cfgdlg = create_cfgdlg();
    aud_prefswin_page_new(cfgdlg, "Scrobbler", DATA_DIR "/images/audioscrobbler.png");
}

static void cleanup(void)
{
    stop();
    configure_cleanup();
    aud_prefswin_page_destroy(cfgdlg);
}

static void *xs_thread(void *data __attribute__((unused)))
{
	int run = 1;

	while (run) {
		Tuple *tuple;
		GTimeVal sleeptime;

		/* Error catching */
		if(sc_catch_error())
		{
			errorbox_show(sc_fetch_error());
			sc_clear_error();
		}

		if(gerpok_sc_catch_error())
		{
			errorbox_show(gerpok_sc_fetch_error());
			gerpok_sc_clear_error();
		}

		if (submit)
		{
			Playlist *playlist;

			pdebug("Submitting song.", DEBUG);

			playlist = aud_playlist_get_active();
			tuple = aud_playlist_get_tuple(playlist, aud_playlist_get_position(playlist));

			if (tuple == NULL)
				continue;

			if (ishttp(aud_tuple_get_string(tuple, FIELD_FILE_NAME, NULL)))
				continue;

			if (aud_tuple_get_string(tuple, FIELD_ARTIST, NULL) != NULL &&
				aud_tuple_get_string(tuple, FIELD_TITLE, NULL) != NULL)
			{
				pdebug(fmt_vastr(
					"submitting artist: %s, title: %s",
					aud_tuple_get_string(tuple, FIELD_ARTIST, NULL),
					aud_tuple_get_string(tuple, FIELD_TITLE, NULL)), DEBUG);

				if (sc_going)
					sc_addentry(m_scrobbler, tuple, aud_tuple_get_int(tuple, FIELD_LENGTH, NULL) / 1000);
				if (ge_going)
					gerpok_sc_addentry(m_scrobbler, tuple, aud_tuple_get_int(tuple, FIELD_LENGTH, NULL) / 1000);
                                if (!track_timeout)
                                    track_timeout = g_timeout_add_seconds(1, sc_timeout, NULL);
			}
			else
				pdebug("tuple does not contain an artist or a title, not submitting.", DEBUG);

			submit = FALSE;
		}

		g_get_current_time(&sleeptime);
		sleeptime.tv_sec += XS_SLEEP;

		g_mutex_lock(xs_mutex);
		g_cond_timed_wait(xs_cond, xs_mutex, &sleeptime);
		g_mutex_unlock(xs_mutex);

		g_mutex_lock(m_scrobbler);
		run = (sc_going != 0 || ge_going != 0);
		g_mutex_unlock(m_scrobbler);
	}
	pdebug("scrobbler thread: exiting", DEBUG);
	g_thread_exit(NULL);

	return NULL;
}

static void *hs_thread(void *data __attribute__((unused)))
{
	int run = 1;
	GTimeVal sleeptime;

	while(run)
	{
		if(sc_going && sc_idle(m_scrobbler))
		{
			pdebug("Giving up due to fatal error", DEBUG);
			g_mutex_lock(m_scrobbler);
			sc_going = 0;
			g_mutex_unlock(m_scrobbler);
		}

		if(ge_going && gerpok_sc_idle(m_scrobbler))
		{
			pdebug("Giving up due to fatal error", DEBUG);
			g_mutex_lock(m_scrobbler);
			ge_going = 0;
			g_mutex_unlock(m_scrobbler);
		}

		g_mutex_lock(m_scrobbler);
		run = (sc_going != 0 || ge_going != 0);
		g_mutex_unlock(m_scrobbler);

		if(run) {
			g_get_current_time(&sleeptime);
			sleeptime.tv_sec += HS_SLEEP;

			g_mutex_lock(hs_mutex);
			g_cond_timed_wait(hs_cond, hs_mutex, &sleeptime);
			g_mutex_unlock(hs_mutex);
		}
	}
	pdebug("handshake thread: exiting", DEBUG);
	g_thread_exit(NULL);

	return NULL;
}

GeneralPlugin *scrobbler_gplist[] = { &scrobbler_gp, NULL };

DECLARE_PLUGIN(scrobbler, NULL, NULL, NULL, NULL, NULL, scrobbler_gplist, NULL, NULL);
