/*
 * ALSA Output Plugin for Audacious
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

#include <gtk/gtk.h>

gchar * alsa_config_pcm = NULL, * alsa_config_mixer = NULL,
 * alsa_config_mixer_element = NULL;
gboolean alsa_config_drop_workaround = TRUE;

static GtkListStore * pcm_list, * mixer_list, * mixer_element_list;
static GtkWidget * window, * pcm_combo, * mixer_combo, * mixer_element_combo,
 * ok_button, * drop_workaround_check;

static GtkTreeIter * list_lookup_member (GtkListStore * list, const gchar *
 text)
{
    GtkTreeIter iter;

    if (! gtk_tree_model_get_iter_first ((GtkTreeModel *) list, & iter))
        return NULL;

    do
    {
        const gchar * iter_text;

        gtk_tree_model_get ((GtkTreeModel *) list, & iter, 0, & iter_text, -1);

        if (! strcmp (iter_text, text))
            return g_memdup (& iter, sizeof (iter));
    }
    while (gtk_tree_model_iter_next ((GtkTreeModel *) list, & iter));

    return NULL;
}

static gboolean list_has_member (GtkListStore * list, const gchar * text)
{
    GtkTreeIter * iter = list_lookup_member (list, text);

    g_free (iter);
    return (iter != NULL);
}

static void get_defined_devices (const gchar * type, gboolean capture, void
 (* found) (const gchar * name, const gchar * description))
{
    void * * hints = NULL;
    gint count;

    CHECK (snd_device_name_hint, -1, type, & hints);

    for (count = 0; hints[count] != NULL; count ++)
    {
        gchar * type = snd_device_name_get_hint (hints[count], "IOID");

        if (type == NULL || ! strcmp (type, capture ? "Input" : "Output"))
        {
            gchar * name = snd_device_name_get_hint (hints[count], "NAME");
            gchar * description = snd_device_name_get_hint (hints[count], "DESC");

            found (name, description);
            free (name);
            free (description);
        }

        free (type);
    }

FAILED:
    if (hints != NULL)
        snd_device_name_free_hint (hints);
}

static gchar * get_card_description (gint card)
{
    static gchar * description;

    CHECK (snd_card_get_name, card, & description);
    return g_strdup (description);

FAILED:
    return NULL;
}

static void get_cards (void (* found) (gint card, const gchar * description))
{
    gint card = -1;

    while (1)
    {
        gchar * description;

        CHECK (snd_card_next, & card);

        if (card < 0)
            break;

        if ((description = get_card_description (card)) != NULL)
        {
            found (card, description);
            g_free (description);
        }
    }

FAILED:
    return;
}

static gchar * get_device_description (snd_ctl_t * control, gint device,
 gboolean capture)
{
    snd_pcm_info_t * info;

    snd_pcm_info_alloca (& info);
    snd_pcm_info_set_device (info, device);
    snd_pcm_info_set_stream (info, capture ? SND_PCM_STREAM_CAPTURE :
     SND_PCM_STREAM_PLAYBACK);

    switch (snd_ctl_pcm_info (control, info))
    {
    case 0:
        return g_strdup (snd_pcm_info_get_name (info));

    case -ENOENT: /* capture but we want playback (or other way around) */
        return NULL;
    }

    CHECK (snd_ctl_pcm_info, control, info);

FAILED:
    return NULL;
}

static void get_devices (gint card, gboolean capture, void (* found)
 (const gchar * name, const gchar * description))
{
    gchar * card_name = g_strdup_printf ("hw:%d", card);
    snd_ctl_t * control = NULL;
    gint device = -1;

    CHECK (snd_ctl_open, & control, card_name, 0);

    while (1)
    {
        gchar * name, * description;

        CHECK (snd_ctl_pcm_next_device, control, & device);

        if (device < 0)
            break;

        name = g_strdup_printf ("hw:%d,%d", card, device);
        description = get_device_description (control, device, capture);

        if (description != NULL)
            found (name, description);

        g_free (name);
        g_free (description);
    }

FAILED:
    if (control != NULL)
        snd_ctl_close (control);

    g_free (card_name);
}

static void pcm_found (const gchar * name, const gchar * description)
{
    GtkTreeIter iter;
    gchar * new = g_strdup_printf ("(%s)", description);

    gtk_list_store_append (pcm_list, & iter);
    gtk_list_store_set (pcm_list, & iter, 0, name, 1, new, -1);

    g_free (new);
}

static void pcm_card_found (gint card, const gchar * description)
{
    get_devices (card, FALSE, pcm_found);
}

static void pcm_list_fill (void)
{
    pcm_found ("default", _("Default PCM device"));
    get_defined_devices ("pcm", FALSE, pcm_found);
    get_cards (pcm_card_found);
}

static void mixer_found (const gchar * name, const gchar * description)
{
    GtkTreeIter iter;
    gchar * new = g_strdup_printf ("(%s)", description);

    gtk_list_store_append (mixer_list, & iter);
    gtk_list_store_set (mixer_list, & iter, 0, name, 1, new, -1);

    g_free (new);
}

static void mixer_card_found (gint card, const gchar * description)
{
    gchar * name = g_strdup_printf ("hw:%d", card);

    mixer_found (name, description);
    g_free (name);
}

static void mixer_list_fill (void)
{
    mixer_found ("default", _("Default mixer device"));
    get_defined_devices ("ctl", FALSE, mixer_found);
    get_cards (mixer_card_found);
}

static void get_mixer_elements (const gchar * mixer, void (* found)
 (const gchar * name))
{
    snd_mixer_t * mixer_handle = NULL;
    snd_mixer_elem_t * element;

    CHECK (snd_mixer_open, & mixer_handle, 0);
    CHECK (snd_mixer_attach, mixer_handle, mixer);
    CHECK (snd_mixer_selem_register, mixer_handle, NULL, NULL);
    CHECK (snd_mixer_load, mixer_handle);

    element = snd_mixer_first_elem (mixer_handle);

    while (element != NULL)
    {
        if (snd_mixer_selem_has_playback_volume (element))
            found (snd_mixer_selem_get_name (element));

        element = snd_mixer_elem_next (element);
    }

FAILED:
    if (mixer_handle != NULL)
        snd_mixer_close (mixer_handle);
}

static void mixer_element_found (const gchar * name)
{
    GtkTreeIter iter;

    gtk_list_store_append (mixer_element_list, & iter);
    gtk_list_store_set (mixer_element_list, & iter, 0, name, -1);
}

static void mixer_element_list_fill (void)
{
    get_mixer_elements (alsa_config_mixer, mixer_element_found);
}

static void guess_mixer_element (void)
{
    static const gchar * guesses[] = {"PCM", "Wave", "Master"};
    gint count;

    if (alsa_config_mixer_element != NULL)
    {
        if (list_has_member (mixer_element_list, alsa_config_mixer_element))
            return;

        g_free (alsa_config_mixer_element);
        alsa_config_mixer_element = NULL;
    }

    for (count = 0; count < G_N_ELEMENTS (guesses); count ++)
    {
        if (list_has_member (mixer_element_list, guesses[count]))
        {
            alsa_config_mixer_element = g_strdup (guesses[count]);
            return;
        }
    }

    ERROR_NOISY ("No suitable mixer element found.\n");
}

void alsa_config_load (void)
{
    mcs_handle_t * database = aud_cfg_db_open ();

    pcm_list = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_STRING);
    mixer_list = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_STRING);
    mixer_element_list = gtk_list_store_new (1, G_TYPE_STRING);

    pcm_list_fill ();
    aud_cfg_db_get_string (database, "alsa", "pcm", & alsa_config_pcm);

    if (alsa_config_pcm == NULL)
        alsa_config_pcm = g_strdup ("default");
    else if (strcmp (alsa_config_pcm, "default") && ! list_has_member (pcm_list,
     alsa_config_pcm))
    {
        g_free (alsa_config_pcm);
        alsa_config_pcm = g_strdup ("default");
    }

    mixer_list_fill ();
    aud_cfg_db_get_string (database, "alsa", "mixer",
     & alsa_config_mixer);

    if (alsa_config_mixer == NULL)
        alsa_config_mixer = g_strdup ("default");
    else if (strcmp (alsa_config_mixer, "default") && ! list_has_member
     (mixer_list, alsa_config_mixer))
    {
        g_free (alsa_config_mixer);
        alsa_config_mixer = g_strdup ("default");
    }

    mixer_element_list_fill ();
    aud_cfg_db_get_string (database, "alsa", "mixer-element",
     & alsa_config_mixer_element);
    guess_mixer_element ();

    aud_cfg_db_get_bool (database, "alsa", "drop-workaround", &
     alsa_config_drop_workaround);

    aud_cfg_db_close (database);
}

void alsa_config_save (void)
{
    mcs_handle_t * database = aud_cfg_db_open ();

    g_object_unref (pcm_list);
    g_object_unref (mixer_list);
    g_object_unref (mixer_element_list);

    aud_cfg_db_set_string (database, "alsa", "pcm", alsa_config_pcm);
    aud_cfg_db_set_string (database, "alsa", "mixer", alsa_config_mixer);
    aud_cfg_db_set_string (database, "alsa", "mixer-element",
     alsa_config_mixer_element);
    aud_cfg_db_set_bool (database, "alsa", "drop-workaround",
     alsa_config_drop_workaround);

    g_free (alsa_config_pcm);
    g_free (alsa_config_mixer);
    g_free (alsa_config_mixer_element);

    aud_cfg_db_close (database);
}

static GtkWidget * combo_new (const gchar * title, GtkListStore * list,
 GtkWidget * * combo, gboolean has_description)
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

    if (has_description)
    {
        cell = gtk_cell_renderer_text_new ();
        gtk_cell_layout_pack_start ((GtkCellLayout *) * combo, cell, FALSE);
        gtk_cell_layout_set_attributes ((GtkCellLayout *) * combo, cell, "text",
         1, NULL);
    }

    return hbox;
}

static void combo_select_by_text (GtkWidget * combo, GtkListStore * list,
 const gchar * text)
{
    GtkTreeIter * iter;

    if (text == NULL)
    {
        gtk_combo_box_set_active ((GtkComboBox *) combo, -1);
        return;
    }

    iter = list_lookup_member (list, text);

    if (iter == NULL)
        return;

    gtk_combo_box_set_active_iter ((GtkComboBox *) combo, iter);
    g_free (iter);
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

static void create_window (void)
{
    GtkWidget * vbox, * hbox;

    window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    gtk_window_set_type_hint ((GtkWindow *) window, GDK_WINDOW_TYPE_HINT_DIALOG);
    gtk_window_set_resizable ((GtkWindow *) window, FALSE);
    gtk_window_set_title ((GtkWindow *) window, _("ALSA Output Plugin "
     "Preferences"));
    gtk_container_set_border_width ((GtkContainer *) window, 6);

    vbox = gtk_vbox_new (FALSE, 6);
    gtk_container_add ((GtkContainer *) window, vbox);

    gtk_box_pack_start ((GtkBox *) vbox, combo_new (_("PCM device:"), pcm_list,
     & pcm_combo, TRUE), FALSE, FALSE, 0);
    gtk_box_pack_start ((GtkBox *) vbox, combo_new (_("Mixer device:"),
     mixer_list, & mixer_combo, TRUE), FALSE, FALSE, 0);
    gtk_box_pack_start ((GtkBox *) vbox, combo_new (_("Mixer element:"),
     mixer_element_list, & mixer_element_combo, FALSE), FALSE, FALSE, 0);

    drop_workaround_check = gtk_check_button_new_with_label (_("Work around "
     "snd_pcm_drop hangup"));
    gtk_toggle_button_set_active ((GtkToggleButton *) drop_workaround_check,
     alsa_config_drop_workaround);
    gtk_box_pack_start ((GtkBox *) vbox, drop_workaround_check, FALSE, FALSE, 0);

    hbox = gtk_hbox_new (FALSE, 6);
    gtk_box_pack_start ((GtkBox *) vbox, hbox, FALSE, FALSE, 0);

    ok_button = gtk_button_new_from_stock (GTK_STOCK_OK);
    gtk_box_pack_end ((GtkBox *) hbox, ok_button, FALSE, FALSE, 0);

    gtk_widget_show_all (window);
}

static void pcm_changed (GtkComboBox * combo, void * unused)
{
    const gchar * new = combo_selected_text (pcm_combo, pcm_list);

    if (new == NULL || ! strcmp (new, alsa_config_pcm))
        return;

    g_free (alsa_config_pcm);
    alsa_config_pcm = g_strdup (combo_selected_text (pcm_combo, pcm_list));
}

static void mixer_changed (GtkComboBox * combo, void * unused)
{
    const gchar * new = combo_selected_text (mixer_combo, mixer_list);

    if (new == NULL || ! strcmp (new, alsa_config_mixer))
        return;

    g_free (alsa_config_mixer);
    alsa_config_mixer = g_strdup (new);

    gtk_list_store_clear (mixer_element_list);
    mixer_element_list_fill ();
    guess_mixer_element ();
    combo_select_by_text (mixer_element_combo, mixer_element_list,
     alsa_config_mixer_element);

    alsa_close_mixer ();
    alsa_open_mixer ();
}

static void mixer_element_changed (GtkComboBox * combo, void * unused)
{
    const gchar * new = combo_selected_text (mixer_element_combo,
     mixer_element_list);

    if (new == NULL || (alsa_config_mixer_element != NULL && ! strcmp (new,
     alsa_config_mixer_element)))
        return;

    g_free (alsa_config_mixer_element);
    alsa_config_mixer_element = g_strdup (new);

    alsa_close_mixer ();
    alsa_open_mixer ();
}

static void boolean_toggled (GtkToggleButton * button, void * data)
{
    * (gboolean *) data = gtk_toggle_button_get_active (button);
}

static void connect_callbacks (void)
{
    g_signal_connect ((GObject *) pcm_combo, "changed", (GCallback) pcm_changed,
     NULL);
    g_signal_connect ((GObject *) mixer_combo, "changed", (GCallback)
     mixer_changed, NULL);
    g_signal_connect ((GObject *) mixer_element_combo, "changed", (GCallback)
     mixer_element_changed, NULL);
    g_signal_connect ((GObject *) drop_workaround_check, "toggled", (GCallback)
     boolean_toggled, & alsa_config_drop_workaround);
    g_signal_connect_swapped ((GObject *) ok_button, "clicked", (GCallback)
     gtk_widget_destroy, window);
    g_signal_connect ((GObject *) window, "destroy", (GCallback)
     gtk_widget_destroyed, & window);
}

void alsa_configure (void)
{
    g_mutex_lock (alsa_mutex);
    alsa_soft_init ();
    g_mutex_unlock (alsa_mutex);

    if (window != NULL)
    {
        gtk_window_present ((GtkWindow *) window);
        return;
    }

    create_window ();
    combo_select_by_text (pcm_combo, pcm_list, alsa_config_pcm);
    combo_select_by_text (mixer_combo, mixer_list, alsa_config_mixer);
    combo_select_by_text (mixer_element_combo, mixer_element_list,
     alsa_config_mixer_element);
    connect_callbacks ();
}
