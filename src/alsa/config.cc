/*
 * ALSA Output Plugin for Audacious
 * Copyright 2009-2012 John Lindgren
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

#include <libaudcore/i18n.h>
#include <libaudcore/runtime.h>

#include "alsa.h"

static GtkListStore * pcm_list, * mixer_list, * mixer_element_list;
static GtkWidget * pcm_combo, * mixer_combo, * mixer_element_combo;

static GtkTreeIter * list_lookup_member (GtkListStore * list, const char * text)
{
    static GtkTreeIter iter;

    if (! gtk_tree_model_get_iter_first ((GtkTreeModel *) list, & iter))
        return nullptr;

    do
    {
        char * iter_text;

        gtk_tree_model_get ((GtkTreeModel *) list, & iter, 0, & iter_text, -1);

        if (! strcmp (iter_text, text))
        {
            g_free (iter_text);
            return & iter;
        }

        g_free (iter_text);
    }
    while (gtk_tree_model_iter_next ((GtkTreeModel *) list, & iter));

    return nullptr;
}

static bool list_has_member (GtkListStore * list, const char * text)
{
    return (list_lookup_member (list, text) != nullptr);
}

static void get_defined_devices (const char * type,
 void (* found) (const char * name, const char * description))
{
    void * * hints = nullptr;
    int count;

    CHECK (snd_device_name_hint, -1, type, & hints);

    for (count = 0; hints[count]; count ++)
    {
        char * type = snd_device_name_get_hint (hints[count], "IOID");

        if (! type || ! strcmp (type, "Output"))
        {
            char * name = snd_device_name_get_hint (hints[count], "NAME");
            char * description = snd_device_name_get_hint (hints[count], "DESC");

            found (name, description);
            g_free (name);
            g_free (description);
        }

        g_free (type);
    }

FAILED:
    if (hints)
        snd_device_name_free_hint (hints);
}

static char * get_card_description (int card)
{
    char * description = nullptr;

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

        if ((description = get_card_description (card)))
        {
            found (card, description);
            g_free (description);
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
        return g_strdup (snd_pcm_info_get_name (info));

    case -ENOENT: /* capture but we want playback (or other way around) */
        return nullptr;
    }

    CHECK (snd_ctl_pcm_info, control, info);

FAILED:
    return nullptr;
}

static void get_devices (int card, int capture, void (* found) (const char *
 name, const char * description))
{
    StringBuf card_name = str_printf ("hw:%d", card);
    snd_ctl_t * control = nullptr;
    int device = -1;

    CHECK (snd_ctl_open, & control, card_name, 0);

    while (1)
    {
        char * description;

        CHECK (snd_ctl_pcm_next_device, control, & device);

        if (device < 0)
            break;

        StringBuf name = str_printf ("hw:%d,%d", card, device);
        description = get_device_description (control, device, capture);

        if (description)
            found (name, description);

        g_free (description);
    }

FAILED:
    if (control)
        snd_ctl_close (control);
}

static void pcm_found (const char * name, const char * description)
{
    GtkTreeIter iter;
    gtk_list_store_append (pcm_list, & iter);
    gtk_list_store_set (pcm_list, & iter, 0, name, 1,
     (const char *) str_printf ("(%s)", description), -1);
}

static void pcm_card_found (int card, const char * description)
{
    get_devices (card, 0, pcm_found);
}

static void pcm_list_fill ()
{
    pcm_list = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_STRING);
    pcm_found ("default", _("Default PCM device"));
    get_defined_devices ("pcm", pcm_found);
    get_cards (pcm_card_found);
}

static void mixer_found (const char * name, const char * description)
{
    GtkTreeIter iter;
    gtk_list_store_append (mixer_list, & iter);
    gtk_list_store_set (mixer_list, & iter, 0, name, 1,
     (const char *) str_printf ("(%s)", description), -1);
}

static void mixer_card_found (int card, const char * description)
{
    mixer_found (str_printf ("hw:%d", card), description);
}

static void mixer_list_fill ()
{
    mixer_list = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_STRING);
    mixer_found ("default", _("Default mixer device"));
    get_defined_devices ("ctl", mixer_found);
    get_cards (mixer_card_found);
}

static void get_mixer_elements (void (* found) (const char * name))
{
    snd_mixer_t * mixer_handle = nullptr;
    CHECK (snd_mixer_open, & mixer_handle, 0);
    CHECK (snd_mixer_attach, mixer_handle, aud_get_str ("alsa", "mixer"));
    CHECK (snd_mixer_selem_register, mixer_handle, nullptr, nullptr);
    CHECK (snd_mixer_load, mixer_handle);

    for (snd_mixer_elem_t * element = snd_mixer_first_elem (mixer_handle);
     element; element = snd_mixer_elem_next (element))
    {
        if (snd_mixer_selem_has_playback_volume (element))
            found (snd_mixer_selem_get_name (element));
    }

FAILED:
    if (mixer_handle)
        snd_mixer_close (mixer_handle);
}

static void mixer_element_found (const char * name)
{
    GtkTreeIter iter;

    gtk_list_store_append (mixer_element_list, & iter);
    gtk_list_store_set (mixer_element_list, & iter, 0, name, -1);
}

static void mixer_element_list_fill ()
{
    mixer_element_list = gtk_list_store_new (1, G_TYPE_STRING);
    get_mixer_elements (mixer_element_found);
}

static void guess_mixer_element ()
{
    for (const char * guess : {"Master", "PCM", "Wave"})
    {
        if (list_has_member (mixer_element_list, guess))
        {
            aud_set_str ("alsa", "mixer-element", guess);
            return;
        }
    }

    ERROR_NOISY ("No suitable mixer element found.\n");
}

static const char * const alsa_defaults[] = {
 "pcm", "default",
 "mixer", "default",
 nullptr};

void alsa_init_config ()
{
    aud_config_set_defaults ("alsa", alsa_defaults);

    String mixer_element = aud_get_str ("alsa", "mixer-element");

    if (! mixer_element[0])
    {
        mixer_element_list_fill ();
        guess_mixer_element ();
        g_object_unref (mixer_element_list);
        mixer_element_list = nullptr;
    }
}

static GtkWidget * combo_new (const char * title, GtkListStore * list,
 GtkWidget * * combo, bool has_description)
{
    GtkWidget * hbox, * label;
    GtkCellRenderer * cell;

    hbox = gtk_hbox_new (FALSE, 6);

    label = gtk_label_new (title);
    gtk_box_pack_start ((GtkBox *) hbox, label, 0, 0, 0);

    * combo = gtk_combo_box_new_with_model ((GtkTreeModel *) list);
    gtk_box_pack_start ((GtkBox *) hbox, * combo, 1, 1, 0);

    cell = gtk_cell_renderer_text_new ();
    gtk_cell_layout_pack_start ((GtkCellLayout *) * combo, cell, 0);
    gtk_cell_layout_set_attributes ((GtkCellLayout *) * combo, cell, "text", 0, nullptr);

    if (has_description)
    {
        cell = gtk_cell_renderer_text_new ();
        gtk_cell_layout_pack_start ((GtkCellLayout *) * combo, cell, 0);
        gtk_cell_layout_set_attributes ((GtkCellLayout *) * combo, cell, "text", 1, nullptr);
    }

    return hbox;
}

static void combo_select_by_text (GtkWidget * combo, GtkListStore * list, const char * text)
{
    GtkTreeIter * iter = text ? list_lookup_member (list, text) : nullptr;
    gtk_combo_box_set_active_iter ((GtkComboBox *) combo, iter);
}

static const char * combo_selected_text (GtkWidget * combo, GtkListStore * list)
{
    GtkTreeIter iter;
    const char * text;

    if (! gtk_combo_box_get_active_iter ((GtkComboBox *) combo, & iter))
        return nullptr;

    gtk_tree_model_get ((GtkTreeModel *) list, & iter, 0, & text, -1);
    return text;
}

static GtkWidget * create_vbox ()
{
    GtkWidget * vbox = gtk_vbox_new (FALSE, 6);

    gtk_box_pack_start ((GtkBox *) vbox, combo_new (_("PCM device:"), pcm_list,
     & pcm_combo, true), 0, 0, 0);
    gtk_box_pack_start ((GtkBox *) vbox, combo_new (_("Mixer device:"),
     mixer_list, & mixer_combo, true), 0, 0, 0);
    gtk_box_pack_start ((GtkBox *) vbox, combo_new (_("Mixer element:"),
     mixer_element_list, & mixer_element_combo, false), 0, 0, 0);

    return vbox;
}

static void pcm_changed (GtkComboBox * combo, void * unused)
{
    const char * pcm = combo_selected_text (pcm_combo, pcm_list);

    if (! pcm || ! strcmp (pcm, aud_get_str ("alsa", "pcm")))
        return;

    aud_set_str ("alsa", "pcm", pcm);

    aud_output_reset (OutputReset::ReopenStream);
}

static void mixer_changed (GtkComboBox * combo, void * unused)
{
    const char * mixer = combo_selected_text (mixer_combo, mixer_list);

    if (! mixer || ! strcmp (mixer, aud_get_str ("alsa", "mixer")))
        return;

    aud_set_str ("alsa", "mixer", mixer);

    gtk_list_store_clear (mixer_element_list);
    get_mixer_elements (mixer_element_found);

    guess_mixer_element ();
    combo_select_by_text (mixer_element_combo, mixer_element_list,
     aud_get_str ("alsa", "mixer-element"));

    alsa_close_mixer ();
    alsa_open_mixer ();
}

static void mixer_element_changed (GtkComboBox * combo, void * unused)
{
    const char * element = combo_selected_text (mixer_element_combo,
     mixer_element_list);

    if (! element || ! strcmp (element, aud_get_str ("alsa", "mixer-element")))
        return;

    aud_set_str ("alsa", "mixer-element", element);

    alsa_close_mixer ();
    alsa_open_mixer ();
}

static void cleanup ()
{
    g_object_unref (pcm_list);
    g_object_unref (mixer_list);
    g_object_unref (mixer_element_list);

    pcm_list = nullptr;
    mixer_list = nullptr;
    mixer_element_list = nullptr;
}

void * alsa_create_config_widget ()
{
    pcm_list_fill ();
    mixer_list_fill ();
    mixer_element_list_fill ();

    GtkWidget * vbox = create_vbox ();
    g_signal_connect (vbox, "destroy", (GCallback) cleanup, nullptr);

    combo_select_by_text (pcm_combo, pcm_list, aud_get_str ("alsa", "pcm"));
    combo_select_by_text (mixer_combo, mixer_list, aud_get_str ("alsa", "mixer"));
    combo_select_by_text (mixer_element_combo, mixer_element_list,
     aud_get_str ("alsa", "mixer-element"));

    g_signal_connect (pcm_combo, "changed", (GCallback) pcm_changed, nullptr);
    g_signal_connect (mixer_combo, "changed", (GCallback) mixer_changed, nullptr);
    g_signal_connect (mixer_element_combo, "changed", (GCallback) mixer_element_changed, nullptr);

    return vbox;
}
