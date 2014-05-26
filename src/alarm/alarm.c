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

#include <stdlib.h>

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

#include <audacious/debug.h>
#include <audacious/drct.h>
#include <audacious/i18n.h>
#include <audacious/misc.h>
#include <audacious/plugin.h>
#include <libaudgui/libaudgui-gtk.h>

#include "alarm.h"
#include "interface.h"
#include "callbacks.h"

static const gchar * const alarm_defaults[] = {
 /* general */
 "alarm_h", "6",
 "alarm_m", "30",
 "cmd_on", "FALSE",
 "fading", "60",
 "quietvol", "25",
 "reminder_on", "FALSE",
 "stop_h", "1",
 "stop_m", "0",
 "stop_on", "TRUE",
 "volume", "80",

 /* days */
 "sun_flags", "3",
 "sun_h", "6",
 "sun_m", "30",
 "mon_flags", "2",
 "mon_h", "6",
 "mon_m", "30",
 "tue_flags", "2",
 "tue_h", "6",
 "tue_m", "30",
 "wed_flags", "2",
 "wed_h", "6",
 "wed_m", "30",
 "thu_flags", "2",
 "thu_h", "6",
 "thu_m", "30",
 "fri_flags", "2",
 "fri_h", "6",
 "fri_m", "30",
 "sat_flags", "2",
 "sat_h", "6",
 "sat_m", "30",
 NULL};

typedef struct
{
    pthread_t tid;
    volatile gboolean  is_valid;
} alarm_thread_t;

static gint timeout_source;
static time_t play_start;

static alarm_thread_t stop;     /* thread id of stop loop */
static pthread_mutex_t fader_lock = PTHREAD_MUTEX_INITIALIZER;

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
    gboolean reminder_on;
}
alarm_conf;

static gint alarm_h, alarm_m;

static gboolean stop_on;
static gint stop_h, stop_m;

static gint volume, quietvol;

static gint fading;

static gboolean cmd_on;

static GtkWidget *config_dialog = NULL;
static GtkWidget *alarm_dialog = NULL;

static GtkWidget *lookup_widget(GtkWidget *w, const gchar *name)
{
    GtkWidget * widget = g_object_get_data ((GObject *) w, name);
    g_return_val_if_fail(widget != NULL, NULL);

    return widget;
}

/*
 * the callback function that is called when the save button is
 * pressed saves configuration to ~/.bmp/alarmconfig
 */
void alarm_save(void)
{
    int daynum = 0;  // used to identify day number

    /*
     * update the live values and write them out
     */
    alarm_h = alarm_conf.default_hour = gtk_spin_button_get_value_as_int (alarm_conf.alarm_h);
    aud_set_int ("alarm", "alarm_h", alarm_h);

    alarm_m = alarm_conf.default_min = gtk_spin_button_get_value_as_int (alarm_conf.alarm_m);
    aud_set_int ("alarm", "alarm_m", alarm_m);

    stop_h = gtk_spin_button_get_value_as_int (alarm_conf.stop_h);
    stop_m = gtk_spin_button_get_value_as_int (alarm_conf.stop_m);
    stop_on = gtk_toggle_button_get_active (alarm_conf.stop_on);

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

        aud_set_int ("alarm", day_flags[daynum], alarm_conf.day[daynum].flags);
        aud_set_int ("alarm", day_h[daynum], alarm_conf.day[daynum].hour);
        aud_set_int ("alarm", day_m[daynum], alarm_conf.day[daynum].min);
    }

    /* END: days of week */

    volume = gtk_range_get_value (alarm_conf.volume);
    aud_set_int ("alarm", "volume", volume);

    quietvol = gtk_range_get_value (alarm_conf.quietvol);
    aud_set_int ("alarm", "quietvol", quietvol);

    fading = gtk_spin_button_get_value_as_int (alarm_conf.fading);

    /* write the new values */
    aud_set_int ("alarm", "stop_h", stop_h);
    aud_set_int ("alarm", "stop_m", stop_m);
    aud_set_int ("alarm", "fading", fading);
    aud_set_bool ("alarm", "stop_on", stop_on);

    char * cmdstr = gtk_editable_get_chars ((GtkEditable *) alarm_conf.cmdstr, 0, -1);
    aud_set_str ("alarm", "cmdstr", cmdstr);
    g_free (cmdstr);

    cmd_on = gtk_toggle_button_get_active (alarm_conf.cmd_on);
    aud_set_bool ("alarm", "cmd_on", cmd_on);

    char * playlist = gtk_editable_get_chars ((GtkEditable *) alarm_conf.playlist, 0, -1);
    aud_set_str ("alarm", "playlist", playlist);
    g_free (playlist);

    /* reminder */
    char * reminder_msg = gtk_editable_get_chars ((GtkEditable *) alarm_conf.reminder, 0, -1);
    aud_set_str ("alarm", "reminder_msg", reminder_msg);
    g_free (reminder_msg);

    alarm_conf.reminder_on = gtk_toggle_button_get_active (alarm_conf.reminder_cb);
    aud_set_bool ("alarm", "reminder_on", alarm_conf.reminder_on);
}

/*
 * read the current configuration from the file
 */
static void alarm_read_config(void)
{
    int daynum = 0;   // used for day number

    aud_config_set_defaults ("alarm", alarm_defaults);

    alarm_h = aud_get_int ("alarm", "alarm_h");
    alarm_m = aud_get_int ("alarm", "alarm_m");

    /* save them here too */
    alarm_conf.default_hour = alarm_h;
    alarm_conf.default_min = alarm_m;

    stop_h = aud_get_int ("alarm", "stop_h");
    stop_m = aud_get_int ("alarm", "stop_m");
    stop_on = aud_get_bool ("alarm", "stop_on");

    volume = aud_get_int ("alarm", "volume");
    quietvol = aud_get_int ("alarm", "quietvol");

    fading = aud_get_int ("alarm", "fading");

    cmd_on = aud_get_bool ("alarm", "cmd_on");

    alarm_conf.reminder_on = aud_get_bool ("alarm", "reminder_on");

    /* day flags and times */
    for(; daynum < 7; daynum++)
    {
        /* read the flags */
        alarm_conf.day[daynum].flags = aud_get_int ("alarm", day_flags[daynum]);

        /* read the times */
        alarm_conf.day[daynum].hour = aud_get_int ("alarm", day_h[daynum]);
        alarm_conf.day[daynum].min = aud_get_int ("alarm", day_m[daynum]);
    }
}

/*
 * displays the configuration window and opens the config file.
 */
static void alarm_configure(void)
{
    int daynum = 0;  // used to loop days
    GtkWidget *w;

    if (config_dialog)
    {
        gtk_window_present(GTK_WINDOW(config_dialog));
        return;
    }

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
     GTK_ADJUSTMENT(gtk_adjustment_new(volume, 0, 100, 1, 5, 0)));

    w = lookup_widget(config_dialog, "quiet_vol_scale");
    alarm_conf.quietvol = GTK_RANGE(w);
    gtk_range_set_adjustment(alarm_conf.quietvol,
     GTK_ADJUSTMENT(gtk_adjustment_new(quietvol, 0, 100, 1, 5, 0)));

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
/*      w = lookup_widget(config_dialog, day_h[daynum]);
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

    char * cmdstr = aud_get_str ("alarm", "cmdstr");
    w = lookup_widget(config_dialog, "cmd_entry");
    alarm_conf.cmdstr = GTK_ENTRY(w);
    gtk_entry_set_text(alarm_conf.cmdstr, cmdstr);
    str_unref (cmdstr);

    w = lookup_widget(config_dialog, "cmd_checkb");
    alarm_conf.cmd_on = GTK_TOGGLE_BUTTON(w);
    gtk_toggle_button_set_active(alarm_conf.cmd_on, cmd_on);

    char * playlist = aud_get_str ("alarm", "playlist");
    w = lookup_widget(config_dialog, "playlist");
    alarm_conf.playlist = GTK_ENTRY(w);
    gtk_entry_set_text(alarm_conf.playlist, playlist);
    str_unref (playlist);

    char * reminder_msg = aud_get_str ("alarm", "reminder_msg");
    w = lookup_widget(config_dialog, "reminder_text");
    alarm_conf.reminder = GTK_ENTRY(w);
    gtk_entry_set_text(alarm_conf.reminder, reminder_msg);
    str_unref (reminder_msg);

    w = lookup_widget(config_dialog, "reminder_cb");
    alarm_conf.reminder_cb = GTK_TOGGLE_BUTTON(w);
    gtk_toggle_button_set_active(alarm_conf.reminder_cb, alarm_conf.reminder_on);

    g_signal_connect (config_dialog, "destroy", (GCallback) gtk_widget_destroyed,
     & config_dialog);

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

static inline alarm_thread_t alarm_thread_create(void *(*start_routine)(void *), void *args, unsigned int detach)
{
    alarm_thread_t thrd;
    pthread_attr_t attr;

    pthread_attr_init(&attr);

    if(detach != 0)
        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);
    pthread_attr_setschedpolicy(&attr, SCHED_OTHER);
    pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);

    thrd.is_valid = (pthread_create(&thrd.tid, &attr, start_routine, args) == 0);

    return thrd;
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
     * volume at the same time, the alarm plugin will not ignore it.  If you have
     * some other app increasing the volume, then it could get louder than you expect
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
    return 0;
}

static void *alarm_stop_thread(void *args)
{
    gint currvol;
    fader fade_vols;
    alarm_thread_t f;

    AUDDBG("alarm_stop_thread\n");


    /* sleep for however long we are meant to be sleeping for until
     * its time to shut up
     */
    threadsleep(((stop_h * 60) + stop_m) * 60);

    AUDDBG("alarm_stop triggered\n");

    if (alarm_dialog)
        gtk_widget_destroy(alarm_dialog);

    aud_drct_get_volume_main(&currvol),

    /* fade back to zero */
    fade_vols.start = currvol;
    fade_vols.end = 0;

    /* The fader thread locks the fader_mutex now */
    f = alarm_thread_create(alarm_fade, &fade_vols, 0);

    pthread_join(f.tid, NULL);
    aud_drct_stop();

    /* might as well set the volume to something higher than zero so we
     * dont confuse the poor people who just woke up and cant work out why
     * theres no music playing when they press the little play button :)
     */
    aud_drct_set_volume_main(currvol);

    AUDDBG("alarm_stop done\n");
    return(NULL);
}

void alarm_stop_cancel(GtkWidget *w, gpointer data)
{
    AUDDBG("alarm_stop_cancel\n");
    if (pthread_cancel(stop.tid) == 0)
        stop.is_valid = FALSE;
}

/* the main alarm thread */
static gboolean alarm_timeout (void * unused)
{
    struct tm *currtime;
    time_t timenow;
    guint today;

    AUDDBG("Getting time\n");
    timenow = time(NULL);
    currtime = localtime(&timenow);
    today = currtime->tm_wday;
    AUDDBG("Today is %d\n", today);

    /* already went off? */
    if (timenow < play_start + 60)
        return TRUE;

    if(alarm_conf.day[today].flags & ALARM_OFF)
        return TRUE;
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
        return TRUE;

    if(cmd_on == TRUE)
    {
        char * cmdstr = aud_get_str ("alarm", "cmdstr");
        AUDDBG("Executing %s, cmd_on is true\n", cmdstr);
        if(system(cmdstr) == -1)
            AUDDBG("Executing %s failed\n",cmdstr);
        str_unref (cmdstr);
    }

    bool_t started = FALSE;

    char * playlist = aud_get_str ("alarm", "playlist");
    if (playlist[0])
    {
        aud_drct_pl_open (playlist);
        started = TRUE;
    }
    str_unref (playlist);

    if(fading)
    {
        fader fade_vols;

        AUDDBG("Fading is true\n");
        aud_drct_set_volume_main(quietvol);

        /* start playing */
        play_start = time(NULL);

        if (! started)
            aud_drct_play ();

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
        char * reminder_msg = aud_get_str ("alarm", "reminder_msg");
        GtkWidget *reminder_dialog;
        AUDDBG("Showing reminder '%s'\n", reminder_msg);

        reminder_dialog = (GtkWidget*) create_reminder_dialog(reminder_msg);
        gtk_widget_show_all(reminder_dialog);
        str_unref (reminder_msg);
    }

    /* bring up the wakeup call dialog if stop_on is set TRUE, this
     * has been moved to after making Audacious play so that it doesnt
     * get in the way for people with manual window placement turned on
     *
     * this means that the dialog doesnt get shown until the volume has
     * finished fading though !, so thats something else to fix
     */
    if(stop_on == TRUE)
    {
        alarm_dialog = create_alarm_dialog();

        AUDDBG("now starting stop thread\n");
        stop = alarm_thread_create(alarm_stop_thread, NULL, 0);
        AUDDBG("Created wakeup dialog and started stop thread\n");
    }

    return TRUE;
}

/*
 * initialization
 * opens the config file and reads the value, creates a new
 * config in memory if the file doesnt exist and sets default vals
 */
static gboolean alarm_init (void)
{
    AUDDBG("alarm_init\n");

    alarm_read_config();

    timeout_source = g_timeout_add_seconds (10, alarm_timeout, NULL);

    aud_plugin_menu_add (AUD_MENU_MAIN, alarm_configure, _("Set Alarm ..."), "appointment-new");

    return TRUE;
}

/*
 * kill the main thread
 */
static void alarm_cleanup(void)
{
    AUDDBG("alarm_cleanup\n");

    aud_plugin_menu_remove (AUD_MENU_MAIN, alarm_configure);

    if (timeout_source)
    {
        g_source_remove (timeout_source);
        timeout_source = 0;
    }

    if (stop.is_valid)
    {
        pthread_cancel(stop.tid);
        stop.is_valid = FALSE;
    }
}

static const char alarm_about[] =
 N_("A plugin that can be used to start playing at a certain time.\n\n"
    "Originally written by Adam Feakin and Daniel Stodden.");

AUD_GENERAL_PLUGIN
(
    .name = N_("Alarm"),
    .domain = PACKAGE,
    .about_text = alarm_about,
    .init = alarm_init,
    .configure = alarm_configure,
    .cleanup = alarm_cleanup,
)
