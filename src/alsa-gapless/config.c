/*
 * ALSA Gapless Output Plugin for Audacious
 * Copyright 2009 John Lindgren
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

#include "alsa.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>

#define DEFAULT_MIXER_ELEMENT "PCM"

gboolean alsa_config_override = FALSE;
gint alsa_config_card = 0;
gint alsa_config_device = 0;
gchar * alsa_config_mixer_element = NULL;

static GtkListStore * card_list = NULL, * device_list = NULL,
 * mixer_element_list = NULL;
static GtkWidget * window, * override_check, * card_combo, * device_combo,
 * mixer_element_combo, * ok_button;

void alsa_config_load (void)
{
    mcs_handle_t * database = aud_cfg_db_open ();

    aud_cfg_db_get_bool (database, "alsa-gapless", "override",
     & alsa_config_override);
    aud_cfg_db_get_int (database, "alsa-gapless", "card", & alsa_config_card);
    aud_cfg_db_get_int (database, "alsa-gapless", "device", & alsa_config_device);
    aud_cfg_db_get_string (database, "alsa-gapless", "mixer-element",
     & alsa_config_mixer_element);

    if (alsa_config_mixer_element == NULL)
        alsa_config_mixer_element = g_strdup (DEFAULT_MIXER_ELEMENT);

    aud_cfg_db_close (database);
}

void alsa_config_save (void)
{
    mcs_handle_t * database = aud_cfg_db_open ();

    aud_cfg_db_set_bool (database, "alsa-gapless", "override",
     alsa_config_override);
    aud_cfg_db_set_int (database, "alsa-gapless", "card", alsa_config_card);
    aud_cfg_db_set_int (database, "alsa-gapless", "device", alsa_config_device);
    aud_cfg_db_set_string (database, "alsa-gapless", "mixer-element",
     alsa_config_mixer_element);

    aud_cfg_db_close (database);
}

static gchar * get_card_name (gint card)
{
    static gchar * name;

    CHECK (snd_card_get_name, card, & name);
    return g_strdup (name);

FAILED:
    return g_strdup ("Unknown card");
}

static void get_cards (void (* found) (gint card, const gchar * name))
{
    gint card = -1;

    while (1)
    {
        gchar * name;

        CHECK (snd_card_next, & card);

        if (card < 0)
            break;

        name = get_card_name (card);
        found (card, name);
        g_free (name);
    }

FAILED:
    return;
}

static gchar * get_device_name (snd_ctl_t * control, gint device)
{
    snd_pcm_info_t * info;

    snd_pcm_info_alloca (& info);
    snd_pcm_info_set_device (info, device);
    CHECK (snd_ctl_pcm_info, control, info);
    return g_strdup (snd_pcm_info_get_name (info));

FAILED:
    return g_strdup ("Unknown device");
}

static void get_devices (gint card, void (* found) (gint device, const gchar *
 name))
{
    gchar * code = g_strdup_printf ("hw:%d", card);
    snd_ctl_t * control = NULL;
    gint device = -1;

    CHECK (snd_ctl_open, & control, code, 0);

    while (1)
    {
        gchar * name;

        CHECK (snd_ctl_pcm_next_device, control, & device);

        if (device < 0)
            break;

        name = get_device_name (control, device);
        found (device, name);
        g_free (name);
    }

FAILED:
    if (control != NULL)
        snd_ctl_close (control);

    g_free (code);
}

static void get_mixer_elements (gint card, void (* found) (const gchar * name))
{
    gchar * code = g_strdup_printf ("hw:%d", card);
    snd_mixer_t * mixer = NULL;
     snd_mixer_elem_t * element;

    CHECK (snd_mixer_open, & mixer, 0);
    CHECK (snd_mixer_attach, mixer, code);
    CHECK (snd_mixer_selem_register, mixer, NULL, NULL);
    CHECK (snd_mixer_load, mixer);

    element = snd_mixer_first_elem (mixer);

    while (element != NULL)
    {
        if (snd_mixer_selem_has_playback_volume (element))
            found (snd_mixer_selem_get_name (element));

        element = snd_mixer_elem_next (element);
    }

FAILED:
    if (mixer != NULL)
        snd_mixer_close (mixer);

    g_free (code);
}

static GtkWidget * combo_new (const gchar * title, GtkListStore * list,
 GtkWidget * * combo)
{
    GtkWidget * hbox, * label;
    GtkCellRenderer * cell;

    hbox = gtk_hbox_new (FALSE, 6);

    label = gtk_label_new (title);
    gtk_box_pack_start ((GtkBox *) hbox, label, FALSE, FALSE, 0);

    * combo = gtk_combo_box_new_with_model ((GtkTreeModel *) list);
    gtk_box_pack_start ((GtkBox *) hbox, * combo, TRUE, TRUE, 0);

    cell = gtk_cell_renderer_text_new ();
    gtk_cell_layout_pack_start ((GtkCellLayout *) * combo, cell, FALSE);
    gtk_cell_layout_set_attributes ((GtkCellLayout *) * combo, cell, "text", 0,
     NULL);

    return hbox;
}

static void combo_select_by_text (GtkWidget * combo, GtkListStore * list,
 const gchar * text)
{
    GtkTreeIter iter;

    if (! gtk_tree_model_get_iter_first ((GtkTreeModel *) list, & iter))
        return;

    while (1)
    {
        const gchar * iter_text;

        gtk_tree_model_get ((GtkTreeModel *) list, & iter, 0, & iter_text, -1);

        if (! strcmp (iter_text, text))
            break;

        if (! gtk_tree_model_iter_next ((GtkTreeModel *) list, & iter))
            return;
    }

    gtk_combo_box_set_active_iter ((GtkComboBox *) combo, & iter);
}

static const gchar * combo_selected_text (GtkWidget * combo, GtkListStore *
 list)
{
    GtkTreeIter iter;
    const gchar * text;

    if (! gtk_combo_box_get_active_iter ((GtkComboBox *) combo, & iter))
        return NULL;

    gtk_tree_model_get ((GtkTreeModel *) list, & iter, 0, & text, -1);
    return text;
}

static void combo_select_by_num (GtkWidget * combo, GtkListStore * list, gint
 num)
{
    GtkTreeIter iter;

    if (! gtk_tree_model_get_iter_first ((GtkTreeModel *) list, & iter))
        return;

    while (1)
    {
        gint iter_num;

        gtk_tree_model_get ((GtkTreeModel *) list, & iter, 1, & iter_num, -1);

        if (iter_num == num)
            break;

        if (! gtk_tree_model_iter_next ((GtkTreeModel *) list, & iter))
            return;
    }

    gtk_combo_box_set_active_iter ((GtkComboBox *) combo, & iter);
}

static gint combo_selected_num (GtkWidget * combo, GtkListStore * list)
{
    GtkTreeIter iter;
    gint num;

    if (! gtk_combo_box_get_active_iter ((GtkComboBox *) combo, & iter))
        return 0;

    gtk_tree_model_get ((GtkTreeModel *) list, & iter, 1, & num, -1);
    return num;
}

static void create_window (void)
{
    GtkWidget * vbox, * hbox;

    window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    gtk_window_set_type_hint ((GtkWindow *) window, GDK_WINDOW_TYPE_HINT_DIALOG);
    gtk_window_set_resizable ((GtkWindow *) window, FALSE);
    gtk_window_set_title ((GtkWindow *) window, _("ALSA Gapless Output Plugin "
     "Preferences"));
    gtk_container_set_border_width ((GtkContainer *) window, 6);

    vbox = gtk_vbox_new (FALSE, 6);
    gtk_container_add ((GtkContainer *) window, vbox);

    override_check = gtk_check_button_new_with_label (_("Override default "
     "devices"));
    gtk_box_pack_start ((GtkBox *) vbox, override_check, FALSE, FALSE, 0);

    if (card_list == NULL)
        card_list = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_INT);
    else
        gtk_list_store_clear (card_list);

    if (device_list == NULL)
        device_list = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_INT);
    else
        gtk_list_store_clear (device_list);

    if (mixer_element_list == NULL)
        mixer_element_list = gtk_list_store_new (1, G_TYPE_STRING);
    else
        gtk_list_store_clear (mixer_element_list);

    gtk_box_pack_start ((GtkBox *) vbox, combo_new (_("Card:"), card_list,
     & card_combo), FALSE, FALSE, 0);
    gtk_box_pack_start ((GtkBox *) vbox, combo_new (_("Device:"), device_list,
     & device_combo), FALSE, FALSE, 0);
    gtk_box_pack_start ((GtkBox *) vbox, combo_new (_("Mixer element:"),
     mixer_element_list, & mixer_element_combo), FALSE, FALSE, 0);

    hbox = gtk_hbox_new (FALSE, 6);
    gtk_box_pack_start ((GtkBox *) vbox, hbox, FALSE, FALSE, 0);

    ok_button = gtk_button_new_from_stock (GTK_STOCK_OK);
    gtk_box_pack_end ((GtkBox *) hbox, ok_button, FALSE, FALSE, 0);

    gtk_widget_show_all (window);
}

static void card_found (gint card, const gchar * name)
{
    GtkTreeIter iter;

    gtk_list_store_append (card_list, & iter);
    gtk_list_store_set (card_list, & iter, 0, name, 1, card, -1);
}

static void device_found (gint device, const gchar * name)
{
    GtkTreeIter iter;

    gtk_list_store_append (device_list, & iter);
    gtk_list_store_set (device_list, & iter, 0, name, 1, device, -1);
}

static void mixer_element_found (const gchar * name)
{
    GtkTreeIter iter;

    gtk_list_store_append (mixer_element_list, & iter);
    gtk_list_store_set (mixer_element_list, & iter, 0, name, -1);
}

static void set_initial_state (void)
{
    gtk_toggle_button_set_active ((GtkToggleButton *) override_check,
     alsa_config_override);

    get_cards (card_found);
    combo_select_by_num (card_combo, card_list, alsa_config_card);
    gtk_widget_set_sensitive (card_combo, alsa_config_override);

    get_devices (alsa_config_card, device_found);
    combo_select_by_num (device_combo, device_list, alsa_config_device);
    gtk_widget_set_sensitive (device_combo, alsa_config_override);

    get_mixer_elements (alsa_config_card, mixer_element_found);
    combo_select_by_text (mixer_element_combo, mixer_element_list,
     alsa_config_mixer_element);
    gtk_widget_set_sensitive (mixer_element_combo, alsa_config_override);
}

static void override_toggled (GtkToggleButton * toggle, void * unused)
{
    alsa_config_override = gtk_toggle_button_get_active (toggle);

    gtk_widget_set_sensitive (card_combo, alsa_config_override);
    gtk_widget_set_sensitive (device_combo, alsa_config_override);
    gtk_widget_set_sensitive (mixer_element_combo, alsa_config_override);

    alsa_close_mixer ();
    alsa_open_mixer ();
}

static void card_changed (GtkComboBox * combo, void * unused)
{
    gint new = combo_selected_num (card_combo, card_list);

    if (alsa_config_card == new)
        return;

    alsa_config_card = new;

    alsa_config_device = 0;
    gtk_list_store_clear (device_list);
    get_devices (alsa_config_card, device_found);
    combo_select_by_num (device_combo, device_list, alsa_config_device);

    gtk_list_store_clear (mixer_element_list);
    get_mixer_elements (alsa_config_card, mixer_element_found);
    combo_select_by_text (mixer_element_combo, mixer_element_list,
     alsa_config_mixer_element);

    alsa_close_mixer ();
    alsa_open_mixer ();
}

static void device_changed (GtkComboBox * combo, void * unused)
{
    alsa_config_device = combo_selected_num (device_combo, device_list);
}

static void mixer_element_changed (GtkComboBox * combo, void * unused)
{
    const gchar * new = combo_selected_text (mixer_element_combo,
     mixer_element_list);

    if (new == NULL)
        new = DEFAULT_MIXER_ELEMENT;

    if (! strcmp (alsa_config_mixer_element, new))
        return;

    g_free (alsa_config_mixer_element);
    alsa_config_mixer_element = g_strdup (new);

    alsa_close_mixer ();
    alsa_open_mixer ();
}

static void connect_callbacks (void)
{
    g_signal_connect ((GObject *) override_check, "toggled", (GCallback)
     override_toggled, NULL);
    g_signal_connect ((GObject *) card_combo, "changed", (GCallback)
     card_changed, NULL);
    g_signal_connect ((GObject *) device_combo, "changed", (GCallback)
     device_changed, NULL);
    g_signal_connect ((GObject *) mixer_element_combo, "changed", (GCallback)
     mixer_element_changed, NULL);
    g_signal_connect_swapped ((GObject *) ok_button, "clicked", (GCallback)
     gtk_widget_destroy, window);
    g_signal_connect ((GObject *) window, "destroy", (GCallback)
     gtk_widget_destroyed, & window);
}

void alsa_configure (void)
{
    if (window == NULL)
    {
        create_window ();
        set_initial_state ();
        connect_callbacks ();
    }

    gtk_window_present ((GtkWindow *) window);
}
