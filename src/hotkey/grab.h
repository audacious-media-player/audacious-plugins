#ifndef _GRAB_H_INCLUDED_
#define _GRAB_H_INCLUDED_

#include <glib.h>

void grab_keys ();
void ungrab_keys ();
gboolean setup_filter();
void release_filter();

#endif
