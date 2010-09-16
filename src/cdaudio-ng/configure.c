#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>
#include <audacious/i18n.h>
#include "config.h"

#include "cdaudio-ng.h"
#include "configure.h"

extern cdng_cfg_t cdng_cfg;

static GtkWidget *configwindow = NULL,
    *okbutton = NULL,
    *cancelbutton = NULL,
    *maintable = NULL,
    *daeframe = NULL,
    *titleinfoframe = NULL,
    *miscframe = NULL,
    *daetable = NULL,
    *titleinfotable = NULL,
    *misctable = NULL,
    *usecdtextcheckbutton = NULL,
    *usecddbcheckbutton = NULL,
    *cddbserverlabel = NULL,
    *cddbpathlabel = NULL,
    *cddbportlabel = NULL,
    *cddbserverentry = NULL,
    *cddbpathentry = NULL,
    *cddbportentry = NULL,
    *cddbhttpcheckbutton = NULL,
    *usedevicecheckbutton = NULL,
    *buttonbox = NULL, * disc_speed_button = NULL, *deviceentry = NULL;


static void configure_values_to_gui (void)
{
    gchar portstr[16];

    gtk_spin_button_set_value ((GtkSpinButton *) disc_speed_button,
     cdng_cfg.disc_speed);

    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (usecdtextcheckbutton),
                                  cdng_cfg.use_cdtext);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (usecddbcheckbutton),
                                  cdng_cfg.use_cddb);

    gtk_entry_set_text (GTK_ENTRY (cddbserverentry), cdng_cfg.cddb_server);
    gtk_entry_set_text (GTK_ENTRY (cddbpathentry), cdng_cfg.cddb_path);
    g_snprintf (portstr, sizeof (portstr), "%d", cdng_cfg.cddb_port);
    gtk_entry_set_text (GTK_ENTRY (cddbportentry), portstr);
    gtk_widget_set_sensitive (cddbserverentry, cdng_cfg.use_cddb);
    gtk_widget_set_sensitive (cddbpathentry, cdng_cfg.use_cddb);
    gtk_widget_set_sensitive (cddbportentry, cdng_cfg.use_cddb);

    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (cddbhttpcheckbutton),
                                  cdng_cfg.cddb_http);
    gtk_widget_set_sensitive (cddbhttpcheckbutton, cdng_cfg.use_cddb);

    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (usedevicecheckbutton),
                                  strlen (cdng_cfg.device) > 0);

    gtk_widget_set_sensitive (deviceentry, strlen (cdng_cfg.device) > 0);
    gtk_entry_set_text (GTK_ENTRY (deviceentry), cdng_cfg.device);
}


static void configure_gui_to_values (void)
{
    cdng_cfg.disc_speed = gtk_spin_button_get_value ((GtkSpinButton *)
     disc_speed_button);

    cdng_cfg.use_cdtext =
        gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (usecdtextcheckbutton));
    cdng_cfg.use_cddb =
        gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (usecddbcheckbutton));
    pstrcpy (&cdng_cfg.cddb_server,
             gtk_entry_get_text (GTK_ENTRY (cddbserverentry)));
    pstrcpy (&cdng_cfg.cddb_path,
             gtk_entry_get_text (GTK_ENTRY (cddbpathentry)));
    cdng_cfg.cddb_http =
        gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (cddbhttpcheckbutton));
    cdng_cfg.cddb_port =
        strtol (gtk_entry_get_text (GTK_ENTRY (cddbportentry)), NULL, 10);

    if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (usedevicecheckbutton)))
        pstrcpy (&cdng_cfg.device,
                 gtk_entry_get_text (GTK_ENTRY (deviceentry)));
    else
        pstrcpy (&cdng_cfg.device, "");
}


static gboolean delete_window (GtkWidget * widget, GdkEvent * event,
                               gpointer data)
{
    (void) widget;
    (void) event;
    (void) data;
    gtk_widget_hide (configwindow);
    return TRUE;
}


static void button_clicked (GtkWidget * widget, gpointer data)
{
    (void) data;
    gtk_widget_hide (configwindow);
    if (widget == okbutton)
        configure_gui_to_values ();
}


static void checkbutton_toggled (GtkWidget * widget, gpointer data)
{
    (void) widget;
    (void) data;

    gtk_widget_set_sensitive (cddbserverentry,
                              gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON
                                                            (usecddbcheckbutton)));

    gtk_widget_set_sensitive (cddbpathentry,
                              gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON
                                                            (usecddbcheckbutton)));

    gtk_widget_set_sensitive (cddbhttpcheckbutton,
                              gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON
                                                            (usecddbcheckbutton)));

    gtk_widget_set_sensitive (cddbportentry,
                              gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON
                                                            (usecddbcheckbutton)));

    gtk_widget_set_sensitive (deviceentry,
                              gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON
                                                            (usedevicecheckbutton)));
}


void configure_create_gui ()
{
    configwindow = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    gtk_window_set_type_hint (GTK_WINDOW (configwindow),
                              GDK_WINDOW_TYPE_HINT_DIALOG);
    gtk_window_set_title (GTK_WINDOW (configwindow),
                          _("CD Audio Plugin Configuration"));
    gtk_window_set_resizable (GTK_WINDOW (configwindow), FALSE);
    gtk_window_set_position (GTK_WINDOW (configwindow),
                             GTK_WIN_POS_CENTER_ALWAYS);
    gtk_container_set_border_width (GTK_CONTAINER (configwindow), 10);
    g_signal_connect (G_OBJECT (configwindow), "delete_event",
                      G_CALLBACK (delete_window), NULL);

    maintable = gtk_table_new (4, 2, TRUE);
    gtk_table_set_homogeneous (GTK_TABLE (maintable), FALSE);
    gtk_container_add (GTK_CONTAINER (configwindow), maintable);

    daeframe = gtk_frame_new (_("Digital audio extraction"));
    gtk_table_attach_defaults (GTK_TABLE (maintable), daeframe, 0, 2, 0, 1);
    daetable = gtk_table_new (1, 2, TRUE);
    gtk_container_add (GTK_CONTAINER (daeframe), daetable);

    titleinfoframe = gtk_frame_new (_("Title information"));
    gtk_table_attach_defaults (GTK_TABLE (maintable), titleinfoframe, 0, 2, 1,
                               2);
    titleinfotable = gtk_table_new (2, 2, TRUE);
    gtk_container_add (GTK_CONTAINER (titleinfoframe), titleinfotable);

    miscframe = gtk_frame_new (_("Misc"));
    gtk_table_attach_defaults (GTK_TABLE (maintable), miscframe, 0, 2, 2, 3);
    misctable = gtk_table_new (2, 2, TRUE);
    gtk_container_add (GTK_CONTAINER (miscframe), misctable);

    gtk_table_attach_defaults ((GtkTable *) daetable, gtk_label_new (_("Disc "
     "speed:")), 0, 1, 0, 1);
    disc_speed_button = gtk_spin_button_new_with_range (MIN_DISC_SPEED,
     MAX_DISC_SPEED, 1);
    gtk_table_attach_defaults ((GtkTable *) daetable, disc_speed_button, 1, 2,
     0, 1);

    usecdtextcheckbutton =
        gtk_check_button_new_with_label (_("Use cd-text if available"));
    g_signal_connect (G_OBJECT (usecdtextcheckbutton), "toggled",
                      G_CALLBACK (checkbutton_toggled), NULL);
    gtk_table_attach_defaults (GTK_TABLE (titleinfotable), usecdtextcheckbutton,
                               0, 1, 0, 1);

    usecddbcheckbutton =
        gtk_check_button_new_with_label (_("Use CDDB if available"));
    g_signal_connect (G_OBJECT (usecddbcheckbutton), "toggled",
                      G_CALLBACK (checkbutton_toggled), NULL);
    gtk_table_attach_defaults (GTK_TABLE (titleinfotable), usecddbcheckbutton,
                               0, 1, 1, 2);

    cddbserverlabel = gtk_label_new (_("Server: "));
    gtk_table_attach_defaults (GTK_TABLE (titleinfotable), cddbserverlabel, 0,
                               1, 2, 3);

    cddbpathlabel = gtk_label_new (_("Path: "));
    gtk_table_attach_defaults (GTK_TABLE (titleinfotable), cddbpathlabel, 0, 1,
                               3, 4);

    cddbportlabel = gtk_label_new (_("Port: "));
    gtk_table_attach_defaults (GTK_TABLE (titleinfotable), cddbportlabel, 0, 1,
                               4, 5);

    cddbserverentry = gtk_entry_new ();
    gtk_table_attach_defaults (GTK_TABLE (titleinfotable), cddbserverentry, 1,
                               2, 2, 3);

    cddbpathentry = gtk_entry_new ();
    gtk_table_attach_defaults (GTK_TABLE (titleinfotable), cddbpathentry, 1, 2,
                               3, 4);

    cddbhttpcheckbutton =
        gtk_check_button_new_with_label (_("Use HTTP instead of CDDBP"));
    g_signal_connect (G_OBJECT (cddbhttpcheckbutton), "toggled",
                      G_CALLBACK (checkbutton_toggled), NULL);
    gtk_table_attach_defaults (GTK_TABLE (titleinfotable), cddbhttpcheckbutton,
                               1, 2, 1, 2);

    cddbportentry = gtk_entry_new ();
    gtk_table_attach_defaults (GTK_TABLE (titleinfotable), cddbportentry, 1, 2,
                               4, 5);


    usedevicecheckbutton =
        gtk_check_button_new_with_label (_("Override default device: "));
    g_signal_connect (G_OBJECT (usedevicecheckbutton), "toggled",
                      G_CALLBACK (checkbutton_toggled), NULL);
    gtk_table_attach_defaults (GTK_TABLE (misctable), usedevicecheckbutton, 0,
                               1, 0, 1);

    deviceentry = gtk_entry_new ();
    gtk_table_attach_defaults (GTK_TABLE (misctable), deviceentry, 1, 2, 0, 1);

    buttonbox = gtk_hbutton_box_new ();
    gtk_button_box_set_layout (GTK_BUTTON_BOX (buttonbox), GTK_BUTTONBOX_END);
    gtk_box_set_spacing (GTK_BOX (buttonbox), 10);
    gtk_table_attach_defaults (GTK_TABLE (maintable), buttonbox, 0, 2, 3, 4);

    okbutton = gtk_button_new_with_label (_("Ok"));
    g_signal_connect (G_OBJECT (okbutton), "clicked",
                      G_CALLBACK (button_clicked), NULL);
    gtk_container_add (GTK_CONTAINER (buttonbox), okbutton);

    cancelbutton = gtk_button_new_with_label (_("Cancel"));
    g_signal_connect (G_OBJECT (cancelbutton), "clicked",
                      G_CALLBACK (button_clicked), NULL);
    gtk_container_add (GTK_CONTAINER (buttonbox), cancelbutton);

    gtk_widget_show_all (maintable);
}


void configure_show_gui (void)
{
    if (configwindow == NULL)
        configure_create_gui ();

    configure_values_to_gui ();
    gtk_widget_show (configwindow);
    gtk_window_present (GTK_WINDOW (configwindow));
}


gint pstrcpy (gchar ** res, const gchar * str)
{
    if (!res || !str)
        return -1;

    g_free (*res);
    if ((*res = (gchar *) g_malloc (strlen (str) + 1)) == NULL)
        return -2;

    strcpy (*res, str);
    return 0;
}
