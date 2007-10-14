/*
 * Audacious: A cross-platform multimedia player.
 * Copyright (c) 2005  Audacious Team
 */
#include "config.h"

#include <glib.h>
#include <gtk/gtk.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <signal.h>
#include <unistd.h>
#include <stdio.h>

#include <string.h>

#include <audacious/plugin.h>
#include <audacious/ui_preferences.h>
#include <audacious/configdb.h>
#include <audacious/auddrct.h>
#include <audacious/formatter.h>
#include <audacious/i18n.h>
#include <audacious/hook.h>
#include <audacious/playlist.h>

static void init(void);
static void cleanup(void);
static GtkWidget *configure(void);
static void songchange_playback_begin(gpointer unused, gpointer unused2);
static void songchange_playback_end(gpointer unused, gpointer unused2);
static void songchange_playlist_eof(gpointer unused, gpointer unused2);
static void songchange_playback_ttc(gpointer, gpointer);

typedef struct
{
  gchar *title;
  gchar *filename;
}
songchange_playback_ttc_prevs_t;
static songchange_playback_ttc_prevs_t *ttc_prevs = NULL;

static char *cmd_line = NULL;
static char *cmd_line_after = NULL;
static char *cmd_line_end = NULL;
static char *cmd_line_ttc = NULL;

static GtkWidget *configure_vbox = NULL;
static GtkWidget *cmd_entry, *cmd_after_entry, *cmd_end_entry, *cmd_ttc_entry;
static GtkWidget *cmd_warn_label, *cmd_warn_img;

GeneralPlugin sc_gp =
{
	NULL,			/* handle */
	NULL,			/* filename */
	"Song Change " PACKAGE_VERSION,			/* Description */
	init,
	NULL,
	NULL,
	cleanup,
};

GeneralPlugin *songchange_gplist[] = { &sc_gp, NULL };
DECLARE_PLUGIN(songchange, NULL, NULL, NULL, NULL, NULL, songchange_gplist, NULL, NULL);

static void bury_child(int signal)
{
	waitpid(-1, NULL, WNOHANG);
}

static void execute_command(char *cmd)
{
	char *argv[4] = {"/bin/sh", "-c", NULL, NULL};
	int i;
	argv[2] = cmd;
	signal(SIGCHLD, bury_child);
	if (fork() == 0)
	{
		/* We don't want this process to hog the audio device etc */
		for (i = 3; i < 255; i++)
			close(i);
		execv("/bin/sh", argv);
	}
}

/* Format codes:
 *
 *   F - frequency (in hertz)
 *   c - number of channels
 *   f - filename (full path)
 *   l - length (in milliseconds)
 *   n - name
 *   r - rate (in bits per second)
 *   s - name (since everyone's used to it)
 *   t - playlist position (%02d)
 *   p - currently playing (1 or 0)
 */
/* do_command(): do @cmd after replacing the format codes
   @cmd: command to run
   @current_file: file name of current song
   @pos: playlist_pos */
static void
do_command(char *cmd, const char *current_file, int pos)
{
	int length, rate, freq, nch;
	char *str, *shstring = NULL, *temp, numbuf[16];
	gboolean playing;
	Formatter *formatter;

	if (cmd && strlen(cmd) > 0)
	{
		formatter = formatter_new();
		str = audacious_drct_pl_get_title(pos);
		if (str)
		{
			temp = aud_escape_shell_chars(str);
			formatter_associate(formatter, 's', temp);
			formatter_associate(formatter, 'n', temp);
			g_free(str);
			g_free(temp);
		}
		else
		{
			formatter_associate(formatter, 's', "");
			formatter_associate(formatter, 'n', "");
		}

		if (current_file)
		{
			temp = aud_escape_shell_chars(current_file);
			formatter_associate(formatter, 'f', temp);
			g_free(temp);
		}
		else
			formatter_associate(formatter, 'f', "");
		sprintf(numbuf, "%02d", pos + 1);
		formatter_associate(formatter, 't', numbuf);
		length = audacious_drct_pl_get_time(pos);
		if (length != -1)
		{
			sprintf(numbuf, "%d", length);
			formatter_associate(formatter, 'l', numbuf);
		}
		else
			formatter_associate(formatter, 'l', "0");
		audacious_drct_get_info(&rate, &freq, &nch);
		sprintf(numbuf, "%d", rate);
		formatter_associate(formatter, 'r', numbuf);
		sprintf(numbuf, "%d", freq);
		formatter_associate(formatter, 'F', numbuf);
		sprintf(numbuf, "%d", nch);
		formatter_associate(formatter, 'c', numbuf);
		playing = audacious_drct_get_playing();
		sprintf(numbuf, "%d", playing);
		formatter_associate(formatter, 'p', numbuf);
		shstring = formatter_format(formatter, cmd);
		formatter_destroy(formatter);

		if (shstring)
		{
			execute_command(shstring);
			/* FIXME: This can possibly be freed too early */
			g_free(shstring);
		}
	}
}

static void read_config(void)
{
	ConfigDb *db;

	db = bmp_cfg_db_open();
	if ( !bmp_cfg_db_get_string(db, "song_change", "cmd_line", &cmd_line) )
		cmd_line = g_strdup("");
	if ( !bmp_cfg_db_get_string(db, "song_change", "cmd_line_after", &cmd_line_after) )
		cmd_line_after = g_strdup("");
	if ( !bmp_cfg_db_get_string(db, "song_change", "cmd_line_end", &cmd_line_end) )
		cmd_line_end = g_strdup("");
	if ( !bmp_cfg_db_get_string(db, "song_change", "cmd_line_ttc", &cmd_line_ttc) )
		cmd_line_ttc = g_strdup("");
	bmp_cfg_db_close(db);
}

static void cleanup(void)
{
	aud_hook_dissociate("playback begin", songchange_playback_begin);
	aud_hook_dissociate("playback end", songchange_playback_end);
	aud_hook_dissociate("playlist end reached", songchange_playlist_eof);
      aud_hook_dissociate( "playlist set info" , songchange_playback_ttc);

	if ( ttc_prevs != NULL )
	{
		if ( ttc_prevs->title != NULL ) g_free( ttc_prevs->title );
		if ( ttc_prevs->filename != NULL ) g_free( ttc_prevs->filename );
		g_free( ttc_prevs );
		ttc_prevs = NULL;
	}

	g_free(cmd_line);
	g_free(cmd_line_after);
	g_free(cmd_line_end);
	g_free(cmd_line_ttc);
	cmd_line = NULL;
	cmd_line_after = NULL;
	cmd_line_end = NULL;
	cmd_line_ttc = NULL;
	signal(SIGCHLD, SIG_DFL);

	prefswin_page_destroy(configure_vbox);
}

static void save_and_close(GtkWidget *w, gpointer data)
{
	ConfigDb *db;
	char *cmd, *cmd_after, *cmd_end, *cmd_ttc;

	cmd = g_strdup(gtk_entry_get_text(GTK_ENTRY(cmd_entry)));
	cmd_after = g_strdup(gtk_entry_get_text(GTK_ENTRY(cmd_after_entry)));
	cmd_end = g_strdup(gtk_entry_get_text(GTK_ENTRY(cmd_end_entry)));
	cmd_ttc = g_strdup(gtk_entry_get_text(GTK_ENTRY(cmd_ttc_entry)));

	db = bmp_cfg_db_open();
	bmp_cfg_db_set_string(db, "song_change", "cmd_line", cmd);
	bmp_cfg_db_set_string(db, "song_change", "cmd_line_after", cmd_after);
	bmp_cfg_db_set_string(db, "song_change", "cmd_line_end", cmd_end);
	bmp_cfg_db_set_string(db, "song_change", "cmd_line_ttc", cmd_ttc);
	bmp_cfg_db_close(db);

	if (cmd_line != NULL)
		g_free(cmd_line);

	cmd_line = g_strdup(cmd);

	if (cmd_line_after != NULL)
		g_free(cmd_line_after);

	cmd_line_after = g_strdup(cmd_after);

	if (cmd_line_end != NULL)
		g_free(cmd_line_end);

	cmd_line_end = g_strdup(cmd_end);

	if (cmd_line_ttc != NULL)
		g_free(cmd_line_ttc);

	cmd_line_ttc = g_strdup(cmd_ttc);

	g_free(cmd);
	g_free(cmd_after);
	g_free(cmd_end);
	g_free(cmd_ttc);
}

static int check_command(char *command)
{
	const char *dangerous = "fns";
	char *c;
	int qu = 0;

	for (c = command; *c != '\0'; c++)
	{
		if (*c == '"' && (c == command || *(c - 1) != '\\'))
			qu = !qu;
		else if (*c == '%' && !qu && strchr(dangerous, *(c + 1)))
			return -1;
	}
	return 0;
}

static void configure_ok_cb(GtkWidget *w, gpointer data)
{
	char *cmd, *cmd_after, *cmd_end, *cmd_ttc;

	cmd = g_strdup(gtk_entry_get_text(GTK_ENTRY(cmd_entry)));
	cmd_after = g_strdup(gtk_entry_get_text(GTK_ENTRY(cmd_after_entry)));
	cmd_end = g_strdup(gtk_entry_get_text(GTK_ENTRY(cmd_end_entry)));
	cmd_ttc = g_strdup(gtk_entry_get_text(GTK_ENTRY(cmd_ttc_entry)));

	if (check_command(cmd) < 0 || check_command(cmd_after) < 0 ||
	    check_command(cmd_end) < 0 || check_command(cmd_ttc) < 0)
	{
		gtk_widget_show(cmd_warn_img);
		gtk_widget_show(cmd_warn_label);
	}
	else
	{
		gtk_widget_hide(cmd_warn_img);
		gtk_widget_hide(cmd_warn_label);
		save_and_close(NULL, NULL);
	}

	g_free(cmd);
	g_free(cmd_after);
	g_free(cmd_end);
	g_free(cmd_ttc);
}


static GtkWidget *configure(void)
{
	GtkWidget *sep1, *sep2, *sep3, *sep4;
	GtkWidget *cmd_hbox, *cmd_label;
	GtkWidget *cmd_after_hbox, *cmd_after_label;
	GtkWidget *cmd_end_hbox, *cmd_end_label;
	GtkWidget *cmd_desc, *cmd_after_desc, *cmd_end_desc, *f_desc;
      GtkWidget *cmd_ttc_hbox, *cmd_ttc_label, *cmd_ttc_desc;
	GtkWidget *song_frame, *song_vbox;
	GtkWidget *bbox_hbox;
	char *temp;

	read_config();

	configure_vbox = gtk_vbox_new(FALSE, 12);

	song_frame = gtk_frame_new(_("Commands"));
	gtk_box_pack_start(GTK_BOX(configure_vbox), song_frame, FALSE, FALSE, 0);
	song_vbox = gtk_vbox_new(FALSE, 12);
	gtk_container_set_border_width(GTK_CONTAINER(song_vbox), 6);
	gtk_container_add(GTK_CONTAINER(song_frame), song_vbox);

	cmd_desc = gtk_label_new(_(
		   "Command to run when Audacious starts a new song."));
	gtk_label_set_justify(GTK_LABEL(cmd_desc), GTK_JUSTIFY_LEFT);
	gtk_misc_set_alignment(GTK_MISC(cmd_desc), 0, 0.5);
	gtk_box_pack_start(GTK_BOX(song_vbox), cmd_desc, FALSE, FALSE, 0);
	gtk_label_set_line_wrap(GTK_LABEL(cmd_desc), FALSE);

	cmd_hbox = gtk_hbox_new(FALSE, 6);
	gtk_box_pack_start(GTK_BOX(song_vbox), cmd_hbox, FALSE, FALSE, 0);

	cmd_label = gtk_label_new(_("Command:"));
	gtk_box_pack_start(GTK_BOX(cmd_hbox), cmd_label, FALSE, FALSE, 0);

	cmd_entry = gtk_entry_new();
	if (cmd_line)
		gtk_entry_set_text(GTK_ENTRY(cmd_entry), cmd_line);
	gtk_widget_set_usize(cmd_entry, 200, -1);
	gtk_box_pack_start(GTK_BOX(cmd_hbox), cmd_entry, TRUE, TRUE, 0);

	sep1 = gtk_hseparator_new();
	gtk_box_pack_start(GTK_BOX(song_vbox), sep1, TRUE, TRUE, 0);


	cmd_after_desc = gtk_label_new(_(
		   "Command to run toward the end of a song."));
	gtk_label_set_justify(GTK_LABEL(cmd_after_desc), GTK_JUSTIFY_LEFT);
	gtk_misc_set_alignment(GTK_MISC(cmd_after_desc), 0, 0.5);
	gtk_box_pack_start(GTK_BOX(song_vbox), cmd_after_desc, FALSE, FALSE, 0);

	cmd_after_hbox = gtk_hbox_new(FALSE, 6);
	gtk_box_pack_start(GTK_BOX(song_vbox), cmd_after_hbox, FALSE, FALSE, 0);

	cmd_after_label = gtk_label_new(_("Command:"));
	gtk_box_pack_start(GTK_BOX(cmd_after_hbox), cmd_after_label, FALSE, FALSE, 0);

	cmd_after_entry = gtk_entry_new();
	if (cmd_line_after)
		gtk_entry_set_text(GTK_ENTRY(cmd_after_entry), cmd_line_after);
	gtk_widget_set_usize(cmd_after_entry, 200, -1);
	gtk_box_pack_start(GTK_BOX(cmd_after_hbox), cmd_after_entry, TRUE, TRUE, 0);
	sep2 = gtk_hseparator_new();
	gtk_box_pack_start(GTK_BOX(song_vbox), sep2, TRUE, TRUE, 0);

	cmd_end_desc = gtk_label_new(_(
		"Command to run when Audacious reaches the end "
		"of the playlist."));
	gtk_label_set_justify(GTK_LABEL(cmd_end_desc), GTK_JUSTIFY_LEFT);
	gtk_misc_set_alignment(GTK_MISC(cmd_end_desc), 0, 0.5);
	gtk_box_pack_start(GTK_BOX(song_vbox), cmd_end_desc, FALSE, FALSE, 0);

	cmd_end_hbox = gtk_hbox_new(FALSE, 6);
	gtk_box_pack_start(GTK_BOX(song_vbox), cmd_end_hbox, FALSE, FALSE, 0);

	cmd_end_label = gtk_label_new(_("Command:"));
	gtk_box_pack_start(GTK_BOX(cmd_end_hbox), cmd_end_label, FALSE, FALSE, 0);

	cmd_end_entry = gtk_entry_new();
	if (cmd_line_end)
		gtk_entry_set_text(GTK_ENTRY(cmd_end_entry), cmd_line_end);
	gtk_widget_set_usize(cmd_end_entry, 200, -1);
	gtk_box_pack_start(GTK_BOX(cmd_end_hbox), cmd_end_entry, TRUE, TRUE, 0);
	sep3 = gtk_hseparator_new();
	gtk_box_pack_start(GTK_BOX(song_vbox), sep3, TRUE, TRUE, 0);

	cmd_ttc_desc = gtk_label_new(_(
		"Command to run when title changes for a song "
		"(i.e. network streams titles)."));
	gtk_label_set_justify(GTK_LABEL(cmd_ttc_desc), GTK_JUSTIFY_LEFT);
	gtk_misc_set_alignment(GTK_MISC(cmd_ttc_desc), 0, 0.5);
	gtk_box_pack_start(GTK_BOX(song_vbox), cmd_ttc_desc, FALSE, FALSE, 0);

	cmd_ttc_hbox = gtk_hbox_new(FALSE, 6);
	gtk_box_pack_start(GTK_BOX(song_vbox), cmd_ttc_hbox, FALSE, FALSE, 0);

	cmd_ttc_label = gtk_label_new(_("Command:"));
	gtk_box_pack_start(GTK_BOX(cmd_ttc_hbox), cmd_ttc_label, FALSE, FALSE, 0);

	cmd_ttc_entry = gtk_entry_new();
	if (cmd_line_ttc)
		gtk_entry_set_text(GTK_ENTRY(cmd_ttc_entry), cmd_line_ttc);
	gtk_widget_set_usize(cmd_ttc_entry, 200, -1);
	gtk_box_pack_start(GTK_BOX(cmd_ttc_hbox), cmd_ttc_entry, TRUE, TRUE, 0);
	sep4 = gtk_hseparator_new();
	gtk_box_pack_start(GTK_BOX(song_vbox), sep4, TRUE, TRUE, 0);

	temp = g_strdup_printf(
		_("You can use the following format strings which\n"
		  "will be substituted before calling the command\n"
		  "(not all are useful for the end-of-playlist command).\n\n"
		  "%%F: Frequency (in hertz)\n"
		  "%%c: Number of channels\n"
		  "%%f: filename (full path)\n"
		  "%%l: length (in milliseconds)\n"
		  "%%n or %%s: Song name\n"
		  "%%r: Rate (in bits per second)\n"
		  "%%t: Playlist position (%%02d)\n"
		  "%%p: Currently playing (1 or 0)"));
	f_desc = gtk_label_new(temp);
	g_free(temp);

	gtk_label_set_justify(GTK_LABEL(f_desc), GTK_JUSTIFY_LEFT);
	gtk_misc_set_alignment(GTK_MISC(f_desc), 0, 0.5);
	gtk_box_pack_start(GTK_BOX(song_vbox), f_desc, FALSE, FALSE, 0);

	bbox_hbox = gtk_hbox_new(FALSE, 6);
	gtk_box_pack_start(GTK_BOX(configure_vbox), bbox_hbox, FALSE, FALSE, 0);

	cmd_warn_img = gtk_image_new_from_stock("gtk-dialog-warning", GTK_ICON_SIZE_MENU);
	gtk_box_pack_start(GTK_BOX(bbox_hbox), cmd_warn_img, FALSE, FALSE, 0);

	temp = g_strdup_printf(
		_("<span size='small'>Parameters passed to the shell should be encapsulated in quotes. Doing otherwise is a security risk.</span>"));
	cmd_warn_label = gtk_label_new(temp);
	gtk_label_set_markup(GTK_LABEL(cmd_warn_label), temp);
	gtk_box_pack_start(GTK_BOX(bbox_hbox), cmd_warn_label, FALSE, FALSE, 0);

	g_signal_connect(GTK_OBJECT(cmd_entry), "changed", GTK_SIGNAL_FUNC(configure_ok_cb), NULL);
	g_signal_connect(GTK_OBJECT(cmd_after_entry), "changed", GTK_SIGNAL_FUNC(configure_ok_cb), NULL);
	g_signal_connect(GTK_OBJECT(cmd_end_entry), "changed", GTK_SIGNAL_FUNC(configure_ok_cb), NULL);
	g_signal_connect(GTK_OBJECT(cmd_ttc_entry), "changed", GTK_SIGNAL_FUNC(configure_ok_cb), NULL);

	gtk_widget_show_all(configure_vbox);

	return configure_vbox;
}

static void init(void)
{
	read_config();

	configure_vbox = configure();
	prefswin_page_new(configure_vbox, "Song Change", DATA_DIR "/images/plugins.png");

	aud_hook_associate("playback begin", songchange_playback_begin, NULL);
	aud_hook_associate("playback end", songchange_playback_end, NULL);
	aud_hook_associate("playlist end reached", songchange_playlist_eof, NULL);

	ttc_prevs = g_malloc0(sizeof(songchange_playback_ttc_prevs_t));
	ttc_prevs->title = NULL;
	ttc_prevs->filename = NULL;
	aud_hook_associate( "playlist set info" , songchange_playback_ttc , ttc_prevs );

	configure_ok_cb(NULL, NULL);
}

static void
songchange_playback_begin(gpointer unused, gpointer unused2)
{
	int pos;
	char *current_file;

	pos = audacious_drct_pl_get_pos();
	current_file = audacious_drct_pl_get_file(pos);

	do_command(cmd_line, current_file, pos);

	g_free(current_file);
}

static void
songchange_playback_end(gpointer unused, gpointer unused2)
{
	int pos;
	char *current_file;

	pos = audacious_drct_pl_get_pos();
	current_file = audacious_drct_pl_get_file(pos);

	do_command(cmd_line_after, current_file, pos);

	g_free(current_file);
}

static void
songchange_playback_ttc(gpointer plentry_gp, gpointer prevs_gp)
{
  if ( ( aud_ip_state->playing ) && ( strcmp(cmd_line_ttc,"") ) )
  {
    songchange_playback_ttc_prevs_t *prevs = prevs_gp;
    PlaylistEntry *pl_entry = plentry_gp;

    /* same filename but title changed, useful to detect http stream song changes */

    if ( ( prevs->title != NULL ) && ( prevs->filename != NULL ) )
    {
      if ( ( pl_entry->filename != NULL ) && ( !strcmp(pl_entry->filename,prevs->filename) ) )
      {
        if ( ( pl_entry->title != NULL ) && ( strcmp(pl_entry->title,prevs->title) ) )
        {
          int pos = audacious_drct_pl_get_pos();
          char *current_file = audacious_drct_pl_get_file(pos);
          do_command(cmd_line_ttc, current_file, pos);
          g_free(current_file);
          g_free(prevs->title);
          prevs->title = g_strdup(pl_entry->title);
        }
      }
      else
      {
        g_free(prevs->filename);
        prevs->filename = g_strdup(pl_entry->filename);
        /* if filename changes, reset title as well */
        if ( prevs->title != NULL )
          g_free(prevs->title);
        prevs->title = g_strdup(pl_entry->title);
      }
    }
    else
    {
      if ( prevs->title != NULL )
        g_free(prevs->title);
      prevs->title = g_strdup(pl_entry->title);
      if ( prevs->filename != NULL )
        g_free(prevs->filename);
      prevs->filename = g_strdup(pl_entry->filename);
    }
  }
}

static void
songchange_playlist_eof(gpointer unused, gpointer unused2)
{
	int pos;
	char *current_file;

	pos = audacious_drct_pl_get_pos();
	current_file = audacious_drct_pl_get_file(pos);

	do_command(cmd_line_end, current_file, pos);

	g_free(current_file);
}

