/*
 * interface.c
 * Copyright 2012 Thomas Lange
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions, and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions, and the following disclaimer in the documentation
 *    provided with the distribution.
 *
 * This software is provided "as is" and without any warranty, express or
 * implied. In no event shall the authors be liable for any damages arising from
 * the use of this software.
 */

#include <gtk/gtk.h>
#include <libaudcore/i18n.h>
#include <libaudgui/libaudgui.h>

#include "callbacks.h"

#if GTK_CHECK_VERSION (3, 12, 0)
    #define gtk_widget_set_margin_right(w, m) gtk_widget_set_margin_end(w, m)
#endif

const char *help[] =
{
   N_("Time\n"
    "  Alarm at:\n"
    "    The time for the alarm to come on.\n\n"

    "  Quiet after:\n"
    "    Stop alarm after this amount of time.\n"
    "       (if the wakeup dialog is not closed)\n\n\n"

    "Days\n"
    "  Day:\n"
    "    Select the days for the alarm to activate.\n\n"

    "  Time:\n"
    "    Choose the time for the alarm on each day,\n"
    "    or select the toggle button to use the default\n"
    "    time.\n\n\n"),

   N_("Volume\n"
    "  Fading:\n"
    "    Fade the volume up to the chosen volume\n"
    "    for this amount of time.\n\n"

    "  Start at:\n"
    "    Start fading from this volume.\n\n"

    "  Final:\n"
    "    The volume to stop fading at.  If the fading\n"
    "    time is 0 then set volume to this and start\n"
    "    playing.\n\n\n"

    "Options:\n"
    "  Additional Command:\n"
    "    Run this command at the alarm time.\n\n"),

   N_("  Playlist:\n"
    "    Load this playlist. If no playlist\n"
    "    is given, the current one will be used.\n"
    "    The URL of an mp3/ogg stream\n"
    "    can also be entered here.\n\n"

    "  Reminder:\n"
    "    Display a reminder when the alarm goes off.\n"
    "    Type the reminder in the box and turn on the\n"
    "    toggle button if you want it to be shown."),

    nullptr
};

GtkWidget *create_alarm_dialog (void)
{
    GtkWidget *alarm_dialog;

    alarm_dialog = gtk_message_dialog_new (nullptr, GTK_DIALOG_DESTROY_WITH_PARENT,
     GTK_MESSAGE_INFO, GTK_BUTTONS_CLOSE, _("This is your wakeup call."));
    gtk_window_set_title (GTK_WINDOW (alarm_dialog), _("Alarm"));

    g_signal_connect (alarm_dialog, "response",
     G_CALLBACK (alarm_stop_cancel), nullptr);
    g_signal_connect_swapped (alarm_dialog, "response",
     G_CALLBACK (gtk_widget_destroy), alarm_dialog);

    gtk_widget_show_all (alarm_dialog);

    return alarm_dialog;
}

GtkWidget *create_reminder_dialog (const char *reminder_msg)
{
    GtkWidget *reminder_dialog;

    reminder_dialog = gtk_message_dialog_new (nullptr, GTK_DIALOG_DESTROY_WITH_PARENT,
     GTK_MESSAGE_INFO, GTK_BUTTONS_CLOSE, _("Your reminder for today is..."));
    gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (reminder_dialog), "%s", reminder_msg);
    gtk_window_set_title (GTK_WINDOW (reminder_dialog), _("Reminder"));

    g_signal_connect_swapped (reminder_dialog, "response",
     G_CALLBACK (gtk_widget_destroy), reminder_dialog);

    return reminder_dialog;
}

static void file_set_cb (GtkFileChooserButton *button, void * entry)
{
    char *uri = gtk_file_chooser_get_uri (GTK_FILE_CHOOSER (button));
    gtk_entry_set_text (GTK_ENTRY (entry), uri);
    g_free (uri);
}

GtkWidget *create_config_notebook (void)
{
    /* General */
    GtkWidget *notebook;
    GtkWidget *vbox, *hbox, *label, *frame, *grid;
    GtkAdjustment *adjustment;

    /* Page 1 */
    GtkWidget *alarm_h_spin, *alarm_m_spin;
    GtkWidget *stop_checkb, *stop_h_spin, *stop_m_spin;

    /* Page 2 */
    int i, j;
    GtkWidget *checkbutton;
    GtkWidget *widget[21];

    const char *weekdays[] = { _("Monday"), _("Tuesday"), _("Wednesday"),
                                _("Thursday"), _("Friday"), _("Saturday"), _("Sunday") };

    const char *day_cb[] = { "mon_cb", "tue_cb", "wed_cb", "thu_cb",
                              "fri_cb", "sat_cb", "sun_cb" };

    const char *day_def[] = { "mon_def", "tue_def", "wed_def", "thu_def",
                               "fri_def", "sat_def", "sun_def", };

    const char *day_h[] = { "mon_h", "tue_h", "wed_h", "thu_h",
                             "fri_h", "sat_h", "sun_h" };

    const char *day_m[] = { "mon_m", "tue_m", "wed_m", "thu_m",
                             "fri_m", "sat_m", "sun_m" };

    const GCallback cb_def[] = { G_CALLBACK (on_mon_def_toggled), G_CALLBACK (on_tue_def_toggled),
                                 G_CALLBACK (on_wed_def_toggled), G_CALLBACK (on_thu_def_toggled),
                                 G_CALLBACK (on_fri_def_toggled), G_CALLBACK (on_sat_def_toggled),
                                 G_CALLBACK (on_sun_def_toggled) };

    /* Page 3 */
    GtkWidget *vbox2, *hbox2;
    GtkWidget *fading_spin, *quiet_vol_scale, *vol_scale, *separator, *current_button;

    /* Page 4 */
    GtkWidget  *cmd_entry, *playlist_entry, *reminder_text;
    GtkWidget *cmd_checkb, *reminder_checkb, *file_chooser_button;

    /* Page 5 */
    GtkWidget *view, *scrolled_window;
    GtkTextBuffer *text_buffer;
    char *help_text;


    /* General */
    notebook = gtk_notebook_new ();


    /* Page 1 */
    frame = gtk_frame_new (_("Time"));
    gtk_container_set_border_width (GTK_CONTAINER (frame), 10);
    grid = gtk_grid_new ();
    gtk_grid_set_column_spacing (GTK_GRID (grid), 3);
    gtk_grid_set_row_homogeneous (GTK_GRID (grid), TRUE);
    gtk_container_set_border_width (GTK_CONTAINER (grid), 5);

    label = gtk_label_new (_("Alarm at (default):"));
    gtk_widget_set_margin_right (label, 10);
    gtk_grid_attach (GTK_GRID (grid), label, 0, 0, 1, 1);

    adjustment = gtk_adjustment_new (6, 0, 23, 1, 10, 0);
    alarm_h_spin = gtk_spin_button_new (adjustment, 1, 0);
    g_object_set_data (G_OBJECT (notebook), "alarm_h_spin", alarm_h_spin);
    gtk_spin_button_set_update_policy (GTK_SPIN_BUTTON (alarm_h_spin), GTK_UPDATE_IF_VALID);
    gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (alarm_h_spin), TRUE);
    gtk_spin_button_set_wrap (GTK_SPIN_BUTTON (alarm_h_spin), TRUE);
    gtk_grid_attach (GTK_GRID (grid), alarm_h_spin, 1, 0, 1, 1);

    label = gtk_label_new (":");
    gtk_grid_attach (GTK_GRID (grid), label, 2, 0, 1, 1);

    adjustment = gtk_adjustment_new (30, 0, 59, 1, 10, 0);
    alarm_m_spin = gtk_spin_button_new (adjustment, 1, 0);
    g_object_set_data (G_OBJECT (notebook), "alarm_m_spin", alarm_m_spin);
    gtk_spin_button_set_update_policy (GTK_SPIN_BUTTON (alarm_m_spin), GTK_UPDATE_IF_VALID);
    gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (alarm_m_spin), TRUE);
    gtk_spin_button_set_wrap (GTK_SPIN_BUTTON (alarm_m_spin), TRUE);
    gtk_grid_attach (GTK_GRID (grid), alarm_m_spin, 3, 0, 1, 1);

    label = gtk_label_new (_("h"));
    gtk_widget_set_halign (label, GTK_ALIGN_START);
    gtk_grid_attach (GTK_GRID (grid), label, 4, 0, 1, 1);

    stop_checkb = gtk_check_button_new_with_label (_("Quiet after:"));
    g_object_set_data (G_OBJECT (notebook), "stop_checkb", stop_checkb);
    gtk_widget_set_margin_right (stop_checkb, 10);
    gtk_widget_set_valign (stop_checkb, GTK_ALIGN_CENTER);
    gtk_grid_attach (GTK_GRID (grid), stop_checkb, 0, 1, 1, 1);

    adjustment = gtk_adjustment_new (0, 0, 100, 1, 10, 0);
    stop_h_spin = gtk_spin_button_new (adjustment, 1, 0);
    g_object_set_data (G_OBJECT (notebook), "stop_h_spin", stop_h_spin);
    gtk_spin_button_set_update_policy (GTK_SPIN_BUTTON (stop_h_spin), GTK_UPDATE_IF_VALID);
    gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (stop_h_spin), TRUE);
    gtk_spin_button_set_wrap (GTK_SPIN_BUTTON (stop_h_spin), TRUE);
    gtk_grid_attach (GTK_GRID (grid), stop_h_spin, 1, 1, 1, 1);

    label = gtk_label_new (_("hours"));
    gtk_widget_set_margin_right (label, 10);
    gtk_grid_attach (GTK_GRID (grid), label, 2, 1, 1, 1);

    adjustment = gtk_adjustment_new (0, 0, 59, 1, 10, 0);
    stop_m_spin = gtk_spin_button_new (adjustment, 1, 0);
    g_object_set_data (G_OBJECT (notebook), "stop_m_spin", stop_m_spin);
    gtk_spin_button_set_update_policy (GTK_SPIN_BUTTON (stop_m_spin), GTK_UPDATE_IF_VALID);
    gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (stop_m_spin), TRUE);
    gtk_spin_button_set_wrap (GTK_SPIN_BUTTON (stop_m_spin), TRUE);
    gtk_grid_attach (GTK_GRID (grid), stop_m_spin, 3, 1, 1, 1);

    label = gtk_label_new (_("minutes"));
    gtk_grid_attach (GTK_GRID (grid), label, 4, 1, 1, 1);
    gtk_container_add (GTK_CONTAINER (frame), grid);

    label = gtk_label_new (_("Time"));
    gtk_notebook_append_page (GTK_NOTEBOOK (notebook), frame, label);


    /* Page 2 */
    frame = gtk_frame_new (_("Choose the days for the alarm to come on"));
    gtk_container_set_border_width (GTK_CONTAINER (frame), 10);
    grid = gtk_grid_new ();
    gtk_grid_set_column_spacing (GTK_GRID (grid), 15);
    gtk_grid_set_row_homogeneous (GTK_GRID (grid), TRUE);
    gtk_container_set_border_width (GTK_CONTAINER (grid), 5);

    label = gtk_label_new (_("Day"));
    gtk_grid_attach (GTK_GRID (grid), label, 0, 0, 1, 1);

    label = gtk_label_new (_("Time"));
    gtk_grid_attach (GTK_GRID (grid), label, 2, 0, 3, 1);

    for (i = 0; i < 7; i ++)
    {
        widget[i] = gtk_check_button_new_with_label (weekdays[i]);
        g_object_set_data (G_OBJECT (notebook), day_cb[i], widget[i]);
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget[i]), TRUE);
        gtk_widget_set_valign (widget[i], GTK_ALIGN_CENTER);
        gtk_grid_attach (GTK_GRID (grid), widget[i], 0, i + 1, 1, 1);
    }
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget[6]), false);

    for (i = 0; i < 7; i ++)
    {
        checkbutton = gtk_check_button_new_with_label (_("Default"));
        g_object_set_data (G_OBJECT (notebook), day_def[i], checkbutton);
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (checkbutton), TRUE);
        gtk_widget_set_valign (checkbutton, GTK_ALIGN_CENTER);
        g_signal_connect (checkbutton, "toggled", G_CALLBACK (cb_def[i]), nullptr);
        gtk_grid_attach (GTK_GRID (grid), checkbutton, 1, i + 1, 1, 1);
    }

    for (i = 7, j = 0; i < 14; i ++, j ++)
    {
        adjustment = gtk_adjustment_new (6, 0, 23, 1, 10, 0);
        widget[i] = gtk_spin_button_new (adjustment, 1, 0);
        g_object_set_data (G_OBJECT (notebook), day_h[j], widget[i]);
        gtk_grid_attach (GTK_GRID (grid), widget[i], 2, j + 1, 1, 1);
    }

    for (i = 0; i < 7; i ++)
    {
        label = gtk_label_new (":");
        gtk_grid_attach (GTK_GRID (grid), label, 3, i + 1, 1, 1);
    }

    for (i = 14, j = 0; i < 21; i ++, j ++)
    {
        adjustment = gtk_adjustment_new (30, 0, 59, 1, 10, 0);
        widget[i] = gtk_spin_button_new (adjustment, 1, 0);
        g_object_set_data (G_OBJECT (notebook), day_m[j], widget[i]);
        gtk_grid_attach (GTK_GRID (grid), widget[i], 4, j + 1, 1, 1);
    }

    label = gtk_label_new (_("Days"));
    gtk_container_add (GTK_CONTAINER (frame), grid);
    gtk_notebook_append_page (GTK_NOTEBOOK (notebook), frame, label);


    /* Page 3 */
    vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
    hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);

    frame = gtk_frame_new (_("Fading"));
    gtk_container_set_border_width (GTK_CONTAINER (frame), 10);
    gtk_container_set_border_width (GTK_CONTAINER (hbox), 8);
    adjustment = gtk_adjustment_new (120, 0, 3600, 1, 10, 0);
    fading_spin = gtk_spin_button_new (adjustment, 1, 0);
    g_object_set_data (G_OBJECT (notebook), "fading_spin", fading_spin);
    gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (fading_spin), true);
    gtk_spin_button_set_update_policy (GTK_SPIN_BUTTON (fading_spin), GTK_UPDATE_IF_VALID);
    label = gtk_label_new (_("seconds"));

    gtk_container_add (GTK_CONTAINER (hbox), fading_spin);
    gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 3);
    gtk_container_add (GTK_CONTAINER (frame), hbox);
    gtk_container_add (GTK_CONTAINER (vbox), frame);

    frame = gtk_frame_new (_("Volume"));
    vbox2 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_set_border_width (GTK_CONTAINER (frame), 10);
    gtk_container_set_border_width (GTK_CONTAINER (vbox2), 8);

    label = gtk_label_new (_("Start at"));
    gtk_widget_set_margin_bottom (label, 2);
    gtk_widget_set_halign (label, GTK_ALIGN_START);
    gtk_container_add (GTK_CONTAINER (vbox2), label);

    quiet_vol_scale = gtk_scale_new (GTK_ORIENTATION_HORIZONTAL, gtk_adjustment_new (20, 0, 100, 1, 5, 0));
    g_object_set_data (G_OBJECT (notebook), "quiet_vol_scale", quiet_vol_scale);
    gtk_scale_set_value_pos (GTK_SCALE (quiet_vol_scale), GTK_POS_RIGHT);
    gtk_scale_set_digits (GTK_SCALE (quiet_vol_scale), 0);
    label = gtk_label_new ("%");
    hbox2 = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 2);
    gtk_box_pack_start (GTK_BOX (hbox2), quiet_vol_scale, TRUE, TRUE, 0);
    gtk_container_add (GTK_CONTAINER (hbox2), label);
    gtk_container_add (GTK_CONTAINER (vbox2), hbox2);

    separator = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
    gtk_box_pack_start (GTK_BOX (vbox2), separator, FALSE, FALSE, 10);

    label = gtk_label_new (_("Final"));
    gtk_widget_set_margin_bottom (label, 2);
    gtk_widget_set_halign (label, GTK_ALIGN_START);
    gtk_container_add (GTK_CONTAINER (vbox2), label);

    vol_scale = gtk_scale_new (GTK_ORIENTATION_HORIZONTAL, gtk_adjustment_new (80, 0, 100, 1, 5, 0));
    g_object_set_data (G_OBJECT (notebook), "vol_scale", vol_scale);
    gtk_scale_set_value_pos (GTK_SCALE (vol_scale), GTK_POS_RIGHT);
    gtk_scale_set_digits (GTK_SCALE (vol_scale), 0);
    label = gtk_label_new ("%");
    hbox2 = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 2);
    gtk_box_pack_start (GTK_BOX (hbox2), vol_scale, TRUE, TRUE, 0);
    gtk_container_add (GTK_CONTAINER (hbox2), label);
    gtk_container_add (GTK_CONTAINER (vbox2), hbox2);

    current_button = gtk_button_new_with_label (_("Current"));
    gtk_widget_set_margin_top (current_button, 10);
    gtk_widget_set_halign (current_button, GTK_ALIGN_END);
    g_signal_connect (current_button, "clicked", G_CALLBACK (alarm_current_volume), nullptr);
    gtk_container_add (GTK_CONTAINER (vbox2), current_button);

    gtk_container_add (GTK_CONTAINER (frame), vbox2);
    gtk_container_add (GTK_CONTAINER (vbox), frame);

    label = gtk_label_new (_("Volume"));
    gtk_notebook_append_page (GTK_NOTEBOOK (notebook), vbox, label);


    /* Page 4 */
    vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
    frame = gtk_frame_new (_("Additional Command"));
    hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_container_set_border_width (GTK_CONTAINER (frame), 10);
    gtk_container_set_border_width (GTK_CONTAINER (hbox), 8);
    cmd_entry = gtk_entry_new ();
    g_object_set_data (G_OBJECT (notebook), "cmd_entry", cmd_entry);
    cmd_checkb = gtk_check_button_new_with_label (_("enable"));
    g_object_set_data (G_OBJECT (notebook), "cmd_checkb", cmd_checkb);
    gtk_box_pack_start (GTK_BOX (hbox), cmd_entry, TRUE, TRUE, 0);
    gtk_container_add (GTK_CONTAINER (hbox), cmd_checkb);
    gtk_container_add (GTK_CONTAINER (frame), hbox);
    gtk_container_add (GTK_CONTAINER (vbox), frame);

    frame = gtk_frame_new (_("Playlist (optional)"));
    hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_container_set_border_width (GTK_CONTAINER (frame), 10);
    gtk_container_set_border_width (GTK_CONTAINER (hbox), 8);
    playlist_entry = gtk_entry_new ();
    g_object_set_data (G_OBJECT (notebook), "playlist", playlist_entry);

    file_chooser_button = gtk_file_chooser_button_new (_("Select a playlist"), GTK_FILE_CHOOSER_ACTION_OPEN);
    gtk_widget_set_valign (file_chooser_button, GTK_ALIGN_CENTER);
    g_signal_connect (file_chooser_button, "file-set", G_CALLBACK (file_set_cb), playlist_entry);
    gtk_box_pack_start (GTK_BOX (hbox), playlist_entry, TRUE, TRUE, 0);
    gtk_container_add (GTK_CONTAINER (hbox), file_chooser_button);
    gtk_container_add (GTK_CONTAINER (frame), hbox);
    gtk_container_add (GTK_CONTAINER (vbox), frame);

    frame = gtk_frame_new (_("Reminder"));
    hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_container_set_border_width (GTK_CONTAINER (frame), 10);
    gtk_container_set_border_width (GTK_CONTAINER (hbox), 8);
    reminder_text = gtk_entry_new ();
    reminder_checkb = gtk_check_button_new_with_label (_("enable"));
    g_object_set_data (G_OBJECT (notebook), "reminder_text", reminder_text);
    g_object_set_data (G_OBJECT (notebook), "reminder_cb", reminder_checkb);
    gtk_box_pack_start (GTK_BOX (hbox), reminder_text, TRUE, TRUE, 0);
    gtk_container_add (GTK_CONTAINER (hbox), reminder_checkb);
    gtk_container_add (GTK_CONTAINER (frame), hbox);
    gtk_container_add (GTK_CONTAINER (vbox), frame);

    label = gtk_label_new (_("Options"));
    gtk_notebook_append_page (GTK_NOTEBOOK (notebook), vbox, label);


    /* Page 5 */
    frame = gtk_frame_new (_("What do these options mean?"));
    gtk_container_set_border_width (GTK_CONTAINER (frame), 10);
    view = gtk_text_view_new ();
    gtk_text_view_set_editable (GTK_TEXT_VIEW (view), false);
    gtk_text_view_set_cursor_visible (GTK_TEXT_VIEW (view), false);
    text_buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));
    help_text = g_strconcat (_(help[0]), _(help[1]), _(help[2]), nullptr);
    gtk_text_buffer_set_text (text_buffer, help_text, -1);
    g_free (help_text);
    scrolled_window = gtk_scrolled_window_new (nullptr, nullptr);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
     GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_set_border_width (GTK_CONTAINER (scrolled_window), 6);
    gtk_container_add (GTK_CONTAINER (scrolled_window), view);
    gtk_container_add (GTK_CONTAINER (frame), GTK_WIDGET (scrolled_window));

    label = gtk_label_new (_("Help"));
    gtk_notebook_append_page (GTK_NOTEBOOK (notebook), frame, label);

    return notebook;
}
