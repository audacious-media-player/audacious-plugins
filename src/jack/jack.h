#ifndef _JACK_H
#define _JACK_H

#include <libaudcore/core.h>

void jack_configure(void);

typedef struct
{
    bool_t isTraceEnabled; /* if true we will print debug information to the console */
    int volume_left, volume_right; /* for loading the stored volume setting */
} jackconfig;

void jack_set_port_connection_mode(); /* called by jack_init() and the 'ok' handler in configure.c */

#endif /* #ifndef _JACK_H */
