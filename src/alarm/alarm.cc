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
#include <string.h>

#include <gdk/gdk.h>
#include <gtk/gtk.h>
#include <pthread.h>
#include <assert.h>
#include <math.h>

#include <libaudcore/drct.h>
#include <libaudcore/i18n.h>
#include <libaudcore/interface.h>
#include <libaudcore/plugin.h>
#include <libaudcore/plugins.h>
#include <libaudcore/preferences.h>
#include <libaudcore/runtime.h>

#include <libaudgui/libaudgui.h>
#include <libaudgui/libaudgui-gtk.h>

#include "alarm.h"
#include "interface.h"
#include "callbacks.h"

class AlarmPlugin : public GeneralPlugin
{
public:
    static const char about[];
    static const char * const defaults[];
    static const PreferencesWidget widgets[];
    static const PluginPreferences prefs;

    static constexpr PluginInfo info = {
        N_("Alarm"),
        PACKAGE,
        about,
        & prefs,
        PluginGLibOnly
    };

    constexpr AlarmPlugin () : GeneralPlugin (info, false) {}

    bool init ();
    void cleanup ();
};

EXPORT AlarmPlugin aud_plugin_instance;

const char * const AlarmPlugin::defaults[] = {
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
 nullptr};

typedef struct
{
    pthread_t tid;
    volatile gboolean  is_valid;
} alarm_thread_t;

static int timeout_source;
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

    GtkWidget *playlist;

    int default_hour;
    int default_min;

    // array allows looping of days
    alarmday day[7];


    GtkEntry *reminder;
    GtkToggleButton *reminder_cb;
    gboolean reminder_on;
}
alarm_conf;

static int alarm_h, alarm_m;

static gboolean stop_on;
static int stop_h, stop_m;

static int volume, quietvol;

static int fading;

static gboolean cmd_on;

static GtkWidget *config_notebook = nullptr;
static GtkWidget *alarm_dialog = nullptr;

static GtkWidget *lookup_widget(GtkWidget *w, const char *name)
{
    GtkWidget * widget = (GtkWidget *) g_object_get_data ((GObject *) w, name);
    g_return_val_if_fail(widget != nullptr, nullptr);

    return widget;
}

/*
 * the callback function that is called when the save button is
 * pressed saves configuration to ~/.bmp/alarmconfig
 */
static void alarm_save()
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

    String playlist = audgui_file_entry_get_uri (alarm_conf.playlist);
    aud_set_str ("alarm", "playlist", playlist ? playlist : "");

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
static void alarm_read_config()
{
    int daynum = 0;   // used for day number

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
static void *alarm_make_config_widget()
{
    int daynum = 0;  // used to loop days
    GtkWidget *w;

    alarm_read_config();

    /*
     * Create the widgets
     */
    config_notebook = create_config_notebook();

    w = lookup_widget(config_notebook, "alarm_h_spin");
    alarm_conf.alarm_h = GTK_SPIN_BUTTON(w);
    gtk_spin_button_set_value(alarm_conf.alarm_h, alarm_h);

    w = lookup_widget(config_notebook, "alarm_m_spin");
    alarm_conf.alarm_m =  GTK_SPIN_BUTTON(w);
    gtk_spin_button_set_value(alarm_conf.alarm_m, alarm_m);

    w = lookup_widget(config_notebook, "stop_h_spin");
    alarm_conf.stop_h = GTK_SPIN_BUTTON(w);
    gtk_spin_button_set_value(alarm_conf.stop_h, stop_h);

    w = lookup_widget(config_notebook, "stop_m_spin");
    alarm_conf.stop_m = GTK_SPIN_BUTTON(w);
    gtk_spin_button_set_value(alarm_conf.stop_m, stop_m);

    w = lookup_widget(config_notebook, "stop_checkb");
    alarm_conf.stop_on = GTK_TOGGLE_BUTTON(w);
    gtk_toggle_button_set_active(alarm_conf.stop_on, stop_on);

    w = lookup_widget(config_notebook, "vol_scale");
    alarm_conf.volume = GTK_RANGE(w);
    gtk_range_set_adjustment(alarm_conf.volume,
     GTK_ADJUSTMENT(gtk_adjustment_new(volume, 0, 100, 1, 5, 0)));

    w = lookup_widget(config_notebook, "quiet_vol_scale");
    alarm_conf.quietvol = GTK_RANGE(w);
    gtk_range_set_adjustment(alarm_conf.quietvol,
     GTK_ADJUSTMENT(gtk_adjustment_new(quietvol, 0, 100, 1, 5, 0)));

    /* days of week */
    for(; daynum < 7; daynum++)
    {
        w = lookup_widget(config_notebook, day_cb[daynum]);
        alarm_conf.day[daynum].cb = GTK_CHECK_BUTTON(w);
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(alarm_conf.day[daynum].cb),
         !(alarm_conf.day[daynum].flags & ALARM_OFF));

        w = lookup_widget(config_notebook, day_def[daynum]);
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
            w = lookup_widget(config_notebook, day_h[daynum]);
            alarm_conf.day[daynum].spin_hr = GTK_SPIN_BUTTON(w);
            gtk_spin_button_set_value(alarm_conf.day[daynum].spin_hr, alarm_conf.default_hour);

            w = lookup_widget(config_notebook, day_m[daynum]);
            alarm_conf.day[daynum].spin_min = GTK_SPIN_BUTTON(w);
            gtk_spin_button_set_value(alarm_conf.day[daynum].spin_min, alarm_conf.default_min);

            gtk_widget_set_sensitive((GtkWidget *)alarm_conf.day[daynum].spin_hr, false);
            gtk_widget_set_sensitive((GtkWidget *)alarm_conf.day[daynum].spin_min, false);
        }
        else
        {
            w = lookup_widget(config_notebook, day_h[daynum]);
            alarm_conf.day[daynum].spin_hr = GTK_SPIN_BUTTON(w);
            gtk_spin_button_set_value(alarm_conf.day[daynum].spin_hr, alarm_conf.day[daynum].hour);

            w = lookup_widget(config_notebook, day_m[daynum]);
            alarm_conf.day[daynum].spin_min = GTK_SPIN_BUTTON(w);
            gtk_spin_button_set_value(alarm_conf.day[daynum].spin_min, alarm_conf.day[daynum].min);

            gtk_widget_set_sensitive((GtkWidget *)alarm_conf.day[daynum].spin_hr, true);
            gtk_widget_set_sensitive((GtkWidget *)alarm_conf.day[daynum].spin_min, true);
        }
    }

   /* END: days of week */

    w = lookup_widget(config_notebook,"fading_spin");
    alarm_conf.fading = GTK_SPIN_BUTTON(w);
    gtk_spin_button_set_value(alarm_conf.fading, fading);

    String cmdstr = aud_get_str ("alarm", "cmdstr");
    w = lookup_widget(config_notebook, "cmd_entry");
    alarm_conf.cmdstr = GTK_ENTRY(w);
    gtk_entry_set_text(alarm_conf.cmdstr, cmdstr);

    w = lookup_widget(config_notebook, "cmd_checkb");
    alarm_conf.cmd_on = GTK_TOGGLE_BUTTON(w);
    gtk_toggle_button_set_active(alarm_conf.cmd_on, cmd_on);

    String playlist = aud_get_str ("alarm", "playlist");
    w = lookup_widget(config_notebook, "playlist");
    alarm_conf.playlist = w;
    audgui_file_entry_set_uri(alarm_conf.playlist, playlist);

    String reminder_msg = aud_get_str ("alarm", "reminder_msg");
    w = lookup_widget(config_notebook, "reminder_text");
    alarm_conf.reminder = GTK_ENTRY(w);
    gtk_entry_set_text(alarm_conf.reminder, reminder_msg);

    w = lookup_widget(config_notebook, "reminder_cb");
    alarm_conf.reminder_cb = GTK_TOGGLE_BUTTON(w);
    gtk_toggle_button_set_active(alarm_conf.reminder_cb, alarm_conf.reminder_on);

    AUDDBG("END alarm_configure\n");

    return config_notebook;
}

/* functions for greying out the time for days */
static void on_day_def_toggled(GtkToggleButton *togglebutton, void * user_data, int daynum)
{
    GtkWidget *w;

    /* change the time shown too */
    w = lookup_widget(config_notebook, day_h[daynum]);
    if(w == nullptr)
        return;

    if(gtk_toggle_button_get_active(togglebutton) == true)
    {
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(w), alarm_conf.default_hour);
        gtk_widget_set_sensitive(w, false);
    }
    else
    {
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(w), alarm_conf.day[daynum].hour);
        gtk_widget_set_sensitive(w, true);
    }

    w = lookup_widget(config_notebook, day_m[daynum]);
    if(gtk_toggle_button_get_active(togglebutton) == true)
    {
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(w), alarm_conf.default_min);
        gtk_widget_set_sensitive(w, false);
    }
    else
    {
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(w), alarm_conf.day[daynum].min);
        gtk_widget_set_sensitive(w, true);
    }
}

void on_sun_def_toggled(GtkToggleButton *togglebutton, void * user_data)
{
    on_day_def_toggled(togglebutton, user_data, 0);
}

void on_mon_def_toggled(GtkToggleButton *togglebutton, void * user_data)
{
    on_day_def_toggled(togglebutton, user_data, 1);
}

void on_tue_def_toggled(GtkToggleButton *togglebutton, void * user_data)
{
    on_day_def_toggled(togglebutton, user_data, 2);
}

void on_wed_def_toggled(GtkToggleButton *togglebutton, void * user_data)
{
    on_day_def_toggled(togglebutton, user_data, 3);
}

void on_thu_def_toggled(GtkToggleButton *togglebutton, void * user_data)
{
    on_day_def_toggled(togglebutton, user_data, 4);
}

void on_fri_def_toggled(GtkToggleButton *togglebutton, void * user_data)
{
    on_day_def_toggled(togglebutton, user_data, 5);
}

void on_sat_def_toggled(GtkToggleButton *togglebutton, void * user_data)
{
    on_day_def_toggled(togglebutton, user_data, 6);
}

/* END: greying things */

void alarm_current_volume(GtkButton *button, void * data)
{
    GtkAdjustment *adj;

    AUDDBG("on_current_button_clicked\n");

    adj = gtk_range_get_adjustment(alarm_conf.volume);
    gtk_adjustment_set_value(adj, aud_drct_get_volume_main());
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
    int inc, diff, adiff;

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

    aud_drct_set_volume_main((int)vols->start);

    for (int i = 0; i < adiff; i ++)
    {
        threadsleep((float)fading / (float)adiff);
        aud_drct_set_volume_main(aud_drct_get_volume_main() + inc);
    }
    /* Setting the volume to the end volume sort of defeats the point if having
     * the code in there to allow other apps to control volume too :)
     */
    //aud_drct_set_volume_main((int)vols->end);

    /* and */
    pthread_mutex_unlock(&fader_lock);

    AUDDBG("volume = %f%%\n", (double)vols->end);
    return 0;
}

static void *alarm_stop_thread(void *args)
{
    int currvol;
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

    currvol = aud_drct_get_volume_main(),

    /* fade back to zero */
    fade_vols.start = currvol;
    fade_vols.end = 0;

    /* The fader thread locks the fader_mutex now */
    f = alarm_thread_create(alarm_fade, &fade_vols, 0);

    pthread_join(f.tid, nullptr);
    aud_drct_stop();

    /* might as well set the volume to something higher than zero so we
     * dont confuse the poor people who just woke up and cant work out why
     * theres no music playing when they press the little play button :)
     */
    aud_drct_set_volume_main(currvol);

    AUDDBG("alarm_stop done\n");
    return(nullptr);
}

void alarm_stop_cancel(GtkWidget *w, void * data)
{
    AUDDBG("alarm_stop_cancel\n");
    if (pthread_cancel(stop.tid) == 0)
        stop.is_valid = false;
}

/* the main alarm thread */
static gboolean alarm_timeout (void * unused)
{
    struct tm *currtime;
    time_t timenow;
    unsigned today;

    AUDDBG("Getting time\n");
    timenow = time(nullptr);
    currtime = localtime(&timenow);
    today = currtime->tm_wday;
    AUDDBG("Today is %d\n", today);

    /* already went off? */
    if (timenow < play_start + 60)
        return true;

    if(alarm_conf.day[today].flags & ALARM_OFF)
        return true;
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
        return true;

    if(cmd_on == true)
    {
        String cmdstr = aud_get_str ("alarm", "cmdstr");
        AUDDBG("Executing %s, cmd_on is true\n", (const char *) cmdstr);
        if(system(cmdstr) == -1)
            AUDDBG("Executing %s failed\n", (const char *) cmdstr);
    }

    gboolean started = false;

    String playlist = aud_get_str ("alarm", "playlist");
    if (playlist[0])
    {
        aud_drct_pl_open (playlist);
        started = true;
    }

    if(fading)
    {
        fader fade_vols;

        AUDDBG("Fading is true\n");
        aud_drct_set_volume_main(quietvol);

        /* start playing */
        play_start = time(nullptr);

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
        play_start = time(nullptr);
        aud_drct_play();
    }

    if(alarm_conf.reminder_on == true)
    {
        String reminder_msg = aud_get_str ("alarm", "reminder_msg");
        GtkWidget *reminder_dialog;
        AUDDBG("Showing reminder '%s'\n", (const char *) reminder_msg);

        reminder_dialog = (GtkWidget*) create_reminder_dialog(reminder_msg);
        gtk_widget_show_all(reminder_dialog);
    }

    /* bring up the wakeup call dialog if stop_on is set TRUE, this
     * has been moved to after making Audacious play so that it doesnt
     * get in the way for people with manual window placement turned on
     *
     * this means that the dialog doesnt get shown until the volume has
     * finished fading though !, so thats something else to fix
     */
    if(stop_on == true)
    {
        alarm_dialog = create_alarm_dialog();

        AUDDBG("now starting stop thread\n");
        stop = alarm_thread_create(alarm_stop_thread, nullptr, 0);
        AUDDBG("Created wakeup dialog and started stop thread\n");
    }

    return true;
}

static void alarm_configure ()
{
    audgui_show_plugin_prefs (aud_plugin_by_header (& aud_plugin_instance));
}

/*
 * initialization
 * opens the config file and reads the value, creates a new
 * config in memory if the file doesnt exist and sets default vals
 */
bool AlarmPlugin::init ()
{
    AUDDBG("alarm_init\n");

    aud_config_set_defaults ("alarm", defaults);

    alarm_read_config();

    timeout_source = g_timeout_add_seconds (10, alarm_timeout, nullptr);

    aud_plugin_menu_add (AudMenuID::Main, alarm_configure, _("Set Alarm ..."), "appointment-new");

    return true;
}

/*
 * kill the main thread
 */
void AlarmPlugin::cleanup ()
{
    AUDDBG("alarm_cleanup\n");

    aud_plugin_menu_remove (AudMenuID::Main, alarm_configure);

    if (timeout_source)
    {
        g_source_remove (timeout_source);
        timeout_source = 0;
    }

    if (stop.is_valid)
    {
        pthread_cancel(stop.tid);
        stop.is_valid = false;
    }
}

const char AlarmPlugin::about[] =
 N_("A plugin that can be used to start playing at a certain time.\n\n"
    "Originally written by Adam Feakin and Daniel Stodden.");

const PreferencesWidget AlarmPlugin::widgets[] = {
    WidgetCustomGTK (alarm_make_config_widget)
};

const PluginPreferences AlarmPlugin::prefs = {
    {widgets},
    nullptr,  // init
    alarm_save,
    nullptr  // cleanup
};
