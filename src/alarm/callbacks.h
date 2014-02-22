#ifndef __CALLBACKS_H
#define __CALLBACKS_H

#include <gtk/gtk.h>

void alarm_save (void);

void alarm_current_volume (GtkButton *button, gpointer data);

void alarm_stop_cancel (GtkWidget *widget, gpointer data);

void on_mon_def_toggled (GtkToggleButton *togglebutton, gpointer data);

void on_tue_def_toggled (GtkToggleButton *togglebutton, gpointer data);

void on_wed_def_toggled (GtkToggleButton *togglebutton, gpointer data);

void on_thu_def_toggled (GtkToggleButton *togglebutton, gpointer data);

void on_fri_def_toggled (GtkToggleButton *togglebutton, gpointer data);

void on_sat_def_toggled (GtkToggleButton *togglebutton, gpointer data);

void on_sun_def_toggled (GtkToggleButton *togglebutton, gpointer data);

#endif
