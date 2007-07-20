
#ifndef CONFIGURE_H
#define CONFIGURE_H

void				configure_set_variables(gboolean *usedae, int *limitspeed, gboolean *usecdtext, gboolean *usecddb, char *device, gboolean *debug, char *cddbserver, int *cddbport);
void				configure_create_gui();
void				configure_show_gui();


#endif	// CONFIGURE_H

