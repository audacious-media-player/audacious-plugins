#ifndef __CALLBACKS_H
#define __CALLBACKS_H

#include <gtk/gtk.h>

void alarm_current_volume (GtkButton *button, void * data);

void alarm_stop_cancel (GtkWidget *widget, void * data);

void on_mon_def_toggled (GtkToggleButton *togglebutton, void * data);

void on_tue_def_toggled (GtkToggleButton *togglebutton, void * data);

void on_wed_def_toggled (GtkToggleButton *togglebutton, void * data);

void on_thu_def_toggled (GtkToggleButton *togglebutton, void * data);

void on_fri_def_toggled (GtkToggleButton *togglebutton, void * data);

void on_sat_def_toggled (GtkToggleButton *togglebutton, void * data);

void on_sun_def_toggled (GtkToggleButton *togglebutton, void * data);

#endif
