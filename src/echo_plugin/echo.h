#ifndef ECHO_H
#define ECHO_H

#include <glib.h>

#define MAX_DELAY 1000

void echo_about(void);
void echo_configure(void);

extern gint echo_delay, echo_feedback, echo_volume;
extern gboolean echo_surround_enable;


#endif
