#include "cuesheet.h"

gboolean
do_stop(gpointer data)
{
    AUDDBG("f: do_stop\n");
    audacious_drct_stop();
    AUDDBG("e: do_stop\n");

    return FALSE; /* only once */
}

gboolean
do_setpos(gpointer data)
{
    Playlist *playlist = aud_playlist_get_active();
    gint pos = aud_playlist_get_position_nolock(playlist);
    gint incr = *(gint *)data;

    /* mpris needs state change */
    audacious_drct_stop();

    pos = pos + incr;
    if(pos < 0)
        pos = 0;

    AUDDBG("do_setpos: pos = %d\n\n", pos);

    if (!playlist)
        return FALSE;

    /* being done from the main loop thread, does not require locks */
    aud_playlist_set_position(playlist, (guint)pos);

    /* mpris needs state change */
    audacious_drct_play();

    return FALSE; /* only once */
}

gpointer
watchdog_func(gpointer data)
{
    gint time = 0;
    Playlist *playlist = NULL;
    GTimeVal sleep_time;

    AUDDBG("enter\n");

    while(1) {
#if 0
        AUDDBG("time = %d cur = %d cidx = %d nidx = %d last = %d\n",
               time,
               cur_cue_track,
               cue_tracks[cur_cue_track].index,
               cue_tracks[cur_cue_track+1].index,
               last_cue_track);
#endif
        g_get_current_time(&sleep_time);
        g_time_val_add(&sleep_time, 500000); /* interval is 0.5sec. */

        g_mutex_lock(cue_mutex);
        switch(watchdog_state) {
        case EXIT:
            AUDDBG("e: watchdog exit\n");
            g_mutex_unlock(cue_mutex); /* stop() will lock cue_mutex */
            stop(real_ip); /* no need to care real_ip != NULL here. */
            g_thread_exit(NULL);
            break;
        case RUN:
            if(!playlist)
                playlist = aud_playlist_get_active();
            g_cond_timed_wait(cue_cond, cue_mutex, &sleep_time);
            break;
        case STOP:
            AUDDBG("watchdog deactivated\n");
            g_cond_wait(cue_cond, cue_mutex);
            playlist = aud_playlist_get_active();
            break;
        }
        g_mutex_unlock(cue_mutex);

        if(watchdog_state != RUN)
            continue;

        /* get raw time */
        time = real_ip->output->output_time();

#if 0
        AUDDBG("time = %d target_time = %lu durattion = %d\n",
               time,
               target_time,
               cue_tracks[cur_cue_track].duration);
#endif

        if(time == 0 || time <= target_time)
            continue;

        /* next track */
        if(time >= cue_tracks[cur_cue_track].index +
           cue_tracks[cur_cue_track].duration)
        {
            static gint incr = 0;
            AUDDBG("i: watchdog next\n");
            AUDDBG("time = %d cur = %d cidx = %d nidx = %d last = %d lidx = %d\n",
                   time,
                   cur_cue_track,
                   cue_tracks[cur_cue_track].index,
                   cue_tracks[cur_cue_track+1].index,
                   last_cue_track,
                   cue_tracks[last_cue_track].index);

            incr = 1; /* is this ok? */

            if(aud_cfg->stopaftersong) {
                g_idle_add_full(G_PRIORITY_HIGH, do_stop, (void *)real_ip, NULL);
                continue;
            }
            else {
                g_idle_add_full(G_PRIORITY_HIGH , do_setpos, &incr, NULL);
                continue;
            }
        }

        /* last track */
        if (cur_cue_track + 1 == last_cue_track &&
            (cue_tracks[last_cue_track].index - time < 500 ||
             time > cue_tracks[last_cue_track].index) ){
            AUDDBG("last track\n");
            gint pos = aud_playlist_get_position(playlist);
            if (pos + 1 == aud_playlist_get_length(playlist)) {

                AUDDBG("i: watchdog eof reached\n\n");

                if(aud_cfg->repeat) {
                    static gint incr = 0;
                    incr = -pos;
                    g_idle_add_full(G_PRIORITY_HIGH , do_setpos, &incr, NULL);
                    continue;
                }
                else {
                    g_idle_add_full(G_PRIORITY_HIGH, do_stop,
                                    (void *)real_ip, NULL);
                    continue;
                }
            }
            else {
                if(aud_cfg->stopaftersong) {
                    g_idle_add_full(G_PRIORITY_HIGH, do_stop,
                                    (void *)real_ip, NULL);
                    continue;
                }

                AUDDBG("i: watchdog end of cue, advance in playlist\n\n");

                static gint incr = 1;
                g_idle_add_full(G_PRIORITY_HIGH , do_setpos, &incr, NULL);
                continue;
            }
        }
    }

    AUDDBG("e: watchdog\n");

    return NULL; /* dummy */
}
