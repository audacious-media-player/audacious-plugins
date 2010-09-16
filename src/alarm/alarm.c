/*
 * Copyright (C) Adam Feakin <adamf@snika.uklinux.net>
 *
 * modified by Daniel Stodden <stodden@in.tum.de>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

/*
 * xmms-alarm.c - a XMMS plugin which allows you to set a time for it to
 * start playing a playlist with the volume fading up and more things
 * the next time I get bored
 */

#if STDC_HEADERS
# include <stdlib.h>
#endif

#include <time.h>
#if TM_IN_SYS_TIME
# include <sys/time.h>
#endif

#include <string.h>
#include <stdio.h>

#include <gdk/gdk.h>
#include <gtk/gtk.h>
#include <pthread.h>
#include <assert.h>
#include <math.h>

#include <audacious/configdb.h>
#include <audacious/debug.h>
#include <audacious/drct.h>
#include <audacious/plugin.h>

#include "alarm.h"
#include "interface.h"
#include "callbacks.h"

static pthread_t start_tid;	       /* thread id of alarm loop */
static pthread_t stop_tid;	       /* thread id of stop loop */
static pthread_mutex_t fader_lock = PTHREAD_MUTEX_INITIALIZER;

static GeneralPlugin alarm_plugin;

/* string tokens to allow loops and shorten code */
static char day_cb[7][7] = {"sun_cb", "mon_cb", "tue_cb",
                           "wed_cb", "thu_cb", "fri_cb", "sat_cb"};

static char day_flags[7][10] = {"sun_flags", "mon_flags", "tue_flags",
                              "wed_flags", "thu_flags", "fri_flags", "sat_flags"};

static char day_h[7][6] = {"sun_h", "mon_h", "tue_h",
                          "wed_h", "thu_h", "fri_h", "sat_h"};

static char day_m[7][6] = {"sun_m", "mon_m", "tue_m",
                          "wed_m", "thu_m", "fri_m", "sat_m"};

static char day_def[7][8] = {"sun_def", "mon_def", "tue_def",
                            "wed_def", "thu_def", "fri_def", "sat_def"};

static struct
{
   GtkSpinButton *alarm_h;
   GtkSpinButton *alarm_m;

   GtkToggleButton *stop_on;
   GtkSpinButton *stop_h;
   GtkSpinButton *stop_m;

   GtkRange *volume;
   GtkRange *quietvol;

   GtkSpinButton *fading;

   GtkEntry *cmdstr;
   GtkToggleButton *cmd_on;

   GtkEntry *playlist;

   int default_hour;
   int default_min;

   // array allows looping of days
   alarmday day[7];


   GtkEntry *reminder;
   GtkToggleButton *reminder_cb;
   gchar *reminder_msg;
   gboolean reminder_on;
}
alarm_conf;

static gint alarm_h, alarm_m;

static gboolean stop_on;
static gint stop_h, stop_m;

static gint volume, quietvol;

static gint fading;

static gchar *cmdstr = NULL;
static gboolean cmd_on;

static gchar *playlist = NULL;

static GtkWidget *config_dialog = NULL;
static GtkWidget *alarm_dialog = NULL;

static GtkWidget *lookup_widget(GtkWidget *w, const gchar *name)
{
   GtkWidget *widget;

   widget = (GtkWidget*) gtk_object_get_data(GTK_OBJECT(w),
					     name);
   g_return_val_if_fail(widget != NULL, NULL);

   return widget;
}

static void dialog_destroyed(GtkWidget *dialog, gpointer data)
{
   AUDDBG("dialog destroyed\n");
   *(GtkObject**)data = NULL;
}

static inline gboolean dialog_visible(GtkWidget *dialog)
{
   return(((dialog != NULL) && GTK_WIDGET_VISIBLE(dialog)));
}

/*
 * tell the user about that bug
 */
static void alarm_warning(void)
{

   static GtkWidget *warning_dialog = NULL;

   if(dialog_visible(warning_dialog))
     return;

   warning_dialog = create_warning_dialog();

   gtk_signal_connect(GTK_OBJECT(warning_dialog), "destroy",
		      GTK_SIGNAL_FUNC(dialog_destroyed), &warning_dialog);

   gtk_widget_show_all(warning_dialog);

   return;
}

/*
 * the callback function that is called when the save button is
 * pressed saves configuration to ~/.bmp/alarmconfig
 */
void alarm_save(GtkButton *w, gpointer data)
{
   int daynum = 0;  // used to identify day number
   mcs_handle_t *conf;

   AUDDBG("alarm_save\n");

   conf = aud_cfg_db_open();

   /*
    * update the live values and write them out
    */
   alarm_h = alarm_conf.default_hour =
     gtk_spin_button_get_value_as_int(alarm_conf.alarm_h);
   aud_cfg_db_set_int(conf, "alarm", "alarm_h", alarm_h);

   alarm_m = alarm_conf.default_min =
     gtk_spin_button_get_value_as_int(alarm_conf.alarm_m);
   aud_cfg_db_set_int(conf, "alarm", "alarm_m", alarm_m);


   stop_h =
     gtk_spin_button_get_value_as_int( alarm_conf.stop_h);

   stop_m =
     gtk_spin_button_get_value_as_int(alarm_conf.stop_m);

   stop_on =
     gtk_toggle_button_get_active(alarm_conf.stop_on);

   /* days of the week */
   for(; daynum < 7; daynum++)
   {
     if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(alarm_conf.day[daynum].cb)))
       alarm_conf.day[daynum].flags = 0;
     else
       alarm_conf.day[daynum].flags = ALARM_OFF;

     if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(alarm_conf.day[daynum].cb_def)))
       alarm_conf.day[daynum].flags |= ALARM_DEFAULT;

     alarm_conf.day[daynum].hour =
       gtk_spin_button_get_value_as_int(alarm_conf.day[daynum].spin_hr);
     alarm_conf.day[daynum].min =
       gtk_spin_button_get_value_as_int(alarm_conf.day[daynum].spin_min);

     aud_cfg_db_set_int(conf, "alarm", day_flags[daynum], alarm_conf.day[daynum].flags);
     aud_cfg_db_set_int(conf, "alarm", day_h[daynum], alarm_conf.day[daynum].hour);
     aud_cfg_db_set_int(conf, "alarm", day_m[daynum], alarm_conf.day[daynum].min);
   }

   /* END: days of week */

   volume =
     gtk_range_get_adjustment(alarm_conf.volume)->value;
   aud_cfg_db_set_int(conf, "alarm", "volume", volume);

   quietvol =
     gtk_range_get_adjustment(alarm_conf.quietvol)->value;
   aud_cfg_db_set_int(conf, "alarm", "quietvol", quietvol);

   fading =
     gtk_spin_button_get_value_as_int(alarm_conf.fading);
   //xmms_cfg_write_int(conf, "alarm", "fading", fading);

   /* lets check to see if we need to show the bug warning */
   if((stop_on == TRUE) &&
      ((((stop_h * 60) + stop_m) * 60) < (fading + 65)))
   {
	   AUDDBG("Displaying bug warning, stop %dh %dm, fade %d\n",
	         stop_h, stop_m, fading);
	   alarm_warning();
   }
   else if((stop_on == TRUE) && (fading < 10))
   {
     AUDDBG("Displaying bug warning, stop %dh %dm, fade %d\n",
           stop_h, stop_m, fading);
     alarm_warning();
   }
   else
   {
	   /* write the new values */
	   aud_cfg_db_set_int(conf, "alarm", "stop_h", stop_h);
	   aud_cfg_db_set_int(conf, "alarm", "stop_m", stop_m);
	   aud_cfg_db_set_int(conf, "alarm", "fading", fading);
	   aud_cfg_db_set_bool(conf, "alarm", "stop_on", stop_on);
   }


   g_free(cmdstr);
   cmdstr = gtk_editable_get_chars(GTK_EDITABLE(alarm_conf.cmdstr),
				   0, -1);
   aud_cfg_db_set_string(conf, "alarm", "cmdstr", cmdstr);

   cmd_on =
     gtk_toggle_button_get_active(alarm_conf.cmd_on);
   aud_cfg_db_set_bool(conf, "alarm", "cmd_on", cmd_on);

   g_free(playlist);
   playlist = gtk_editable_get_chars(GTK_EDITABLE(alarm_conf.playlist),
				     0, -1);
   aud_cfg_db_set_string(conf, "alarm", "playlist", playlist);

   /* reminder */
   g_free(alarm_conf.reminder_msg);
   alarm_conf.reminder_msg = gtk_editable_get_chars(GTK_EDITABLE(alarm_conf.reminder),
       0, -1);
   aud_cfg_db_set_string(conf, "alarm", "reminder_msg", alarm_conf.reminder_msg);

   alarm_conf.reminder_on =
     gtk_toggle_button_get_active(alarm_conf.reminder_cb);
   aud_cfg_db_set_bool(conf, "alarm", "reminder_on", alarm_conf.reminder_on);

   aud_cfg_db_close(conf);
}

/*
 * read the current configuration from the file
 */
static void alarm_read_config()
{
   int daynum = 0;   // used for day number
   mcs_handle_t *conf;

   AUDDBG("alarm_read_config\n");

   conf = aud_cfg_db_open();

   if(!aud_cfg_db_get_int(conf, "alarm", "alarm_h", &alarm_h))
     alarm_h = DEFAULT_ALARM_HOUR;
   if(!aud_cfg_db_get_int(conf, "alarm", "alarm_m", &alarm_m))
     alarm_m = DEFAULT_ALARM_MIN;

   /* save them here too */
   alarm_conf.default_hour = alarm_h;
   alarm_conf.default_min = alarm_m;

   if(!aud_cfg_db_get_int( conf, "alarm", "stop_h", &stop_h))
     stop_h = DEFAULT_STOP_HOURS;
   if(!aud_cfg_db_get_int( conf, "alarm", "stop_m", &stop_m))
     stop_m = DEFAULT_STOP_MINS;
   if(!aud_cfg_db_get_bool(conf, "alarm", "stop_on", &stop_on))
     stop_on = TRUE;

   if(!aud_cfg_db_get_int(conf, "alarm", "volume", &volume))
     volume = DEFAULT_VOLUME;
   if(!aud_cfg_db_get_int(conf, "alarm", "quietvol", &quietvol))
     quietvol = DEFAULT_QUIET_VOL;

   if(!aud_cfg_db_get_int(conf, "alarm", "fading", &fading))
     fading = DEFAULT_FADING;

   g_free(cmdstr);
   if(!aud_cfg_db_get_string(conf, "alarm", "cmdstr", &cmdstr))
     cmdstr = g_strdup("");
   if(!aud_cfg_db_get_bool(conf, "alarm", "cmd_on", &cmd_on))
     cmd_on = FALSE;

   g_free(playlist);
   if(!aud_cfg_db_get_string(conf, "alarm", "playlist", &playlist))
     playlist = g_strdup("");

   g_free(alarm_conf.reminder_msg);
   if(!aud_cfg_db_get_string(conf, "alarm", "reminder_msg", &alarm_conf.reminder_msg))
     alarm_conf.reminder_msg = g_strdup("");
   if(!aud_cfg_db_get_bool(conf, "alarm", "reminder_on", &alarm_conf.reminder_on))
     alarm_conf.reminder_on = FALSE;

   /* day flags and times */
   for(; daynum < 7; daynum++)
   {
     /* read the flags */
     if(!aud_cfg_db_get_int(conf, "alarm", day_flags[daynum], &alarm_conf.day[daynum].flags)) {
       // only turn alarm off by default on a sunday
       if(daynum != 0)
         alarm_conf.day[daynum].flags = DEFAULT_FLAGS;
       else
         alarm_conf.day[daynum].flags = DEFAULT_FLAGS | ALARM_OFF;
     }

     /* read the times */
     if(!aud_cfg_db_get_int(conf, "alarm", day_h[daynum], &alarm_conf.day[daynum].hour))
       alarm_conf.day[daynum].hour = DEFAULT_ALARM_HOUR;

     if(!aud_cfg_db_get_int(conf, "alarm", day_m[daynum], &alarm_conf.day[daynum].min))
       alarm_conf.day[daynum].min = DEFAULT_ALARM_MIN;
   }

   aud_cfg_db_close(conf);
   AUDDBG("END alarm_read_config\n");
}

/*
 * display an about box
 */
static void alarm_about()
{
   static GtkWidget *about_dialog = NULL;

   AUDDBG("alarm_about\n");

   if(dialog_visible(about_dialog))
     return;

   about_dialog = create_about_dialog();

   gtk_signal_connect(GTK_OBJECT(about_dialog), "destroy",
		      GTK_SIGNAL_FUNC(dialog_destroyed), &about_dialog);

   gtk_widget_show_all(about_dialog);

   return;
}

/*
 * create a playlist file selection dialog
 */
static void alarm_playlist_browse(GtkButton *button, gpointer data)
{
   GtkWidget *fs;
   gchar *dirname, *path;

   dirname = g_dirname(playlist);
   AUDDBG("dirname = %s\n", dirname);
   path = g_strdup_printf("%s/", dirname);
   AUDDBG("path = %s\n", path);
   g_free(dirname);

   fs = create_playlist_fileselection();

   gtk_file_selection_set_filename(GTK_FILE_SELECTION(fs), path);
   g_free(path);

   gtk_widget_show_all(fs);
}

/*
 * save selected playlist to the corresponding text entry
 */
void alarm_store_playlistname(GtkButton *button, gpointer data)
{
   GtkFileSelection *fs = GTK_FILE_SELECTION(data);
   gchar *plist;

   AUDDBG("alarm_store_playlistname\n");

   plist = g_strdup(gtk_file_selection_get_filename(fs));

   gtk_entry_set_text(alarm_conf.playlist, plist);
   g_free(plist);
}

/*
 * displays the configuration window and opens the config file.
 */
static void alarm_configure(void)
{
   int daynum = 0;  // used to loop days
   GtkWidget *w;

   AUDDBG("alarm_configure\n");

   /*
    * dont want to show more than one config window
    */
   if(dialog_visible(config_dialog))
     return;

   alarm_read_config();

   /*
    * Create the widgets
    */
   config_dialog = create_config_dialog();

   w = lookup_widget(config_dialog, "alarm_h_spin");
   alarm_conf.alarm_h = GTK_SPIN_BUTTON(w);
   gtk_spin_button_set_value(alarm_conf.alarm_h, alarm_h);

   w = lookup_widget(config_dialog, "alarm_m_spin");
   alarm_conf.alarm_m =  GTK_SPIN_BUTTON(w);
   gtk_spin_button_set_value(alarm_conf.alarm_m, alarm_m);

   w = lookup_widget(config_dialog, "stop_h_spin");
   alarm_conf.stop_h = GTK_SPIN_BUTTON(w);
   gtk_spin_button_set_value(alarm_conf.stop_h, stop_h);

   w = lookup_widget(config_dialog, "stop_m_spin");
   alarm_conf.stop_m = GTK_SPIN_BUTTON(w);
   gtk_spin_button_set_value(alarm_conf.stop_m, stop_m);

   w = lookup_widget(config_dialog, "stop_checkb");
   alarm_conf.stop_on = GTK_TOGGLE_BUTTON(w);
   gtk_toggle_button_set_active(alarm_conf.stop_on, stop_on);

   w = lookup_widget(config_dialog, "vol_scale");
   alarm_conf.volume = GTK_RANGE(w);
   gtk_range_set_adjustment(alarm_conf.volume,
			    GTK_ADJUSTMENT(gtk_adjustment_new(volume,
							      0,
							      100, 1,
							      5, 0)));

   w = lookup_widget(config_dialog, "quiet_vol_scale");
   alarm_conf.quietvol = GTK_RANGE(w);
   gtk_range_set_adjustment(alarm_conf.quietvol,
			    GTK_ADJUSTMENT(gtk_adjustment_new(quietvol,
							      0,
							      100, 1,
							      5, 0)));

   /* days of week */
   for(; daynum < 7; daynum++)
   {
     w = lookup_widget(config_dialog, day_cb[daynum]);
     alarm_conf.day[daynum].cb = GTK_CHECK_BUTTON(w);
     gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(alarm_conf.day[daynum].cb),
				!(alarm_conf.day[daynum].flags & ALARM_OFF));

     w = lookup_widget(config_dialog, day_def[daynum]);
     alarm_conf.day[daynum].cb_def = GTK_CHECK_BUTTON(w);
     gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(alarm_conf.day[daynum].cb_def),
         alarm_conf.day[daynum].flags & ALARM_DEFAULT);


     /* Changed to show default time instead of set time when ALARM_DEFAULT set,
      * as suggested by Mark Brown
      */
/*     w = lookup_widget(config_dialog, day_h[daynum]);
     alarm_conf.day[daynum].spin_hr = GTK_SPIN_BUTTON(w);
     gtk_spin_button_set_value(alarm_conf.day[daynum].spin_hr, alarm_conf.day[daynum].hour);

     w = lookup_widget(config_dialog, day_m[daynum]);
     alarm_conf.day[daynum].spin_min = GTK_SPIN_BUTTON(w);
     gtk_spin_button_set_value(alarm_conf.day[daynum].spin_min, alarm_conf.day[daynum].min);
*/
     if(alarm_conf.day[daynum].flags & ALARM_DEFAULT)
     {
       w = lookup_widget(config_dialog, day_h[daynum]);
       alarm_conf.day[daynum].spin_hr = GTK_SPIN_BUTTON(w);
       gtk_spin_button_set_value(alarm_conf.day[daynum].spin_hr, alarm_conf.default_hour);

       w = lookup_widget(config_dialog, day_m[daynum]);
       alarm_conf.day[daynum].spin_min = GTK_SPIN_BUTTON(w);
       gtk_spin_button_set_value(alarm_conf.day[daynum].spin_min, alarm_conf.default_min);

       gtk_widget_set_sensitive((GtkWidget *)alarm_conf.day[daynum].spin_hr, FALSE);
       gtk_widget_set_sensitive((GtkWidget *)alarm_conf.day[daynum].spin_min, FALSE);
     }
     else
     {
       w = lookup_widget(config_dialog, day_h[daynum]);
       alarm_conf.day[daynum].spin_hr = GTK_SPIN_BUTTON(w);
       gtk_spin_button_set_value(alarm_conf.day[daynum].spin_hr, alarm_conf.day[daynum].hour);

       w = lookup_widget(config_dialog, day_m[daynum]);
       alarm_conf.day[daynum].spin_min = GTK_SPIN_BUTTON(w);
       gtk_spin_button_set_value(alarm_conf.day[daynum].spin_min, alarm_conf.day[daynum].min);

       gtk_widget_set_sensitive((GtkWidget *)alarm_conf.day[daynum].spin_hr, TRUE);
       gtk_widget_set_sensitive((GtkWidget *)alarm_conf.day[daynum].spin_min, TRUE);
     }
   }

   /* END: days of week */

   w = lookup_widget(config_dialog,"fading_spin");
   alarm_conf.fading = GTK_SPIN_BUTTON(w);
   gtk_spin_button_set_value(alarm_conf.fading, fading);

   w = lookup_widget(config_dialog, "cmd_entry");
   alarm_conf.cmdstr = GTK_ENTRY(w);
   gtk_entry_set_text(alarm_conf.cmdstr, cmdstr);

   w = lookup_widget(config_dialog, "cmd_checkb");
   alarm_conf.cmd_on = GTK_TOGGLE_BUTTON(w);
   gtk_toggle_button_set_active(alarm_conf.cmd_on, cmd_on);

   w = lookup_widget(config_dialog, "playlist");
   alarm_conf.playlist = GTK_ENTRY(w);
   gtk_entry_set_text(alarm_conf.playlist, playlist);

   w = lookup_widget(config_dialog, "reminder_text");
   alarm_conf.reminder = GTK_ENTRY(w);
   gtk_entry_set_text(alarm_conf.reminder, alarm_conf.reminder_msg);

   w = lookup_widget(config_dialog, "reminder_cb");
   alarm_conf.reminder_cb = GTK_TOGGLE_BUTTON(w);
   gtk_toggle_button_set_active(alarm_conf.reminder_cb, alarm_conf.reminder_on);

   w = lookup_widget(config_dialog, "playlist_browse_button");
   gtk_signal_connect(GTK_OBJECT(w), "clicked",
		      GTK_SIGNAL_FUNC(alarm_playlist_browse), NULL);

   gtk_signal_connect(GTK_OBJECT(config_dialog), "destroy",
		      GTK_SIGNAL_FUNC(dialog_destroyed), &config_dialog);

   gtk_widget_show_all(config_dialog);

   AUDDBG("END alarm_configure\n");
}

/* functions for greying out the time for days */
void on_day_def_toggled(GtkToggleButton *togglebutton, gpointer user_data, int daynum)
{
   GtkWidget *w;

   /* change the time shown too */
   w = lookup_widget(config_dialog, day_h[daynum]);
   if(w == NULL)
     return;

   if(gtk_toggle_button_get_active(togglebutton) == TRUE)
   {
     gtk_spin_button_set_value(GTK_SPIN_BUTTON(w), alarm_conf.default_hour);
     gtk_widget_set_sensitive(w, FALSE);
   }
   else
   {
     gtk_spin_button_set_value(GTK_SPIN_BUTTON(w), alarm_conf.day[daynum].hour);
     gtk_widget_set_sensitive(w, TRUE);
   }

   w = lookup_widget(config_dialog, day_m[daynum]);
   if(gtk_toggle_button_get_active(togglebutton) == TRUE)
   {
     gtk_spin_button_set_value(GTK_SPIN_BUTTON(w), alarm_conf.default_min);
     gtk_widget_set_sensitive(w, FALSE);
   }
   else
   {
     gtk_spin_button_set_value(GTK_SPIN_BUTTON(w), alarm_conf.day[daynum].min);
     gtk_widget_set_sensitive(w, TRUE);
   }
}

void on_sun_def_toggled(GtkToggleButton *togglebutton, gpointer user_data)
{
   on_day_def_toggled(togglebutton, user_data, 0);
}

void on_mon_def_toggled(GtkToggleButton *togglebutton, gpointer user_data)
{
   on_day_def_toggled(togglebutton, user_data, 1);
}

void on_tue_def_toggled(GtkToggleButton *togglebutton, gpointer user_data)
{
   on_day_def_toggled(togglebutton, user_data, 2);
}

void on_wed_def_toggled(GtkToggleButton *togglebutton, gpointer user_data)
{
   on_day_def_toggled(togglebutton, user_data, 3);
}

void on_thu_def_toggled(GtkToggleButton *togglebutton, gpointer user_data)
{
   on_day_def_toggled(togglebutton, user_data, 4);
}

void on_fri_def_toggled(GtkToggleButton *togglebutton, gpointer user_data)
{
   on_day_def_toggled(togglebutton, user_data, 5);
}

void on_sat_def_toggled(GtkToggleButton *togglebutton, gpointer user_data)
{
   on_day_def_toggled(togglebutton, user_data, 6);
}

/* END: greying things */

void alarm_current_volume(GtkButton *button, gpointer data)
{
   gint vol;
   GtkAdjustment *adj;

   AUDDBG("on_current_button_clicked\n");

   aud_drct_get_volume_main(&vol);

   adj = gtk_range_get_adjustment(alarm_conf.volume);
   gtk_adjustment_set_value(adj, (gfloat)vol);
}

/*
 * a thread safe sleeping function -
 * and it even works in solaris (I think)
 */
static void threadsleep(float x)
{
   AUDDBG("threadsleep: waiting %f seconds\n", x);

   g_usleep((int) ((float) x * (float) 1000000.0));

   return;
}

static inline pthread_t alarm_thread_create(void *(*start_routine)(void *), void *args, unsigned int detach)
{
   pthread_t tid;
   pthread_attr_t attr;

   pthread_attr_init(&attr);

   if(detach != 0)
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

   pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);
   pthread_attr_setschedpolicy(&attr, SCHED_OTHER);
   pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);

   pthread_create(&tid, &attr, start_routine, args);

   return(tid);
}

static void *alarm_fade(void *arg)
{
   fader *vols = (fader *)arg;
   guint i;
   gint v;
   gint inc, diff, adiff;

   /* lock */
   pthread_mutex_lock(&fader_lock);

   /* slide volume */
   /* the Kaspar Giger way of fading, check the current mixer volume and
    * increment from there so that if you have some other app lowering the
    * volume at the same time xmms-alarm will not ignore it.  If you have some
    * other app increasing the volume, then it could get louder that you expect
    * though - because the loop does not recalculate the difference each time.
    */

   /* difference between the 2 volumes */
   diff = vols->end - vols->start;
   adiff = abs(diff);

   /* Are we going up or down? */
   if(diff < 0)
     inc = -1;
   else
     inc = 1;

   aud_drct_set_volume_main((gint)vols->start);
   //for(i=0;i<(vols->end - vols->start);i++)
   for(i=0;i<adiff;i++)
   {
     //threadsleep((gfloat)fading / (vols->end - vols->start));
     threadsleep((gfloat)fading / (gfloat)adiff);
     aud_drct_get_volume_main(&v);
     aud_drct_set_volume_main(v + inc);
   }
   /* Setting the volume to the end volume sort of defeats the point if having
    * the code in there to allow other apps to control volume too :)
    */
   //aud_drct_set_volume_main((gint)vols->end);

   /* and */
   pthread_mutex_unlock(&fader_lock);

   AUDDBG("volume = %f%%\n", (gdouble)vols->end);
   return(0);
}

static void *alarm_stop_thread( void *args )
{
   gint currvol;
   fader fade_vols;
   pthread_t f_tid;

   AUDDBG("alarm_stop_thread\n");


   /* sleep for however long we are meant to be sleeping for until
    * its time to shut up
    */
   threadsleep(((stop_h * 60) + stop_m) * 60);

   AUDDBG("alarm_stop triggered\n");

   if (dialog_visible(alarm_dialog))
     gtk_widget_destroy(alarm_dialog);

   aud_drct_get_volume_main(&currvol),

   /* fade back to zero */
   fade_vols.start = currvol;
   fade_vols.end = 0;

   /* The fader thread locks the fader_mutex now */
   f_tid = alarm_thread_create(alarm_fade, &fade_vols, 0);

   pthread_join(f_tid, NULL);
   aud_drct_stop();

   /* might as well set the volume to something higher than zero so we
    * dont confuse the poor people who just woke up and cant work out why
    * theres no music playing when they press the little play button :)
    */
   aud_drct_set_volume_main(currvol);

   AUDDBG("alarm_stop done\n");
   return(NULL);
}

void alarm_stop_cancel(GtkButton *w, gpointer data)
{
   AUDDBG("alarm_stop_cancel\n");
   pthread_cancel(stop_tid);
}

/* the main alarm thread */
static void *alarm_start_thread(void *args)
{
   struct tm *currtime;
   time_t timenow;
   unsigned int play_start = 0;
   guint today;

   /* give it time to set start_tid to something */
   threadsleep(1);

   while(start_tid != 0)
   {
     /* sit around and wait for the faders to not be doing anything */
     AUDDBG("Waiting for fader to be unlocked..");
     pthread_mutex_lock(&fader_lock);
     AUDDBG("Ok\n");
     pthread_mutex_unlock(&fader_lock);

     AUDDBG("Getting time\n");
     timenow = time(NULL);
     currtime = localtime(&timenow);
     today = currtime->tm_wday;
     AUDDBG("Today is %d\n", today);

     /* see if its time to do something */
     AUDDBG("Checking Day\n");

     /* Had to put something here so I put the hour string.
     ** Its only debug stuff anyway */
     AUDDBG("%s",day_h[today]);

     if(alarm_conf.day[today].flags & ALARM_OFF)
     {
       threadsleep(8.5);
       continue;
     }
     else
     {
       /* set the alarm_h and alarm_m for today, if not default */
       if(!(alarm_conf.day[today].flags & ALARM_DEFAULT))
       {
         alarm_h = alarm_conf.day[today].hour;
         alarm_m = alarm_conf.day[today].min;
       }
       else
       {
         alarm_h = alarm_conf.default_hour;
         alarm_m = alarm_conf.default_min;
       }
     }

     AUDDBG("Alarm time is %d:%d (def: %d:%d)\n", alarm_h, alarm_m,
        alarm_conf.default_hour, alarm_conf.default_min);

     AUDDBG("Checking time (%d:%d)\n", currtime->tm_hour, currtime->tm_min);
     if((currtime->tm_hour != alarm_h) || (currtime->tm_min != alarm_m))
     {
       threadsleep(8.5);
       continue;
     }

     if(cmd_on == TRUE)
     {
       AUDDBG("Executing %s, cmd_on is true\n", cmdstr);
       if(system(cmdstr) == -1)
       {
        AUDDBG("Executing %s failed\n",cmdstr);
       }
     }

     AUDDBG("strcmp playlist, playlist is [%s]\n", playlist);
     if(strcmp(playlist, ""))
     {
       AUDDBG("playlist is not blank, aparently\n");
       GList list;

       list.prev = list.next = NULL;
       list.data = playlist;

       aud_drct_pl_clear();
       aud_drct_pl_add_list (& list, -1);
     }

     if(fading)
     {
       fader fade_vols;

       AUDDBG("Fading is true\n");
       aud_drct_set_volume_main(quietvol);

       /* start playing */
       play_start = time(NULL);
       aud_drct_play();

       /* fade volume */
       fade_vols.start = quietvol;
       fade_vols.end = volume;

       //alarm_fade(quietvol, volume);
       alarm_thread_create(alarm_fade, &fade_vols, 0);
     }
     else
     {
       /* no fading */

       /* set volume */
       aud_drct_set_volume_main(volume);

       /* start playing */
       play_start = time(NULL);
       aud_drct_play();
     }

     if(alarm_conf.reminder_on == TRUE)
     {
       GtkWidget *reminder_dialog;
       AUDDBG("Showing reminder '%s'\n", alarm_conf.reminder_msg);

       GDK_THREADS_ENTER();
       reminder_dialog = (GtkWidget*) create_reminder_dialog(alarm_conf.reminder_msg);
       gtk_signal_connect(GTK_OBJECT(reminder_dialog), "destroy",
       GTK_SIGNAL_FUNC(dialog_destroyed), &reminder_dialog);
       gtk_widget_show_all(reminder_dialog);
       GDK_THREADS_LEAVE();
     }

     /* bring up the wakeup call dialog if stop_on is set TRUE, this
      * has been moved to after making xmms play so that it doesnt
      * get in the way for people with manual window placement turned on
      *
      * this means that the dialog doesnt get shown until the volume has
      * finished fading though !, so thats something else to fix
      */
      if(stop_on == TRUE)
      {
         /* ok, so when we want to open dialogs in threaded programs
          * we use this do we?
          * anyone?
          */
          GDK_THREADS_ENTER();
          {
            AUDDBG("stop_on is true\n");
            alarm_dialog = create_alarm_dialog();
            AUDDBG("created alarm dialog, %p\n", alarm_dialog);

            gtk_signal_connect(GTK_OBJECT(alarm_dialog), "destroy",
            GTK_SIGNAL_FUNC(dialog_destroyed), &alarm_dialog);
            AUDDBG("attached destroy signal to alarm dialog, %p\n", alarm_dialog);
            gtk_widget_show_all(alarm_dialog);
            AUDDBG("dialog now showing\n");

            AUDDBG("now starting stop thread\n");
            stop_tid = alarm_thread_create(alarm_stop_thread, NULL, 0);
            AUDDBG("Created wakeup dialog and started stop thread(%d)\n", (int)stop_tid);

	        }
	        GDK_THREADS_LEAVE();

          /* now wait for the stop thread */
          AUDDBG("Waiting for stop to stop.... (%d)", (int)stop_tid);
          pthread_join(stop_tid, NULL);
          /* loop until we are out of the starting minute */
          while(time(NULL) < (play_start + 61))
          {
            AUDDBG("Waiting until out of starting minute\n");
            threadsleep(5.0);
          }
          AUDDBG("OK\n");
      }
      /* loop until we are out of the starting minute */
      while(time(NULL) < (play_start + 61))
      {
        threadsleep(5.0);
      }
      threadsleep(fading);
   }

   AUDDBG("Main thread has gone...\n");
   return NULL;
}

/*
 * initialization
 * opens the config file and reads the value, creates a new
 * config in memory if the file doesnt exist and sets default vals
 */
static void alarm_init()
{
   AUDDBG("alarm_init\n");

   alarm_conf.reminder_msg = NULL;
   alarm_read_config();

   /* start the main thread running */
   start_tid = alarm_thread_create(alarm_start_thread, NULL, 1);
}

/*
 * kill the main thread
 */
static void alarm_cleanup()
{
   AUDDBG("alarm_cleanup\n");

   if (start_tid)
     pthread_cancel(start_tid);
   start_tid = 0;
   if(stop_tid)
     pthread_cancel(stop_tid);
   stop_tid = 0;

   g_free(alarm_conf.reminder_msg);
   alarm_conf.reminder_msg = NULL;
   g_free(playlist);
   playlist = NULL;
   g_free(cmdstr);
   cmdstr = NULL;
}

static GeneralPlugin alarm_plugin =
{
     .description = "Alarm "VERSION,
     .init = alarm_init,
     .about = alarm_about,
     .configure = alarm_configure,
     .cleanup = alarm_cleanup,
};

GeneralPlugin *alarm_gplist[] = { &alarm_plugin, NULL };
SIMPLE_GENERAL_PLUGIN(alarm, alarm_gplist);
