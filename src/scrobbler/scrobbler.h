#ifndef NET_H
#define NET_H 1

#include <glib.h>

#include <libaudcore/tuple.h>
#include <audacious/preferences.h>

#define SC_CURL_TIMEOUT 60

extern const PluginPreferences preferences;

gboolean sc_timeout(gpointer data);

int sc_idle(pthread_mutex_t *mutex);
void sc_init(char *, char *, char *);
void sc_addentry(pthread_mutex_t *mutex, Tuple *tuple, int len, bool_t is_http_source);
void sc_cleaner(void);
int sc_catch_error(void);
char *sc_fetch_error(void);
void sc_clear_error(void);
void sc_playback_end(void);

#endif
