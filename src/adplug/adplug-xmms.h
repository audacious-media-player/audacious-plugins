gboolean adplug_init (void);
void adplug_quit(void);
void adplug_about(void);
void adplug_config(void);
void adplug_stop(InputPlayback * data);
gboolean adplug_play(InputPlayback * data, const gchar * filename, VFSFile * file, gint start_time, gint stop_time, gboolean pause);
void adplug_pause(InputPlayback * playback, gboolean paused);
void adplug_mseek (InputPlayback * playback, gint time);
void adplug_info_box(const gchar *filename);
Tuple* adplug_get_tuple(const gchar *filename, VFSFile *file);
int adplug_is_our_fd(const gchar * filename, VFSFile * fd);

