/*
 * ALSA Output Plugin for Audacious
 * Copyright 2009-2010 John Lindgren
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

#include <alsa/asoundlib.h>
#include <gtk/gtk.h>

#include <audacious/configdb.h>
#include <audacious/i18n.h>
#include <libaudgui/libaudgui.h>
#include <libaudgui/libaudgui-gtk.h>

#include "alsa.h"
#include "config.h"

char * alsa_config_pcm = NULL, * alsa_config_mixer = NULL,
 * alsa_config_mixer_element = NULL;
int alsa_config_drain_workaround = 1;

static GtkListStore * pcm_list, * mixer_list, * mixer_element_list;
static GtkWidget * window, * pcm_combo, * mixer_combo, * mixer_element_combo,
 * ok_button, * drain_workaround_check;

static GtkTreeIter * list_lookup_member (GtkListStore * list, const char * text)
{
    static GtkTreeIter iter;

    if (! gtk_tree_model_get_iter_first ((GtkTreeModel *) list, & iter))
        return NULL;

    do
    {
        char * iter_text;

        gtk_tree_model_get ((GtkTreeModel *) list, & iter, 0, & iter_text, -1);

        if (! strcmp (iter_text, text))
        {
            free (iter_text);
            return & iter;
        }

        free (iter_text);
    }
    while (gtk_tree_model_iter_next ((GtkTreeModel *) list, & iter));

    return NULL;
}

static int list_has_member (GtkListStore * list, const char * text)
{
    return (list_lookup_member (list, text) != NULL);
}

static void get_defined_devices (const char * type, int capture, void (* found)
 (const char * name, const char * description))
{
    void * * hints = NULL;
    int count;

    CHECK (snd_device_name_hint, -1, type, & hints);

    for (count = 0; hints[count] != NULL; count ++)
    {
        char * type = snd_device_name_get_hint (hints[count], "IOID");

        if (type == NULL || ! strcmp (type, capture ? "Input" : "Output"))
        {
            char * name = snd_device_name_get_hint (hints[count], "NAME");
            char * description = snd_device_name_get_hint (hints[count], "DESC");

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

static char * get_card_description (int card)
{
    char * description = NULL;

    CHECK (snd_card_get_name, card, & description);

FAILED:
    return description;
}

static void get_cards (void (* found) (int card, const char * description))
{
    int card = -1;

    while (1)
    {
        char * description;

        CHECK (snd_card_next, & card);

        if (card < 0)
            break;

        if ((description = get_card_description (card)) != NULL)
        {
            found (card, description);
            free (description);
        }
    }

FAILED:
    return;
}

static char * get_device_description (snd_ctl_t * control, int device, int
 capture)
{
    snd_pcm_info_t * info;

    snd_pcm_info_alloca (& info);
    snd_pcm_info_set_device (info, device);
    snd_pcm_info_set_stream (info, capture ? SND_PCM_STREAM_CAPTURE :
     SND_PCM_STREAM_PLAYBACK);

    switch (snd_ctl_pcm_info (control, info))
    {
    case 0:
        return strdup (snd_pcm_info_get_name (info));

    case -ENOENT: /* capture but we want playback (or other way around) */
        return NULL;
    }

    CHECK (snd_ctl_pcm_info, control, info);

FAILED:
    return NULL;
}

static void get_devices (int card, int capture, void (* found) (const char *
 name, const char * description))
{
    char card_name[16];
    snd_ctl_t * control = NULL;
    int device = -1;

    snprintf (card_name, sizeof card_name, "hw:%d", card);
    CHECK (snd_ctl_open, & control, card_name, 0);

    while (1)
    {
        char name[16];
        char * description;

        CHECK (snd_ctl_pcm_next_device, control, & device);

        if (device < 0)
            break;

        snprintf (name, sizeof name, "hw:%d,%d", card, device);
        description = get_device_description (control, device, capture);

        if (description != NULL)
            found (name, description);

        free (description);
    }

FAILED:
    if (control != NULL)
        snd_ctl_close (control);
}

static void pcm_found (const char * name, const char * description)
{
    GtkTreeIter iter;
    char new[512];

    gtk_list_store_append (pcm_list, & iter);
    snprintf (new, sizeof new, "(%s)", description);
    gtk_list_store_set (pcm_list, & iter, 0, name, 1, new, -1);
}

static void pcm_card_found (int card, const char * description)
{
    get_devices (card, 0, pcm_found);
}

static void pcm_list_fill (void)
{
    pcm_found ("default", _("Default PCM device"));
    get_defined_devices ("pcm", 0, pcm_found);
    get_cards (pcm_card_found);
}

static void mixer_found (const char * name, const char * description)
{
    GtkTreeIter iter;
    char new[512];

    gtk_list_store_append (mixer_list, & iter);
    snprintf (new, sizeof new, "(%s)", description);
    gtk_list_store_set (mixer_list, & iter, 0, name, 1, new, -1);
}

static void mixer_card_found (int card, const char * description)
{
    char name[16];

    snprintf (name, sizeof name, "hw:%d", card);
    mixer_found (name, description);
}

static void mixer_list_fill (void)
{
    mixer_found ("default", _("Default mixer device"));
    get_defined_devices ("ctl", 0, mixer_found);
    get_cards (mixer_card_found);
}

static void get_mixer_elements (const char * mixer, void (* found)
 (const char * name))
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

static void mixer_element_found (const char * name)
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
    static const char * guesses[] = {"PCM", "Wave", "Master"};
    int count;

    if (alsa_config_mixer_element != NULL)
    {
        if (list_has_member (mixer_element_list, alsa_config_mixer_element))
            return;

        free (alsa_config_mixer_element);
        alsa_config_mixer_element = NULL;
    }

    for (count = 0; count < G_N_ELEMENTS (guesses); count ++)
    {
        if (list_has_member (mixer_element_list, guesses[count]))
        {
            alsa_config_mixer_element = strdup (guesses[count]);
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
        alsa_config_pcm = strdup ("default");
    else if (strcmp (alsa_config_pcm, "default") && ! list_has_member (pcm_list,
     alsa_config_pcm))
    {
        free (alsa_config_pcm);
        alsa_config_pcm = strdup ("default");
    }

    mixer_list_fill ();
    aud_cfg_db_get_string (database, "alsa", "mixer",
     & alsa_config_mixer);

    if (alsa_config_mixer == NULL)
        alsa_config_mixer = strdup ("default");
    else if (strcmp (alsa_config_mixer, "default") && ! list_has_member
     (mixer_list, alsa_config_mixer))
    {
        free (alsa_config_mixer);
        alsa_config_mixer = strdup ("default");
    }

    mixer_element_list_fill ();
    aud_cfg_db_get_string (database, "alsa", "mixer-element",
     & alsa_config_mixer_element);
    guess_mixer_element ();

    aud_cfg_db_get_bool (database, "alsa", "drain-workaround", &
     alsa_config_drain_workaround);

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
    aud_cfg_db_set_bool (database, "alsa", "drain-workaround",
     alsa_config_drain_workaround);

    free (alsa_config_pcm);
    free (alsa_config_mixer);
    free (alsa_config_mixer_element);

    aud_cfg_db_close (database);
}

static GtkWidget * combo_new (const char * title, GtkListStore * list,
 GtkWidget * * combo, int has_description)
{
    GtkWidget * hbox, * label;
    GtkCellRenderer * cell;

    hbox = gtk_hbox_new (0, 6);

    label = gtk_label_new (title);
    gtk_box_pack_start ((GtkBox *) hbox, label, 0, 0, 0);

    * combo = gtk_combo_box_new_with_model ((GtkTreeModel *) list);
    gtk_box_pack_start ((GtkBox *) hbox, * combo, 1, 1, 0);

    cell = gtk_cell_renderer_text_new ();
    gtk_cell_layout_pack_start ((GtkCellLayout *) * combo, cell, 0);
    gtk_cell_layout_set_attributes ((GtkCellLayout *) * combo, cell, "text", 0,
     NULL);

    if (has_description)
    {
        cell = gtk_cell_renderer_text_new ();
        gtk_cell_layout_pack_start ((GtkCellLayout *) * combo, cell, 0);
        gtk_cell_layout_set_attributes ((GtkCellLayout *) * combo, cell, "text",
         1, NULL);
    }

    return hbox;
}

static void combo_select_by_text (GtkWidget * combo, GtkListStore * list,
 const char * text)
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
}

static const char * combo_selected_text (GtkWidget * combo, GtkListStore * list)
{
    GtkTreeIter iter;
    const char * text;

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
    gtk_window_set_resizable ((GtkWindow *) window, 0);
    gtk_window_set_title ((GtkWindow *) window, _("ALSA Output Plugin "
     "Preferences"));
    gtk_container_set_border_width ((GtkContainer *) window, 6);

    vbox = gtk_vbox_new (0, 6);
    gtk_container_add ((GtkContainer *) window, vbox);

    gtk_box_pack_start ((GtkBox *) vbox, combo_new (_("PCM device:"), pcm_list,
     & pcm_combo, 1), 0, 0, 0);
    gtk_box_pack_start ((GtkBox *) vbox, combo_new (_("Mixer device:"),
     mixer_list, & mixer_combo, 1), 0, 0, 0);
    gtk_box_pack_start ((GtkBox *) vbox, combo_new (_("Mixer element:"),
     mixer_element_list, & mixer_element_combo, 0), 0, 0, 0);

    drain_workaround_check = gtk_check_button_new_with_label (_("Work around "
     "drain hangup"));
    gtk_toggle_button_set_active ((GtkToggleButton *) drain_workaround_check,
     alsa_config_drain_workaround);
    gtk_box_pack_start ((GtkBox *) vbox, drain_workaround_check, 0, 0, 0);

    hbox = gtk_hbox_new (0, 6);
    gtk_box_pack_start ((GtkBox *) vbox, hbox, 0, 0, 0);

    ok_button = gtk_button_new_from_stock (GTK_STOCK_OK);
    gtk_box_pack_end ((GtkBox *) hbox, ok_button, 0, 0, 0);

    gtk_widget_show_all (window);
}

static void pcm_changed (GtkComboBox * combo, void * unused)
{
    const char * new = combo_selected_text (pcm_combo, pcm_list);

    if (new == NULL || ! strcmp (new, alsa_config_pcm))
        return;

    free (alsa_config_pcm);
    alsa_config_pcm = strdup (combo_selected_text (pcm_combo, pcm_list));
}

static void mixer_changed (GtkComboBox * combo, void * unused)
{
    const char * new = combo_selected_text (mixer_combo, mixer_list);

    if (new == NULL || ! strcmp (new, alsa_config_mixer))
        return;

    free (alsa_config_mixer);
    alsa_config_mixer = strdup (new);

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
    const char * new = combo_selected_text (mixer_element_combo,
     mixer_element_list);

    if (new == NULL || (alsa_config_mixer_element != NULL && ! strcmp (new,
     alsa_config_mixer_element)))
        return;

    free (alsa_config_mixer_element);
    alsa_config_mixer_element = strdup (new);

    alsa_close_mixer ();
    alsa_open_mixer ();
}

static void boolean_toggled (GtkToggleButton * button, void * data)
{
    * (int *) data = gtk_toggle_button_get_active (button);
}

static void connect_callbacks (void)
{
    g_signal_connect ((GObject *) pcm_combo, "changed", (GCallback) pcm_changed,
     NULL);
    g_signal_connect ((GObject *) mixer_combo, "changed", (GCallback)
     mixer_changed, NULL);
    g_signal_connect ((GObject *) mixer_element_combo, "changed", (GCallback)
     mixer_element_changed, NULL);
    g_signal_connect ((GObject *) drain_workaround_check, "toggled", (GCallback)
     boolean_toggled, & alsa_config_drain_workaround);
    g_signal_connect_swapped ((GObject *) ok_button, "clicked", (GCallback)
     gtk_widget_destroy, window);
    g_signal_connect ((GObject *) window, "destroy", (GCallback)
     gtk_widget_destroyed, & window);

    audgui_destroy_on_escape (window);
}

void alsa_configure (void)
{
    alsa_soft_init ();

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
