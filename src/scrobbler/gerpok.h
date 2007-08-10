#ifndef G_NET_H
#define G_NET_H 1

#include <audacious/tuple.h>

int gerpok_sc_idle(GMutex *);
void gerpok_sc_init(char *, char *);
void gerpok_sc_addentry(GMutex *, Tuple *, int);
void gerpok_sc_cleaner(void);
int gerpok_sc_catch_error(void);
char *gerpok_sc_fetch_error(void);
void gerpok_sc_clear_error(void);
#endif
