#include "settings.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

#include <glib.h>
#include <glib/gprintf.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#include <audacious/i18n.h>
#include <audacious/misc.h>
#include <audacious/preferences.h>

#include "plugin.h"
#include "fmt.h"
#include "regex.h"

GtkWidget *entry1, *entry2, *entry3, *cfgdlg, *test_entry, *test_label;
GtkTextBuffer *textbuffer;
guint apply_timeout = 0; /* ID of timeout to save new config */
gboolean running = TRUE; /* if plugin threads are running */

static char *hexify(char *pass, int len)
{
    static char buf[33];
    char *bp = buf;
    char hexchars[] = "0123456789abcdef";
    int i;

    memset(buf, 0, sizeof(buf));

    for(i = 0; i < len; i++) {
        *(bp++) = hexchars[(pass[i] >> 4) & 0x0f];
        *(bp++) = hexchars[pass[i] & 0x0f];
    }
    *bp = 0;
    return buf;
}

static char *pwd = NULL;

static void saveconfig(void)
{
    const char *uid = gtk_entry_get_text(GTK_ENTRY(entry1));
    const char *url = gtk_entry_get_text(GTK_ENTRY(entry3));

    GtkTextIter iter_start, iter_end;
    gtk_text_buffer_get_end_iter(GTK_TEXT_BUFFER(textbuffer), &iter_end);
    gtk_text_buffer_get_start_iter(GTK_TEXT_BUFFER(textbuffer), &iter_start);

    char *regexps = gtk_text_buffer_get_text(GTK_TEXT_BUFFER(textbuffer), &iter_start, &iter_end, TRUE);
    char *eregexps = regex_escape(regexps);
    g_free(regexps);

    unsigned char md5pword[16];
    gsize md5len = 16;

    if (pwd != NULL && pwd[0] != '\0') {
        GChecksum * state = g_checksum_new (G_CHECKSUM_MD5);
        g_checksum_update (state, (unsigned char *) pwd, strlen (pwd));
        g_checksum_get_digest (state, md5pword, & md5len);
        g_checksum_free (state);

        aud_set_string ("audioscrobbler", "password",
         hexify ((gchar *) md5pword, sizeof md5pword));
    }
    if (uid != NULL && uid[0] != '\0') {
        aud_set_string ("audioscrobbler", "username", uid);
    } else {
        aud_set_string ("audioscrobbler", "username", "");
        aud_set_string ("audioscrobbler", "password", "");
    }

    if (url != NULL && url[0] != '\0')
        aud_set_string ("audioscrobbler", "sc_url", url);
    else
        aud_set_string ("audioscrobbler", "sc_url", LASTFM_HS_URL);

    if(regexps != NULL && regexps[0] != '\0'){
        aud_set_string ("audioscrobbler", "regexps", eregexps);
        g_free(eregexps);
    } else {
        aud_set_string ("audioscrobbler", "regexps", "");
    }

}

static gboolean apply_config_changes(gpointer data) {
    apply_timeout = 0;
    saveconfig();
    start();
    running = TRUE;
    return FALSE;
}

static void configure_apply(void) {
    if (apply_timeout) { /* config has been changed, but wasn't saved yet */
        g_source_remove(apply_timeout);
        apply_config_changes(NULL);
    }
}

static void configure_cleanup(void) {
    g_free(pwd);
    pwd = NULL;
}

static void
entry_changed(GtkWidget *widget, gpointer data)
{
    if (running) {
        stop();
        running = FALSE;
    }

    if (apply_timeout)
        g_source_remove(apply_timeout);

    apply_timeout = g_timeout_add_seconds(10, (GSourceFunc) apply_config_changes, NULL);
}

static void entry_focus_in(GtkWidget *widget, gpointer data)
{
    gtk_entry_set_text(GTK_ENTRY(widget), "");
    gtk_entry_set_visibility(GTK_ENTRY(widget), FALSE);
}

static void entry_focus_out(GtkWidget *widget, gpointer data)
{
    if (widget == entry2)
    {
        g_free(pwd);
        pwd = g_strdup(gtk_entry_get_text(GTK_ENTRY(entry2)));
    }

    entry_changed(widget, data);
    gtk_entry_set_text(GTK_ENTRY(widget), _("Change password"));
    gtk_entry_set_visibility(GTK_ENTRY(widget), TRUE);
}

static void regex_test_changed(GtkWidget* widget, gpointer data){
    const gchar* test_data = gtk_entry_get_text(GTK_ENTRY(test_entry));

    GtkTextIter iter_start, iter_end;

    gtk_text_buffer_get_end_iter(GTK_TEXT_BUFFER(textbuffer), &iter_end);
    gtk_text_buffer_get_start_iter(GTK_TEXT_BUFFER(textbuffer), &iter_start);
    gchar *regexps = gtk_text_buffer_get_text(GTK_TEXT_BUFFER(textbuffer), &iter_start, &iter_end, TRUE);

    GSList* rx = regex_str_to_items(regexps);
    g_free(regexps);

    regex_item_t* match = regex_match(test_data, rx);

    if(match == NULL){
        gtk_label_set_text(GTK_LABEL(test_label), "applicable pattern was not found");
    } else {
        char buff[(match->album?strlen(match->album):0)+(match->artist?strlen(match->artist):0)+(match->title?strlen(match->title):0)+25];
        g_sprintf(buff, "artist: %s\ntitle: %s\nalbum: %s", match->artist, match->title, match->album);
        gtk_label_set_text(GTK_LABEL(test_label), buff);
        regex_match_free(match);
    }

    regex_free_list(rx);
}

static void create_cfgdlg_regex(GtkWidget* notebook){
    GtkWidget *label, *textarea, *vbox;
    gchar *regexps, *uregexps;

    vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 6);

    regexps = aud_get_string("audioscrobbler", "regexps");
    uregexps = regex_unescape(regexps);
    g_free(regexps);

    textbuffer = gtk_text_buffer_new(NULL);
    if(uregexps) gtk_text_buffer_set_text(GTK_TEXT_BUFFER(textbuffer), uregexps, -1);
    g_signal_connect(textbuffer, "changed", G_CALLBACK(entry_changed), NULL);

    label = gtk_label_new(_(
"You can define regexp's to parse data send in http streams.\n\
Data feed to parsing is: <artist>—<title>—<album>. Be aware that '—' is not a '-'.\n\
You can define one regexp per line.\n\nExample:"));
    gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 6);

    label = gtk_label_new("^AnimeNfo Radio[^—]+—(?P<artist>.*) - (?P<title>.*) \\((?P<album>.*)\\)—.*$");
    gtk_label_set_selectable(GTK_LABEL(label), TRUE);
    gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 6);

    textarea = gtk_text_view_new_with_buffer(GTK_TEXT_BUFFER(textbuffer));
    gtk_box_pack_start(GTK_BOX(vbox), textarea, TRUE, TRUE, 6);
    gtk_container_set_border_width(GTK_CONTAINER(textarea), 6);

    test_entry = gtk_entry_new();
    g_signal_connect(test_entry, "changed", G_CALLBACK(regex_test_changed), NULL);
    gtk_box_pack_start(GTK_BOX(vbox), test_entry, FALSE, FALSE, 6);

    test_label = gtk_label_new(_("Write something in above field to test your regexps!"));
    gtk_box_pack_start(GTK_BOX(vbox), test_label, FALSE, FALSE, 6);

    label = gtk_label_new(_("<b>Regex</b>"));
    gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), vbox, label);
}

static void *create_cfgdlg(void)
{
    GtkWidget *vbox, *notebook, *grid, *label;
    gchar *username, *sc_url;

    vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    notebook = gtk_notebook_new();

    grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 6);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 6);
    gtk_container_set_border_width(GTK_CONTAINER(grid), 6);

    label = gtk_label_new(_("<b>Services</b>"));
    gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
    gtk_widget_set_halign(label, GTK_ALIGN_START);
    gtk_container_add(GTK_CONTAINER(vbox), label);

    label = gtk_label_new(_("<b>Last.fm</b>"));
    gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), grid, label);

    create_cfgdlg_regex(notebook);

    label = gtk_label_new(_("Username:"));
    gtk_widget_set_halign(label, GTK_ALIGN_END);
    gtk_grid_attach(GTK_GRID(grid), label, 0, 0, 1, 1);

    label = gtk_label_new(_("Password:"));
    gtk_widget_set_halign(label, GTK_ALIGN_END);
    gtk_grid_attach(GTK_GRID(grid), label, 0, 1, 1, 1);

    label = gtk_label_new(_("Scrobbler URL:"));
    gtk_widget_set_halign(label, GTK_ALIGN_END);
    gtk_grid_attach(GTK_GRID(grid), label, 0, 2, 1, 1);

    entry1 = gtk_entry_new();
    gtk_widget_set_hexpand(entry1, TRUE);
    username = aud_get_string("audioscrobbler", "username");
    gtk_entry_set_text(GTK_ENTRY(entry1), username);
    g_signal_connect(entry1, "changed", G_CALLBACK(entry_changed), NULL);
    gtk_grid_attach(GTK_GRID(grid), entry1, 1, 0, 1, 1);
    g_free(username);

    entry2 = gtk_entry_new();
    gtk_widget_set_hexpand(entry2, TRUE);
    gtk_entry_set_text(GTK_ENTRY(entry2), _("Change password"));
    g_signal_connect(entry2, "focus-in-event", G_CALLBACK(entry_focus_in), NULL);
    g_signal_connect(entry2, "focus-out-event", G_CALLBACK(entry_focus_out), NULL);
    gtk_grid_attach(GTK_GRID(grid), entry2, 1, 1, 1, 1);

    entry3 = gtk_entry_new();
    gtk_widget_set_hexpand(entry3, TRUE);
    sc_url = aud_get_string("audioscrobbler", "sc_url");
    gtk_entry_set_text(GTK_ENTRY(entry3), sc_url);
    g_signal_connect(entry3, "changed", G_CALLBACK(entry_changed), NULL);
    gtk_grid_attach(GTK_GRID(grid), entry3, 1, 2, 1, 1);
    g_free(sc_url);

    gtk_box_pack_start(GTK_BOX(vbox), notebook, FALSE, FALSE, 6);
    gtk_widget_show_all(vbox);

    return vbox;
}

/* TODO: don't use WIDGET_CUSTOM there */
static PreferencesWidget settings[] = {
 {WIDGET_CUSTOM, .data = {.populate = create_cfgdlg}}};

PluginPreferences preferences = {
    .widgets = settings,
    .n_widgets = G_N_ELEMENTS (settings),
    .apply = configure_apply,
    .cleanup = configure_cleanup
};
