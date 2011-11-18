/*
 * OSS4 Output Plugin for Audacious
 * Copyright 2010-2011 Michał Lipski <tallica@o2.pl>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "oss.h"
#include <gtk/gtk.h>

static GtkWidget *window;

static void dev_list_changed_cb(GtkComboBox *combo, gpointer data)
{
    GtkTreeIter iter;
    gchar *string = DEFAULT_DSP;
    GtkTreeModel *model;

    if (gtk_combo_box_get_active_iter(combo, &iter))
    {
        model = gtk_combo_box_get_model(combo);
        gtk_tree_model_get(model, &iter, 1, &string, -1);
    }

    aud_set_string("oss4", "device", string);
}

static void alt_dev_text_changed_cb(GtkEditable *widget, gpointer data)
{
    aud_set_string("oss4", "alt_device", gtk_editable_get_chars(widget, 0, -1));
}

static void alt_dev_toggled_cb(GtkToggleButton *widget, gpointer data)
{
    gboolean active = gtk_toggle_button_get_active(widget);
    aud_set_bool("oss4", "use_alt_device", active);
    gtk_widget_set_sensitive(GTK_WIDGET(data), active);
}

static void cookedmode_toggled_cb(GtkToggleButton *widget, gpointer data)
{
    aud_set_bool("oss4", "cookedmode", gtk_toggle_button_get_active(widget));
}

static void exclusive_toggled_cb(GtkToggleButton *widget, gpointer data)
{
    aud_set_bool("oss4", "exclusive", gtk_toggle_button_get_active(widget));
}

static void vol_toggled_cb(GtkToggleButton *widget, gpointer data)
{
    aud_set_bool("oss4", "save_volume", gtk_toggle_button_get_active(widget));
}

static GtkTreeModel *get_device_list(void)
{
    GtkListStore *list;
    GtkTreeIter iter;
    oss_sysinfo sysinfo;
    gint mixerfd, i, a = 1;

    list = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_STRING);

    CHECK_NOISY(mixerfd = open, DEFAULT_MIXER, O_RDWR);
    CHECK(ioctl, mixerfd, SNDCTL_SYSINFO, &sysinfo);
    CHECK_NOISY(oss_probe_for_adev, &sysinfo);

    gtk_list_store_append(list, &iter);
    gtk_list_store_set(list, &iter, 0, _("1. Default device"), 1, DEFAULT_DSP, -1);

    for (i = 0; i < sysinfo.numaudios; i++)
    {
        oss_audioinfo ainfo;
        ainfo.dev = i;

        CHECK(ioctl, mixerfd, SNDCTL_AUDIOINFO, &ainfo);

        if (ainfo.caps & PCM_CAP_OUTPUT)
        {
            gtk_list_store_append(list, &iter);
            gtk_list_store_set(list, &iter, 0, g_strdup_printf("%d. %s", ++a, ainfo.name), 1, ainfo.devnode, -1);
        }
        else
            continue;
    }

    close(mixerfd);

    return GTK_TREE_MODEL(list);

    FAILED:
        close(mixerfd);
        return NULL;
}

static void select_combo_item(GtkComboBox *combo, gchar *text)
{
    GtkTreeIter iter;
    GtkTreeModel *model = gtk_combo_box_get_model(combo);

    if (!gtk_tree_model_get_iter_first(model, &iter))
        return;

    do
    {
        gchar *devnode;

        gtk_tree_model_get(model, &iter, 1, &devnode, -1);

        if (!g_strcmp0(text, devnode))
            gtk_combo_box_set_active_iter(combo, &iter);
    }
    while (gtk_tree_model_iter_next(model, &iter));
}

static void window_create(void)
{
    GtkWidget *vbox, *dev_list_box, *dev_label, *dev_list_combo, *alt_dev_box, *alt_dev_check,
        *alt_dev_text, *option_box, *vol_check, *cookedmode_check, *button_box, *button_ok,
        *exclusive_check;
    GtkTreeModel *dev_list_model;
    GtkCellRenderer *cell;
    gchar *device;

    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), _("OSS4 Output Plugin Preferences"));
    gtk_window_set_type_hint(GTK_WINDOW(window), GDK_WINDOW_TYPE_HINT_DIALOG);
    gtk_window_set_resizable(GTK_WINDOW(window), FALSE);
    gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
    gtk_container_set_border_width(GTK_CONTAINER(window), 10);

    vbox = gtk_vbox_new(FALSE, 10);
    gtk_container_add(GTK_CONTAINER(window), vbox);

    dev_list_box = gtk_hbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), dev_list_box, FALSE, FALSE, 0);

    dev_label = gtk_label_new(_("Audio device:"));
    gtk_box_pack_start(GTK_BOX(dev_list_box), dev_label, FALSE, FALSE, 5);

    dev_list_model = get_device_list();

    if (!GTK_IS_TREE_MODEL(dev_list_model))
    {
        gtk_widget_destroy(window);
        return;
    }

    dev_list_combo = gtk_combo_box_new_with_model(dev_list_model);

    cell = gtk_cell_renderer_text_new();
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(dev_list_combo), cell, TRUE);
    gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(dev_list_combo), cell, "text", 0, NULL);

    g_object_unref(G_OBJECT(dev_list_model));

    device = aud_get_string("oss4", "device");
    select_combo_item(GTK_COMBO_BOX(dev_list_combo), device);
    g_free(device);

    gtk_box_pack_start(GTK_BOX(dev_list_box), dev_list_combo, TRUE, TRUE, 5);

    alt_dev_box = gtk_hbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), alt_dev_box, FALSE, FALSE, 0);

    alt_dev_check = gtk_check_button_new_with_label(_("Use alternate device:"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(alt_dev_check), aud_get_bool("oss4", "use_alt_device"));

    gtk_box_pack_start(GTK_BOX(alt_dev_box), alt_dev_check, FALSE, FALSE, 5);

    alt_dev_text = gtk_entry_new();
    device = aud_get_string("oss4", "alt_device");
    gtk_entry_set_text(GTK_ENTRY(alt_dev_text), device);
    gtk_widget_set_sensitive(alt_dev_text, aud_get_bool("oss4", "use_alt_device"));
    gtk_box_pack_start(GTK_BOX(alt_dev_box), alt_dev_text, TRUE, TRUE, 5);
    g_free(device);

    option_box = gtk_vbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), option_box, FALSE, FALSE, 0);

    vol_check = gtk_check_button_new_with_label(_("Save volume between sessions"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(vol_check), aud_get_bool("oss4", "save_volume"));
    gtk_box_pack_start(GTK_BOX(option_box), vol_check, FALSE, FALSE, 5);

    cookedmode_check = gtk_check_button_new_with_label(_("Enable format conversions made by the OSS software."));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(cookedmode_check), aud_get_bool("oss4", "cookedmode"));
    gtk_box_pack_start(GTK_BOX(option_box), cookedmode_check, FALSE, FALSE, 5);

    exclusive_check = gtk_check_button_new_with_label(_("Enable exclusive mode to prevent virtual mixing."));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(exclusive_check), aud_get_bool("oss4", "exclusive"));
    gtk_box_pack_start(GTK_BOX(option_box), exclusive_check, FALSE, FALSE, 5);

    button_box = gtk_hbutton_box_new();
    gtk_button_box_set_layout(GTK_BUTTON_BOX(button_box), GTK_BUTTONBOX_END);
    gtk_box_set_spacing(GTK_BOX(button_box), 5);
    gtk_box_pack_start(GTK_BOX(vbox), button_box, TRUE, TRUE, 5);

    button_ok = gtk_button_new_from_stock(GTK_STOCK_OK);
    gtk_box_pack_start(GTK_BOX(button_box), button_ok, FALSE, FALSE, 5);

    gtk_widget_set_can_default(button_ok, TRUE);
    gtk_widget_grab_default(button_ok);

    g_signal_connect(G_OBJECT(dev_list_combo), "changed",
                     G_CALLBACK(dev_list_changed_cb), NULL);

    g_signal_connect(G_OBJECT(alt_dev_text), "changed",
                     G_CALLBACK(alt_dev_text_changed_cb), NULL);

    g_signal_connect(G_OBJECT(alt_dev_check), "toggled",
                     G_CALLBACK(alt_dev_toggled_cb), alt_dev_text);

    g_signal_connect(G_OBJECT(vol_check), "toggled",
                     G_CALLBACK(vol_toggled_cb), NULL);

    g_signal_connect(G_OBJECT(cookedmode_check), "toggled",
                     G_CALLBACK(cookedmode_toggled_cb), NULL);

    g_signal_connect(G_OBJECT(exclusive_check), "toggled",
                     G_CALLBACK(exclusive_toggled_cb), NULL);

    g_signal_connect_swapped(G_OBJECT(button_ok), "clicked",
                             G_CALLBACK(gtk_widget_destroy), window);

    gtk_widget_show_all(window);
}

void oss_configure(void)
{
    if (GTK_IS_WINDOW(window))
    {
        gtk_window_present(GTK_WINDOW(window));
        return;
    }

    window_create();
}
