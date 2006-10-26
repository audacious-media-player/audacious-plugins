#include <gtk/gtk.h>


void
alarm_current_volume                   (GtkButton       *button,
                                        gpointer         user_data);

void
alarm_save                             (GtkButton       *button,
                                        gpointer         user_data);

void
alarm_stop_cancel                      (GtkButton       *button,
                                        gpointer         user_data);

void
alarm_store_playlistname               (GtkButton       *button,
                                        gpointer         user_data);

void
on_mon_def_toggled                     (GtkToggleButton *togglebutton,
                                        gpointer         user_data);

void
on_tue_def_toggled                     (GtkToggleButton *togglebutton,
                                        gpointer         user_data);

void
on_wed_def_toggled                     (GtkToggleButton *togglebutton,
                                        gpointer         user_data);

void
on_thu_def_toggled                     (GtkToggleButton *togglebutton,
                                        gpointer         user_data);

void
on_fri_def_toggled                     (GtkToggleButton *togglebutton,
                                        gpointer         user_data);

void
on_sat_def_toggled                     (GtkToggleButton *togglebutton,
                                        gpointer         user_data);

void
on_sun_def_toggled                     (GtkToggleButton *togglebutton,
                                        gpointer         user_data);
/*
 * vi:ai:expandtab:ts=2 sts=2 shiftwidth=2:nowrap:
 */
