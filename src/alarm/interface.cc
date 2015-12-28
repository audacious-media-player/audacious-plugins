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
#include <libaudgui/libaudgui-gtk.h>

#include "callbacks.h"

static const char help_text[] =
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
    "    time.\n\n\n"

    "Volume\n"
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
    "    Run this command at the alarm time.\n\n"

    "  Playlist:\n"
    "    Load this playlist. If no playlist\n"
    "    is given, the current one will be used.\n"
    "    The URL of an mp3/ogg stream\n"
    "    can also be entered here.\n\n"

    "  Reminder:\n"
    "    Display a reminder when the alarm goes off.\n"
    "    Type the reminder in the input field and enable\n"
    "    the checkbox if you want it to be shown.");

GtkWidget *create_alarm_dialog ()
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

GtkWidget *create_config_notebook ()
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

    const char *weekdays[] = { N_("Monday"), N_("Tuesday"), N_("Wednesday"),
                               N_("Thursday"), N_("Friday"), N_("Saturday"), N_("Sunday") };

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
    GtkWidget *cmd_entry, *playlist_entry, *reminder_text;
    GtkWidget *cmd_checkb, *reminder_checkb;

    /* Page 5 */
    GtkWidget *view, *scrolled_window;
    GtkTextBuffer *text_buffer;


    /* General */
    notebook = gtk_notebook_new ();


    /* Page 1 */
    frame = gtk_frame_new (_("Time"));
    gtk_container_set_border_width (GTK_CONTAINER (frame), 6);
    grid = gtk_table_new (0, 0, false);
    gtk_table_set_col_spacings (GTK_TABLE (grid), 6);
    gtk_table_set_row_spacings (GTK_TABLE (grid), 6);
    gtk_container_set_border_width (GTK_CONTAINER (grid), 6);

    label = gtk_label_new (_("Alarm at (default):"));
    gtk_table_attach (GTK_TABLE (grid), label, 0, 1, 0, 1, GTK_FILL, GTK_FILL, 0, 0);

    adjustment = (GtkAdjustment *) gtk_adjustment_new (6, 0, 23, 1, 10, 0);
    alarm_h_spin = gtk_spin_button_new (adjustment, 1, 0);
    g_object_set_data (G_OBJECT (notebook), "alarm_h_spin", alarm_h_spin);
    gtk_spin_button_set_update_policy (GTK_SPIN_BUTTON (alarm_h_spin), GTK_UPDATE_IF_VALID);
    gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (alarm_h_spin), true);
    gtk_spin_button_set_wrap (GTK_SPIN_BUTTON (alarm_h_spin), true);
    gtk_table_attach (GTK_TABLE (grid), alarm_h_spin, 1, 2, 0, 1, GTK_FILL, GTK_FILL, 0, 0);

    label = gtk_label_new (":");
    gtk_table_attach (GTK_TABLE (grid), label, 2, 3, 0, 1, GTK_FILL, GTK_FILL, 0, 0);

    adjustment = (GtkAdjustment *) gtk_adjustment_new (30, 0, 59, 1, 10, 0);
    alarm_m_spin = gtk_spin_button_new (adjustment, 1, 0);
    g_object_set_data (G_OBJECT (notebook), "alarm_m_spin", alarm_m_spin);
    gtk_spin_button_set_update_policy (GTK_SPIN_BUTTON (alarm_m_spin), GTK_UPDATE_IF_VALID);
    gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (alarm_m_spin), true);
    gtk_spin_button_set_wrap (GTK_SPIN_BUTTON (alarm_m_spin), true);
    gtk_table_attach (GTK_TABLE (grid), alarm_m_spin, 3, 4, 0, 1, GTK_FILL, GTK_FILL, 0, 0);

    label = gtk_label_new (_("h"));
    gtk_table_attach (GTK_TABLE (grid), label, 4, 5, 0, 1, GTK_FILL, GTK_FILL, 0, 0);

    stop_checkb = gtk_check_button_new_with_label (_("Quiet after:"));
    g_object_set_data (G_OBJECT (notebook), "stop_checkb", stop_checkb);
    gtk_table_attach (GTK_TABLE (grid), stop_checkb, 0, 1, 1, 2, GTK_FILL, GTK_FILL, 0, 0);

    adjustment = (GtkAdjustment *) gtk_adjustment_new (0, 0, 100, 1, 10, 0);
    stop_h_spin = gtk_spin_button_new (adjustment, 1, 0);
    g_object_set_data (G_OBJECT (notebook), "stop_h_spin", stop_h_spin);
    gtk_spin_button_set_update_policy (GTK_SPIN_BUTTON (stop_h_spin), GTK_UPDATE_IF_VALID);
    gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (stop_h_spin), true);
    gtk_spin_button_set_wrap (GTK_SPIN_BUTTON (stop_h_spin), true);
    gtk_table_attach (GTK_TABLE (grid), stop_h_spin, 1, 2, 1, 2, GTK_FILL, GTK_FILL, 0, 0);

    label = gtk_label_new (_("hours"));
    gtk_table_attach (GTK_TABLE (grid), label, 2, 3, 1, 2, GTK_FILL, GTK_FILL, 0, 0);

    adjustment = (GtkAdjustment *) gtk_adjustment_new (0, 0, 59, 1, 10, 0);
    stop_m_spin = gtk_spin_button_new (adjustment, 1, 0);
    g_object_set_data (G_OBJECT (notebook), "stop_m_spin", stop_m_spin);
    gtk_spin_button_set_update_policy (GTK_SPIN_BUTTON (stop_m_spin), GTK_UPDATE_IF_VALID);
    gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (stop_m_spin), true);
    gtk_spin_button_set_wrap (GTK_SPIN_BUTTON (stop_m_spin), true);
    gtk_table_attach (GTK_TABLE (grid), stop_m_spin, 3, 4, 1, 2, GTK_FILL, GTK_FILL, 0, 0);

    label = gtk_label_new (_("minutes"));
    gtk_table_attach (GTK_TABLE (grid), label, 4, 5, 1, 2, GTK_FILL, GTK_FILL, 0, 0);
    gtk_container_add (GTK_CONTAINER (frame), grid);

    label = gtk_label_new (_("Time"));
    gtk_notebook_append_page (GTK_NOTEBOOK (notebook), frame, label);


    /* Page 2 */
    frame = gtk_frame_new (_("Choose the days for the alarm to come on"));
    gtk_container_set_border_width (GTK_CONTAINER (frame), 6);
    grid = gtk_table_new (0, 0, false);
    gtk_table_set_col_spacings (GTK_TABLE (grid), 6);
    gtk_table_set_row_spacings (GTK_TABLE (grid), 6);
    gtk_container_set_border_width (GTK_CONTAINER (grid), 6);

    label = gtk_label_new (_("Day"));
    gtk_table_attach (GTK_TABLE (grid), label, 0, 1, 0, 1, GTK_FILL, GTK_FILL, 0, 0);

    label = gtk_label_new (_("Time"));
    gtk_table_attach (GTK_TABLE (grid), label, 2, 5, 0, 1, GTK_FILL, GTK_FILL, 0, 0);

    for (i = 0; i < 7; i ++)
    {
        widget[i] = gtk_check_button_new_with_label (_(weekdays[i]));
        g_object_set_data (G_OBJECT (notebook), day_cb[i], widget[i]);
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget[i]), true);
        gtk_table_attach (GTK_TABLE (grid), widget[i], 0, 1, i + 1, i + 2, GTK_FILL, GTK_FILL, 0, 0);
    }
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget[6]), false);

    for (i = 0; i < 7; i ++)
    {
        checkbutton = gtk_check_button_new_with_label (_("Default"));
        g_object_set_data (G_OBJECT (notebook), day_def[i], checkbutton);
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (checkbutton), true);
        g_signal_connect (checkbutton, "toggled", G_CALLBACK (cb_def[i]), nullptr);
        gtk_table_attach (GTK_TABLE (grid), checkbutton, 1, 2, i + 1, i + 2, GTK_FILL, GTK_FILL, 0, 0);
    }

    for (i = 7, j = 0; i < 14; i ++, j ++)
    {
        adjustment = (GtkAdjustment *) gtk_adjustment_new (6, 0, 23, 1, 10, 0);
        widget[i] = gtk_spin_button_new (adjustment, 1, 0);
        g_object_set_data (G_OBJECT (notebook), day_h[j], widget[i]);
        gtk_table_attach (GTK_TABLE (grid), widget[i], 2, 3, j + 1, j + 2, GTK_FILL, GTK_FILL, 0, 0);
    }

    for (i = 0; i < 7; i ++)
    {
        label = gtk_label_new (":");
        gtk_table_attach (GTK_TABLE (grid), label, 3, 4, i + 1, i + 2, GTK_FILL, GTK_FILL, 0, 0);
    }

    for (i = 14, j = 0; i < 21; i ++, j ++)
    {
        adjustment = (GtkAdjustment *) gtk_adjustment_new (30, 0, 59, 1, 10, 0);
        widget[i] = gtk_spin_button_new (adjustment, 1, 0);
        g_object_set_data (G_OBJECT (notebook), day_m[j], widget[i]);
        gtk_table_attach (GTK_TABLE (grid), widget[i], 4, 5, j + 1, j + 2, GTK_FILL, GTK_FILL, 0, 0);
    }

    label = gtk_label_new (_("Days"));
    gtk_container_add (GTK_CONTAINER (frame), grid);
    gtk_notebook_append_page (GTK_NOTEBOOK (notebook), frame, label);


    /* Page 3 */
    vbox = gtk_vbox_new (false, 6);
    hbox = gtk_hbox_new (false, 6);

    frame = gtk_frame_new (_("Fading"));
    gtk_container_set_border_width (GTK_CONTAINER (frame), 6);
    gtk_container_set_border_width (GTK_CONTAINER (hbox), 6);
    adjustment = (GtkAdjustment *) gtk_adjustment_new (120, 0, 3600, 1, 10, 0);
    fading_spin = gtk_spin_button_new (adjustment, 1, 0);
    g_object_set_data (G_OBJECT (notebook), "fading_spin", fading_spin);
    gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (fading_spin), true);
    gtk_spin_button_set_update_policy (GTK_SPIN_BUTTON (fading_spin), GTK_UPDATE_IF_VALID);
    label = gtk_label_new (_("seconds"));

    gtk_box_pack_start (GTK_BOX (hbox), fading_spin, true, true, 0);
    gtk_box_pack_start (GTK_BOX (hbox), label, false, false, 0);
    gtk_container_add (GTK_CONTAINER (frame), hbox);
    gtk_box_pack_start (GTK_BOX (vbox), frame, false, false, 0);

    frame = gtk_frame_new (_("Volume"));
    vbox2 = gtk_vbox_new (false, 6);
    gtk_container_set_border_width (GTK_CONTAINER (frame), 6);
    gtk_container_set_border_width (GTK_CONTAINER (vbox2), 6);

    label = gtk_label_new (_("Start at"));
    gtk_box_pack_start (GTK_BOX (vbox2), label, false, false, 0);

    quiet_vol_scale = gtk_hscale_new ((GtkAdjustment *) gtk_adjustment_new (20, 0, 100, 1, 5, 0));
    g_object_set_data (G_OBJECT (notebook), "quiet_vol_scale", quiet_vol_scale);
    gtk_scale_set_value_pos (GTK_SCALE (quiet_vol_scale), GTK_POS_RIGHT);
    gtk_scale_set_digits (GTK_SCALE (quiet_vol_scale), 0);
    label = gtk_label_new ("%");
    hbox2 = gtk_hbox_new (false, 6);
    gtk_box_pack_start (GTK_BOX (hbox2), quiet_vol_scale, true, true, 0);
    gtk_box_pack_start (GTK_BOX (hbox2), label, false, false, 0);
    gtk_box_pack_start (GTK_BOX (vbox2), hbox2, false, false, 0);

    separator = gtk_hseparator_new ();
    gtk_box_pack_start (GTK_BOX (vbox2), separator, false, false, 0);

    label = gtk_label_new (_("Final"));
    gtk_box_pack_start (GTK_BOX (vbox2), label, false, false, 0);

    vol_scale = gtk_hscale_new ((GtkAdjustment *) gtk_adjustment_new (80, 0, 100, 1, 5, 0));
    g_object_set_data (G_OBJECT (notebook), "vol_scale", vol_scale);
    gtk_scale_set_value_pos (GTK_SCALE (vol_scale), GTK_POS_RIGHT);
    gtk_scale_set_digits (GTK_SCALE (vol_scale), 0);
    label = gtk_label_new ("%");
    hbox2 = gtk_hbox_new (false, 6);
    gtk_box_pack_start (GTK_BOX (hbox2), vol_scale, true, true, 0);
    gtk_box_pack_start (GTK_BOX (hbox2), label, false, false, 0);
    gtk_box_pack_start (GTK_BOX (vbox2), hbox2, false, false, 0);

    current_button = gtk_button_new_with_label (_("Current"));
    g_signal_connect (current_button, "clicked", G_CALLBACK (alarm_current_volume), nullptr);
    gtk_box_pack_start (GTK_BOX (vbox2), current_button, false, false, 0);

    gtk_container_add (GTK_CONTAINER (frame), vbox2);
    gtk_box_pack_start (GTK_BOX (vbox), frame, false, false, 0);

    label = gtk_label_new (_("Volume"));
    gtk_notebook_append_page (GTK_NOTEBOOK (notebook), vbox, label);


    /* Page 4 */
    vbox = gtk_vbox_new (false, 6);
    frame = gtk_frame_new (_("Additional Command"));
    hbox = gtk_hbox_new (false, 6);
    gtk_container_set_border_width (GTK_CONTAINER (frame), 6);
    gtk_container_set_border_width (GTK_CONTAINER (hbox), 6);
    cmd_entry = gtk_entry_new ();
    g_object_set_data (G_OBJECT (notebook), "cmd_entry", cmd_entry);
    cmd_checkb = gtk_check_button_new_with_label (_("enable"));
    g_object_set_data (G_OBJECT (notebook), "cmd_checkb", cmd_checkb);
    gtk_box_pack_start (GTK_BOX (hbox), cmd_entry, true, true, 0);
    gtk_box_pack_start (GTK_BOX (hbox), cmd_checkb, false, false, 0);
    gtk_container_add (GTK_CONTAINER (frame), hbox);
    gtk_box_pack_start (GTK_BOX (vbox), frame, false, false, 0);

    frame = gtk_frame_new (_("Playlist (optional)"));
    hbox = gtk_hbox_new (false, 6);
    gtk_container_set_border_width (GTK_CONTAINER (frame), 6);
    gtk_container_set_border_width (GTK_CONTAINER (hbox), 6);
    playlist_entry = audgui_file_entry_new (GTK_FILE_CHOOSER_ACTION_OPEN, _("Select a playlist"));
    g_object_set_data (G_OBJECT (notebook), "playlist", playlist_entry);

    gtk_box_pack_start (GTK_BOX (hbox), playlist_entry, true, true, 0);
    gtk_container_add (GTK_CONTAINER (frame), hbox);
    gtk_box_pack_start (GTK_BOX (vbox), frame, false, false, 0);

    frame = gtk_frame_new (_("Reminder"));
    hbox = gtk_hbox_new (false, 6);
    gtk_container_set_border_width (GTK_CONTAINER (frame), 6);
    gtk_container_set_border_width (GTK_CONTAINER (hbox), 6);
    reminder_text = gtk_entry_new ();
    reminder_checkb = gtk_check_button_new_with_label (_("enable"));
    g_object_set_data (G_OBJECT (notebook), "reminder_text", reminder_text);
    g_object_set_data (G_OBJECT (notebook), "reminder_cb", reminder_checkb);
    gtk_box_pack_start (GTK_BOX (hbox), reminder_text, true, true, 0);
    gtk_box_pack_start (GTK_BOX (hbox), reminder_checkb, false, false, 0);
    gtk_container_add (GTK_CONTAINER (frame), hbox);
    gtk_box_pack_start (GTK_BOX (vbox), frame, false, false, 0);

    label = gtk_label_new (_("Options"));
    gtk_notebook_append_page (GTK_NOTEBOOK (notebook), vbox, label);


    /* Page 5 */
    frame = gtk_frame_new (_("What do these options mean?"));
    gtk_container_set_border_width (GTK_CONTAINER (frame), 6);
    view = gtk_text_view_new ();
    gtk_text_view_set_editable (GTK_TEXT_VIEW (view), false);
    gtk_text_view_set_cursor_visible (GTK_TEXT_VIEW (view), false);
    text_buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));
    gtk_text_buffer_set_text (text_buffer, _(help_text), -1);
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
