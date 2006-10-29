#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>

//#include <iconv.h>
//#include <libintl.h>

#include <glib.h>
#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <audacious/plugin.h>
#include <audacious/beepctrl.h>
#include <audacious/rcfile.h>
#include <audacious/util.h>
#include <time.h>

#define MPLAYER_INFO_SIZE 128
#define MPLAYER_TITLE_SIZE 260
#define MPLAYER_AUDIO_SIZE 4096
#define MPLAYER_MAX_VECTOR 64

/* For debugging output change the define below  */
#define mplayer_debugf
/*  #define mplayer_debugf printf */   


enum mplayer_vo{
  MPLAYER_VO_NONE = 0,
  MPLAYER_VO_XV,
  MPLAYER_VO_X11,
  MPLAYER_VO_GL,
  MPLAYER_VO_SDL
};

enum mplayer_ao{
  MPLAYER_AO_NONE = 0,
  MPLAYER_AO_OSS,
  MPLAYER_AO_ARTS,
  MPLAYER_AO_ESD,
  MPLAYER_AO_ALSA,
  MPLAYER_AO_SDL,
  MPLAYER_AO_XMMS
};
struct mplayer_info {          /* Stores the info about a file */
  gchar *filename;
  gint vbr,abr,br;              /*Video, audio and total bitrates */ 
  gchar artist[MPLAYER_INFO_SIZE];
  gchar title[MPLAYER_INFO_SIZE];
  glong filesize;               /* bytes */
  gint length;                  /* in seconds */
  gchar caption[MPLAYER_TITLE_SIZE];
  gint rate;                    /*sampling rate*/
  gint nch;                     /*no of channels */
  gint x,y;                     /*x,y resolution */
} 
;

struct mplayer_cfg{
  gint vo,ao;
  gboolean zoom,framedrop,idx,onewin,xmmsaudio; 
  gchar *extra;
};

void mplayer_read_to_eol(char *str1,char *str2);
char **mplayer_make_vector();
void *mplayer_play_loop(void *arg);
void mplayer_vector_append(char**vector,char*param);

struct mplayer_info  *mplayer_read_file_info(char *filename);
gint mplayer_is_our_file(char *filename);
void mplayer_quitting_video(GtkWidget *widget, gpointer data);
void mplayer_play_file(char *filename);
void mplayer_stop();
void mplayer_cleanup();
void mplayer_pause(short p);
void mplayer_seek(int t);
gint mplayer_get_time();
void mplayer_get_song_info(char * filename, char ** title, int * length);
void mplayer_init();
InputPlugin *get_iplugin_info(void);

void on_button_ok_clicked (GtkButton *button, gpointer user_data);
GtkWidget* mplayer_create_configure_win (void);
void mplayer_configure();
static void mplayer_about(void);
struct mplayer_cfg *mplayer_read_cfg();
void mplayer_quitting_video(GtkWidget *widget, gpointer data);
