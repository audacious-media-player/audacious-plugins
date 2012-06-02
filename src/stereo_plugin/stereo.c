/* Extra Stereo Plugin for Audacious
 * Written by Johan Levin, 1999
 * Modified by John Lindgren, 2009-2012 */

#include "config.h"
#include <gtk/gtk.h>

#include <audacious/i18n.h>
#include <audacious/misc.h>
#include <audacious/plugin.h>
#include <audacious/preferences.h>
#include <libaudgui/libaudgui.h>
#include <libaudgui/libaudgui-gtk.h>

static gboolean init (void);
static void about(void);

static void stereo_start (gint * channels, gint * rate);
static void stereo_process (gfloat * * data, gint * samples);
static void stereo_finish (gfloat * * data, gint * samples);

static const gchar * const stereo_defaults[] = {
 "intensity", "2.5",
 NULL};

static const PreferencesWidget stereo_widgets[] = {
 {WIDGET_LABEL, N_("<b>Extra Stereo</b>")},
 {WIDGET_SPIN_BTN, N_("Intensity:"),
  .cfg_type = VALUE_FLOAT, .csect = "extra_stereo", .cname = "intensity",
  .data = {.spin_btn = {0, 10, 0.1}}}};

static const PluginPreferences stereo_prefs = {
 .widgets = stereo_widgets,
 .n_widgets = G_N_ELEMENTS (stereo_widgets)};

AUD_EFFECT_PLUGIN
(
    .name = N_("Extra Stereo"),
    .domain = PACKAGE,
    .prefs = & stereo_prefs,
    .init = init,
    .about = about,
    .start = stereo_start,
    .process = stereo_process,
    .finish = stereo_finish,
    .preserves_format = TRUE
)

static const char *about_text = N_("Extra Stereo Plugin\n\n"
                                   "By Johan Levin 1999.");

static gboolean init (void)
{
    aud_config_set_defaults ("extra_stereo", stereo_defaults);
    return TRUE;
}

static void about (void)
{
    static GtkWidget * about_dialog = NULL;

    audgui_simple_message (& about_dialog, GTK_MESSAGE_INFO,
     _("About Extra Stereo Plugin"), _(about_text));
}

static gint stereo_channels;

static void stereo_start (gint * channels, gint * rate)
{
    stereo_channels = * channels;
}

static void stereo_process (gfloat * * data, gint * samples)
{
    gfloat value = aud_get_double ("extra_stereo", "intensity");
    gfloat * f, * end;
    gfloat center;

    if (stereo_channels != 2 || samples == 0)
        return;

    end = (* data) + (* samples);

    for (f = * data; f < end; f += 2)
    {
        center = (f[0] + f[1]) / 2;
        f[0] = center + (f[0] - center) * value;
        f[1] = center + (f[1] - center) * value;
    }
}

static void stereo_finish (gfloat * * data, gint * samples)
{
    stereo_process (data, samples);
}
