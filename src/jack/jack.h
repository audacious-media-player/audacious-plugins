#ifndef _JACK_H
#define _JACK_H

void jack_configure(void);

typedef struct
{
    int volume_left, volume_right; /* for loading the stored volume setting */
} jackconfig;

void jack_set_port_connection_mode(); /* called by jack_init() and the 'ok' handler in configure.c */

#endif /* #ifndef _JACK_H */
