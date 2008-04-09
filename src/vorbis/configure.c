#include "config.h"

#include "vorbis.h"

#include <glib.h>
#include <gtk/gtk.h>
#include <stdlib.h>
#include <string.h>

#include <audacious/plugin.h>
#include <audacious/util.h>
#include <audacious/i18n.h>

extern GMutex *vf_mutex;

static GtkWidget *vorbis_configurewin = NULL;
static GtkWidget *vbox;

static GtkWidget *title_tag_override, *title_tag_box, *title_tag_entry,
    *title_desc;

vorbis_config_t vorbis_cfg;

static void
vorbis_configurewin_ok(GtkWidget * widget, gpointer data)
{
    mcs_handle_t *db;
    GtkToggleButton *tb;

	if (vorbis_cfg.tag_format != NULL)
		g_free(vorbis_cfg.tag_format);
    vorbis_cfg.tag_format =
        g_strdup(gtk_entry_get_text(GTK_ENTRY(title_tag_entry)));

    tb = GTK_TOGGLE_BUTTON(title_tag_override);
    vorbis_cfg.tag_override = gtk_toggle_button_get_active(tb);

    db = aud_cfg_db_open();
    aud_cfg_db_set_bool(db, "vorbis", "tag_override",
                        vorbis_cfg.tag_override);
    aud_cfg_db_set_string(db, "vorbis", "tag_format", vorbis_cfg.tag_format);
    aud_cfg_db_close(db);
    gtk_widget_destroy(vorbis_configurewin);
}

static void
configure_destroy(GtkWidget * w, gpointer data)
{
	// nothing here currently
}

static void
title_tag_override_cb(GtkWidget * w, gpointer data)
{
    gboolean override;
    override =
        gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(title_tag_override));
    gtk_widget_set_sensitive(title_tag_box, override);
    gtk_widget_set_sensitive(title_desc, override);
}

void
vorbis_configure(void)
{
    GtkWidget *title_frame, *title_tag_vbox, *title_tag_label;
    GtkWidget *bbox, *ok, *cancel;

    if (vorbis_configurewin != NULL) {
        gtk_window_present(GTK_WINDOW(vorbis_configurewin));
        return;
    }

    vorbis_configurewin = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_type_hint(GTK_WINDOW(vorbis_configurewin),
                             GDK_WINDOW_TYPE_HINT_DIALOG);
    gtk_window_set_position(GTK_WINDOW(vorbis_configurewin),
                            GTK_WIN_POS_CENTER);
    g_signal_connect(G_OBJECT(vorbis_configurewin), "destroy",
                     G_CALLBACK(gtk_widget_destroyed), &vorbis_configurewin);
    g_signal_connect(G_OBJECT(vorbis_configurewin), "destroy",
                     G_CALLBACK(configure_destroy), &vorbis_configurewin);
    gtk_window_set_title(GTK_WINDOW(vorbis_configurewin),
                         _("Ogg Vorbis Audio Plugin Configuration"));
    gtk_window_set_resizable(GTK_WINDOW(vorbis_configurewin), FALSE);
    gtk_container_border_width(GTK_CONTAINER(vorbis_configurewin), 10);

    vbox = gtk_vbox_new(FALSE, 10);
    gtk_container_add(GTK_CONTAINER(vorbis_configurewin), vbox);


    /* Title config.. */

    title_frame = gtk_frame_new(_("Ogg Vorbis Tags:"));
    gtk_container_border_width(GTK_CONTAINER(title_frame), 5);

    title_tag_vbox = gtk_vbox_new(FALSE, 10);
    gtk_container_border_width(GTK_CONTAINER(title_tag_vbox), 5);
    gtk_container_add(GTK_CONTAINER(title_frame), title_tag_vbox);

    title_tag_override =
        gtk_check_button_new_with_label(_("Override generic titles"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(title_tag_override),
                                 vorbis_cfg.tag_override);
    g_signal_connect(G_OBJECT(title_tag_override), "clicked",
                     G_CALLBACK(title_tag_override_cb), NULL);
    gtk_box_pack_start(GTK_BOX(title_tag_vbox), title_tag_override, FALSE,
                       FALSE, 0);

    title_tag_box = gtk_hbox_new(FALSE, 5);
    gtk_widget_set_sensitive(title_tag_box, vorbis_cfg.tag_override);
    gtk_box_pack_start(GTK_BOX(title_tag_vbox), title_tag_box, FALSE,
                       FALSE, 0);

    title_tag_label = gtk_label_new(_("Title format:"));
    gtk_box_pack_start(GTK_BOX(title_tag_box), title_tag_label, FALSE,
                       FALSE, 0);

    title_tag_entry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(title_tag_entry), vorbis_cfg.tag_format);
    gtk_box_pack_start(GTK_BOX(title_tag_box), title_tag_entry, TRUE, TRUE, 0);

    gtk_box_pack_start(GTK_BOX(vbox), title_frame, TRUE, TRUE, 0);

    /* Buttons */

    bbox = gtk_hbutton_box_new();
    gtk_button_box_set_layout(GTK_BUTTON_BOX(bbox), GTK_BUTTONBOX_END);
    gtk_button_box_set_spacing(GTK_BUTTON_BOX(bbox), 5);
    gtk_box_pack_start(GTK_BOX(vbox), bbox, FALSE, FALSE, 0);

    cancel = gtk_button_new_from_stock(GTK_STOCK_CANCEL);
    gtk_button_set_use_stock(GTK_BUTTON(cancel), TRUE);
    g_signal_connect_swapped(G_OBJECT(cancel), "clicked",
                             G_CALLBACK(gtk_widget_destroy),
                             G_OBJECT(vorbis_configurewin));
    GTK_WIDGET_SET_FLAGS(cancel, GTK_CAN_DEFAULT);
    gtk_box_pack_start(GTK_BOX(bbox), cancel, TRUE, TRUE, 0);

    ok = gtk_button_new_from_stock(GTK_STOCK_OK);
    gtk_button_set_use_stock(GTK_BUTTON(ok), TRUE);
    g_signal_connect(G_OBJECT(ok), "clicked",
                     G_CALLBACK(vorbis_configurewin_ok), NULL);
    GTK_WIDGET_SET_FLAGS(ok, GTK_CAN_DEFAULT);
    gtk_box_pack_start(GTK_BOX(bbox), ok, TRUE, TRUE, 0);
    gtk_widget_grab_default(ok);

    gtk_widget_show_all(vorbis_configurewin);
}
