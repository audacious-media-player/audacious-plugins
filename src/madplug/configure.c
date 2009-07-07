/*
 * mad plugin for audacious
 * Copyright (C) 2009 Tomasz Moń
 * Copyright (C) 2005-2007 William Pitcock, Yoshiki Yazawa
 *
 * Portions derived from xmms-mad:
 * Copyright (C) 2001-2002 Sam Clegg - See COPYING
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; under version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

/* #define AUD_DEBUG 1 */

#include "plugin.h"

#include <gtk/gtk.h>
#include <audacious/preferences.h>
#include <math.h>

static GtkWidget *configure_win = NULL;

static audmad_config_t config;

static PreferencesWidget metadata_settings_elements[] = {
    {WIDGET_CHK_BTN, N_("Enable fast play-length calculation"), &config.fast_play_time_calc, NULL, NULL, FALSE},
    {WIDGET_CHK_BTN, N_("Parse XING headers"), &config.use_xing, NULL, NULL, FALSE},
    {WIDGET_CHK_BTN, N_("Use SJIS to write ID3 tags (not recommended)"), &config.sjis, NULL, NULL, FALSE},
};

static PreferencesWidget metadata_settings[] = {
    {WIDGET_BOX, N_("Metadata Settings"), NULL, NULL, NULL, FALSE,
        {.box = {metadata_settings_elements,
                 G_N_ELEMENTS(metadata_settings_elements),
                 FALSE /* vertical */, TRUE /* frame */}}},
};

static PreferencesWidget title_settings[] = {
    {WIDGET_CHK_BTN, N_("Override generic titles"), &config.title_override, NULL, NULL, FALSE},
    {WIDGET_ENTRY, N_("ID3 format:"), &config.id3_format, NULL, NULL, TRUE, {.entry = {FALSE}}, VALUE_STRING},
};

static void
update_config()
{
    if (audmad_config->id3_format != NULL)
        g_free(audmad_config->id3_format);

    memcpy(audmad_config, &config, sizeof(config));
}

static void
save_config(void)
{
    mcs_handle_t *db = aud_cfg_db_open();

    //metadata
    aud_cfg_db_set_bool(db, "MAD", "fast_play_time_calc",
                        audmad_config->fast_play_time_calc);
    aud_cfg_db_set_bool(db, "MAD", "use_xing", audmad_config->use_xing);
    aud_cfg_db_set_bool(db, "MAD", "sjis", audmad_config->sjis);

    //text
    aud_cfg_db_set_bool(db, "MAD", "title_override", audmad_config->title_override);
    aud_cfg_db_set_string(db, "MAD", "id3_format", audmad_config->id3_format);

    aud_cfg_db_close(db);
}

static void
configure_win_cancel(GtkWidget *widget, gpointer data)
{
    AUDDBG("cancel\n");
    if (config.id3_format != NULL) {
        g_free(config.id3_format);
        config.id3_format = NULL;
    }
    gtk_widget_destroy(configure_win);
}

static void
configure_win_ok(GtkWidget *widget, gpointer data)
{
    AUDDBG("ok\n");
    update_config();
    save_config();
    gtk_widget_destroy(configure_win);
    config.id3_format = NULL;
}

static void
configure_destroy(GtkWidget *w, gpointer data)
{
}

void
audmad_configure(void)
{
    GtkWidget *vbox;
    GtkWidget *bbox, *ok, *cancel;
    GtkWidget *notebook, *vbox2;

    if (configure_win != NULL) {
        gtk_widget_show(configure_win);
        return;
    }

    memcpy(&config, audmad_config, sizeof(config));
    if (audmad_config->id3_format != NULL)
        config.id3_format = g_strdup(audmad_config->id3_format);

    configure_win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_type_hint(GTK_WINDOW(configure_win), GDK_WINDOW_TYPE_HINT_DIALOG);

    g_signal_connect(G_OBJECT(configure_win), "destroy",
                       G_CALLBACK(gtk_widget_destroyed), &configure_win);
    g_signal_connect(G_OBJECT(configure_win), "destroy",
                       G_CALLBACK(configure_destroy), &configure_win);

    gtk_window_set_title(GTK_WINDOW(configure_win),
                         _("MPEG Audio Plugin Configuration"));
    gtk_window_set_policy(GTK_WINDOW(configure_win), FALSE, FALSE, FALSE);
    gtk_container_border_width(GTK_CONTAINER(configure_win), 10);

    vbox = gtk_vbox_new(FALSE, 10);
    gtk_container_add(GTK_CONTAINER(configure_win), vbox);

    notebook = gtk_notebook_new();
    gtk_box_pack_start(GTK_BOX(vbox), notebook, FALSE, FALSE, 0);


    /********************************************************************************/


    vbox2 = gtk_vbox_new(FALSE, 5);

    aud_create_widgets(GTK_BOX(vbox2), metadata_settings, G_N_ELEMENTS(metadata_settings));

    // add to notebook
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), vbox2, gtk_label_new(_("General")));



    vbox2 = gtk_vbox_new(FALSE, 5);

    aud_create_widgets(GTK_BOX(vbox2), title_settings, G_N_ELEMENTS(title_settings));

    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), vbox2, gtk_label_new(_("Title")));



    /*********************************************************************************/


    bbox = gtk_hbutton_box_new();
    gtk_button_box_set_layout(GTK_BUTTON_BOX(bbox), GTK_BUTTONBOX_END);
    gtk_button_box_set_spacing(GTK_BUTTON_BOX(bbox), 5);
    gtk_box_pack_start(GTK_BOX(vbox), bbox, FALSE, FALSE, 0);

    cancel = gtk_button_new_from_stock(GTK_STOCK_CANCEL);

    g_signal_connect(G_OBJECT(cancel), "clicked",
                     G_CALLBACK(configure_win_cancel), NULL);

    gtk_box_pack_start(GTK_BOX(bbox), cancel, TRUE, TRUE, 0);

    ok = gtk_button_new_from_stock(GTK_STOCK_OK);

    g_signal_connect(G_OBJECT(ok), "clicked",
                     G_CALLBACK(configure_win_ok), NULL);

    gtk_box_pack_start(GTK_BOX(bbox), ok, TRUE, TRUE, 0);
    gtk_widget_grab_default(ok);

    gtk_widget_show_all(configure_win);
}
