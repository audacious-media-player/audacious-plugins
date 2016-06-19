/*
 * ALSA Output Plugin for Audacious
 * Copyright 2009-2014 John Lindgren
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

#include <libaudcore/hook.h>
#include <libaudcore/preferences.h>

#include "alsa.h"

const char ALSAPlugin::about[] =
 N_("ALSA Output Plugin for Audacious\n"
    "Copyright 2009-2012 John Lindgren\n\n"
    "My thanks to William Pitcock, author of the ALSA Output Plugin NG, whose "
    "code served as a reference when the ALSA manual was not enough.");

struct NameDescPair {
    String name, desc;
};

static Index<NameDescPair> pcm_list;
static Index<NameDescPair> mixer_list;
static Index<String> element_list;

static Index<ComboItem> pcm_combo_items;
static Index<ComboItem> mixer_combo_items;
static Index<ComboItem> element_combo_items;

static void get_defined_devices (const char * type,
 void (* found) (const char * name, const char * description))
{
    void * * hints = nullptr;
    CHECK (snd_device_name_hint, -1, type, & hints);

    for (int i = 0; hints[i]; i ++)
    {
        char * type = snd_device_name_get_hint (hints[i], "IOID");

        if (! type || ! strcmp (type, "Output"))
        {
            char * name = snd_device_name_get_hint (hints[i], "NAME");
            char * description = snd_device_name_get_hint (hints[i], "DESC");

            if (name && strcmp (name, "default"))
                found (name, description ? description : _("(no description)"));

            free (name);
            free (description);
        }

        free (type);
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
    for (int card = -1;;)
    {
        CHECK (snd_card_next, & card);
        if (card < 0)
            break;

        char * description = get_card_description (card);
        if (description)
        {
            found (card, description);
            free (description);
        }
    }

FAILED:
    return;
}

static String get_device_description (snd_ctl_t * control, int device)
{
    snd_pcm_info_t * info;
    snd_pcm_info_alloca (& info);
    snd_pcm_info_set_device (info, device);
    snd_pcm_info_set_stream (info, SND_PCM_STREAM_PLAYBACK);

    switch (snd_ctl_pcm_info (control, info))
    {
    case 0:
        return String (snd_pcm_info_get_name (info));

    case -ENOENT: /* capture device? */
        return String ();
    }

    CHECK (snd_ctl_pcm_info, control, info);

FAILED:
    return String ();
}

static void get_devices (int card, void (* found) (const char * name, const char * description))
{
    snd_ctl_t * control = nullptr;
    CHECK (snd_ctl_open, & control, str_printf ("hw:%d", card), 0);

    for (int device = -1;;)
    {
        CHECK (snd_ctl_pcm_next_device, control, & device);
        if (device < 0)
            break;

        StringBuf name = str_printf ("hw:%d,%d", card, device);
        String description = get_device_description (control, device);

        if (description)
            found (name, description);
    }

FAILED:
    if (control)
        snd_ctl_close (control);
}

static void pcm_found (const char * name, const char * description)
{
    NameDescPair & pair = pcm_list.append (
        String (name),
        String (str_concat ({name, " – ", description}))
    );

    pcm_combo_items.append (pair.desc, pair.name);
}

static void pcm_card_found (int card, const char * description)
{
    get_devices (card, pcm_found);
}

static void pcm_list_fill ()
{
    pcm_found ("default", _("Default PCM device"));
    get_defined_devices ("pcm", pcm_found);
    get_cards (pcm_card_found);
}

static void mixer_found (const char * name, const char * description)
{
    NameDescPair & pair = mixer_list.append (
        String (name),
        String (str_concat ({name, " – ", description}))
    );

    mixer_combo_items.append (pair.desc, pair.name);
}

static void mixer_card_found (int card, const char * description)
{
    mixer_found (str_printf ("hw:%d", card), description);
}

static void mixer_list_fill ()
{
    mixer_found ("default", _("Default mixer device"));
    get_defined_devices ("ctl", mixer_found);
    get_cards (mixer_card_found);
}

static void get_elements (void (* found) (const char * name))
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

static void element_found (const char * name)
{
    String & str = element_list.append (String (name));
    element_combo_items.append (str, str);
}

static void element_list_fill ()
{
    get_elements (element_found);
}

static void guess_element ()
{
    for (const char * guess : {"Master", "PCM", "Wave"})
    {
        for (const String & element : element_list)
        {
            if (! strcmp (element, guess))
            {
                aud_set_str ("alsa", "mixer-element", guess);
                return;
            }
        }
    }

    AUDERR ("No suitable mixer element found.\n");
}

const char * const ALSAPlugin::defaults[] = {
    "pcm", "default",
    "mixer", "default",
    nullptr
};

void ALSAPlugin::init_config ()
{
    aud_config_set_defaults ("alsa", defaults);

    String element = aud_get_str ("alsa", "mixer-element");

    if (! element[0])
    {
        element_list_fill ();
        guess_element ();

        element_list.clear ();
        element_combo_items.clear ();
    }
}

void ALSAPlugin::pcm_changed ()
{
    aud_output_reset (OutputReset::ReopenStream);
}

void ALSAPlugin::mixer_changed ()
{
    element_list.clear ();
    element_combo_items.clear ();

    element_list_fill ();
    guess_element ();

    hook_call ("alsa mixer changed", nullptr);

    close_mixer ();
    open_mixer ();
}

void ALSAPlugin::element_changed ()
{
    close_mixer ();
    open_mixer ();
}

static ArrayRef<ComboItem> pcm_combo_fill ()
    { return {pcm_combo_items.begin (), pcm_combo_items.len ()}; }
static ArrayRef<ComboItem> mixer_combo_fill ()
    { return {mixer_combo_items.begin (), mixer_combo_items.len ()}; }
static ArrayRef<ComboItem> element_combo_fill ()
    { return {element_combo_items.begin (), element_combo_items.len ()}; }

const PreferencesWidget ALSAPlugin::widgets[] = {
    WidgetCombo (N_("PCM device:"),
        WidgetString ("alsa", "pcm", pcm_changed),
        {nullptr, pcm_combo_fill}),
    WidgetCombo (N_("Mixer device:"),
        WidgetString ("alsa", "mixer", mixer_changed),
        {nullptr, mixer_combo_fill}),
    WidgetCombo (N_("Mixer element:"),
        WidgetString ("alsa", "mixer-element", element_changed, "alsa mixer changed"),
        {nullptr, element_combo_fill})
};

static void alsa_prefs_init ()
{
    pcm_list_fill ();
    mixer_list_fill ();
    element_list_fill ();
}

static void alsa_prefs_cleanup ()
{
    pcm_list.clear ();
    mixer_list.clear ();
    element_list.clear ();

    pcm_combo_items.clear ();
    mixer_combo_items.clear ();
    element_combo_items.clear ();
}

const PluginPreferences ALSAPlugin::prefs = {
    {widgets},
    alsa_prefs_init,
    nullptr,  // apply
    alsa_prefs_cleanup
};
