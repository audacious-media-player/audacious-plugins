/*
 * search-tool.c
 * Copyright 2011 John Lindgren
 *
 * This file is part of Audacious.
 *
 * Audacious is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, version 2 or version 3 of the License.
 *
 * Audacious is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 * A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * Audacious. If not, see <http://www.gnu.org/licenses/>.
 *
 * The Audacious team does not consider modular code linking to Audacious or
 * using our public API to be a derived work.
 */

#include <string.h>

#include <gtk/gtk.h>

#include <audacious/gtk-compat.h>
#include <audacious/i18n.h>
#include <audacious/misc.h>
#include <audacious/playlist.h>
#include <audacious/plugin.h>
#include <libaudcore/audstrings.h>
#include <libaudcore/hook.h>
#include <libaudgui/list.h>

#include "config.h"

#define MAX_RESULTS 12
#define UPDATE_DELAY 300

enum {GENRE, ARTIST, ALBUM, TITLE, FIELDS};
enum {UPDATE_ITEMS = 1, UPDATE_DICTS};
enum {COLUMN_NAME, COLUMN_MATCHES};

static const gchar * const field_names[] = {N_("Genre"), N_("Artist"),
 N_("Album"), N_("Title")};

typedef struct {
    gchar * name, * folded;
    GArray * matches;
} Item;

static gint playlist_id;
static gchar * * search_terms;

static GHashTable * added_table;
static GHashTable * dicts[FIELDS];
static struct index * items;

static GtkWidget * help_label, * wait_label, * scrolled, * results_list;
static gint update_source, update_type;

static Item * item_new (gint field, const gchar * name)
{
    Item * item = g_slice_new (Item);
    item->name = g_strdup_printf ("%s: %s", _(field_names[field]), name);
    item->folded = g_utf8_casefold (name, -1);
    item->matches = g_array_new (FALSE, FALSE, sizeof (gint));
    return item;
}

static void item_free (Item * item)
{
    g_free (item->name);
    g_free (item->folded);
    g_array_free (item->matches, TRUE);
    g_slice_free (Item, item);
}

static void find_playlist (void)
{
    playlist_id = -1;

    for (gint p = 0; playlist_id < 0 && p < aud_playlist_count (); p ++)
    {
        gchar * title = aud_playlist_get_title (p);

        if (! strcmp (title, _("Library")))
            playlist_id = aud_playlist_get_unique_id (p);

        g_free (title);
    }
}

static void create_playlist (void)
{
    aud_playlist_insert (0);
    aud_playlist_set_title (0, _("Library"));
    playlist_id = aud_playlist_get_unique_id (0);
}

static void set_search_phrase (const gchar * phrase)
{
    g_strfreev (search_terms);

    gchar * folded = g_utf8_casefold (phrase, -1);
    search_terms = g_strsplit (folded, " ", -1);
    g_free (folded);
}

static gchar * get_path (void)
{
    gchar * path = aud_get_string ("search-tool", "path");
    if (g_file_test (path, G_FILE_TEST_EXISTS))
        return path;

    g_free (path);
    path = g_build_filename (g_get_home_dir (), "Music", NULL);
    if (g_file_test (path, G_FILE_TEST_EXISTS))
        return path;

    g_free (path);
    return g_strdup (g_get_home_dir ());
}

static void destroy_added_table (void)
{
    if (! added_table)
        return;

    g_hash_table_destroy (added_table);
    added_table = NULL;
}

static void create_added_table (gint list)
{
    destroy_added_table ();
    added_table = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

    gint entries = aud_playlist_entry_count (list);
    for (gint i = 0; i < entries; i ++)
        g_hash_table_insert (added_table, aud_playlist_entry_get_filename (list, i), NULL);
}

static void destroy_dicts (void)
{
    for (gint f = 0; f < FIELDS; f ++)
    {
        if (! dicts[f])
            continue;

        g_hash_table_destroy (dicts[f]);
        dicts[f] = NULL;
    }
}

static void create_dicts (gint list)
{
    destroy_dicts ();
    for (gint f = 0; f < FIELDS; f ++)
        dicts[f] = g_hash_table_new_full (g_str_hash, g_str_equal, g_free,
         (GDestroyNotify) item_free);

    gint entries = aud_playlist_entry_count (list);

    for (gint e = 0; e < entries; e ++)
    {
        gchar * fields[FIELDS];

        Tuple * tuple = aud_playlist_entry_get_tuple (list, e, TRUE);
        const gchar * genre = tuple ? tuple_get_string (tuple, FIELD_GENRE, NULL) : NULL;
        fields[GENRE] = genre ? g_strdup (genre) : NULL;

        if (tuple)
            tuple_free (tuple)

        aud_playlist_entry_describe (list, e, & fields[TITLE], & fields[ARTIST],
         & fields[ALBUM], TRUE);

        for (gint f = 0; f < FIELDS; f ++)
        {
            if (! fields[f])
                continue;

            Item * item = g_hash_table_lookup (dicts[f], fields[f]);

            if (item)
                g_free (fields[f]);
            else
            {
                item = item_new (f, fields[f]);
                g_hash_table_insert (dicts[f], fields[f], item);
            }

            g_array_append_val (item->matches, e);
        }
    }
}

static void search_cb (void * key, void * item, void * index)
{
    if (index_count (index) > MAX_RESULTS)
        return;

    for (gint t = 0; search_terms[t]; t ++)
    {
        if (! strstr (((Item *) item)->folded, search_terms[t]))
            return;
    }

    index_append (index, item);
}

static gint item_compare (const void * a, const void * b)
{
    return string_compare (((const Item *) a)->name, ((const Item *) b)->name);
}

static void do_search (void)
{
    index_delete (items, 0, index_count (items));

    for (gint f = 0; f < FIELDS; f ++)
    {
        if (! dicts[f])
            continue;

        struct index * index = index_new ();
        g_hash_table_foreach (dicts[f], search_cb, index);

        if (index_count (index) <= MAX_RESULTS)
        {
            index_sort (index, item_compare);
            index_merge_append (items, index);
        }

        index_free (index);
    }
}

static gboolean filter_cb (const gchar * filename, void * unused)
{
    return added_table && ! g_hash_table_lookup_extended (added_table, filename, NULL, NULL);
}

static void begin_scan (const gchar * path)
{
    gint list = aud_playlist_by_unique_id (playlist_id);

    if (list < 0)
    {
        create_playlist ();
        list = aud_playlist_by_unique_id (playlist_id);
    }

    gchar * old = aud_get_string ("search-tool", "path");
    aud_set_string ("search-tool", "path", path);

    if (! strcmp (old, path))
        aud_playlist_remove_failed (list);
    else
        aud_playlist_entry_delete (list, 0, aud_playlist_entry_count (list));

    g_free (old);

    create_added_table (list);

    struct index * add = index_new ();
    index_append (add, filename_to_uri (path));
    aud_playlist_entry_insert_filtered (list, -1, add, NULL, filter_cb, NULL, FALSE);
}

static void show_hide_widgets (void)
{
    gint list = aud_playlist_by_unique_id (playlist_id);

    if (list < 0)
    {
        gtk_widget_hide (wait_label);
        gtk_widget_hide (scrolled);
        gtk_widget_show (help_label);
    }
    else if (! gtk_widget_get_visible (scrolled))
    {
        gtk_widget_hide (help_label);

        if (update_source || aud_playlist_add_in_progress (list))
            gtk_widget_show (wait_label);
        else
        {
            gtk_widget_hide (wait_label);
            gtk_widget_show (scrolled);
        }
    }
}

static gint update_timeout (void * unused)
{
    gint list = aud_playlist_by_unique_id (playlist_id);

    if (update_type == UPDATE_DICTS)
    {
        if (list >= 0)
            create_dicts (list);
        else
            destroy_dicts ();
    }

    do_search ();

    if (results_list)
    {
        audgui_list_delete_rows (results_list, 0, audgui_list_row_count (results_list));
        audgui_list_insert_rows (results_list, 0, index_count (items));
    }

    g_source_remove (update_source);
    update_source = 0;
    update_type = 0;

    if (help_label)
        show_hide_widgets ();

    return FALSE;
}

static void schedule_update (gint type)
{
    if (update_source)
        g_source_remove (update_source);

    update_source = g_timeout_add (UPDATE_DELAY, update_timeout, NULL);
    update_type = MAX (update_type, type);

    if (help_label)
        show_hide_widgets ();
}

static void playlist_update_cb (void * data, void * unused)
{
    gint list = aud_playlist_by_unique_id (playlist_id);
    gint at, count;

    if (list < 0 || aud_playlist_updated_range (list, & at, & count) >= PLAYLIST_UPDATE_METADATA)
        schedule_update (UPDATE_DICTS);
}

static gboolean search_init (void)
{
    find_playlist ();

    set_search_phrase ("");
    items = index_new ();

    schedule_update (UPDATE_DICTS);
    hook_associate ("playlist update", playlist_update_cb, NULL);
    return TRUE;
}

static void search_cleanup (void)
{
    hook_dissociate ("playlist update", playlist_update_cb);

    g_strfreev (search_terms);
    search_terms = NULL;

    index_free (items);
    items = NULL;

    destroy_added_table ();
    destroy_dicts ();
}

static void list_get_value (void * user, gint row, gint column, GValue * value)
{
    g_return_if_fail (items && row >= 0 && row < index_count (items));

    Item * item = index_get (items, row);

    switch (column)
    {
    case COLUMN_NAME:
        g_value_set_string (value, item->name);
        break;
    case COLUMN_MATCHES:
        g_value_set_int (value, item->matches->len);
        break;
    }
}

static void list_activate_row (void * user, gint row)
{
    g_return_if_fail (items && row >= 0 && row < index_count (items));
    gint list = aud_playlist_by_unique_id (playlist_id);

    if (list < 0 || update_source)
        return;

    Item * item = index_get (items, row);
    struct index * filenames = index_new ();
    struct index * tuples = index_new ();

    for (gint m = 0; m < item->matches->len; m ++)
    {
        gint entry = g_array_index (item->matches, gint, m);
        index_append (filenames, aud_playlist_entry_get_filename (list, entry));
        index_append (tuples, aud_playlist_entry_get_tuple (list, entry, TRUE));
    }

    list = aud_playlist_get_temporary ();
    aud_playlist_set_active (list);

    if (aud_get_bool (NULL, "clear_playlist"))
        aud_playlist_entry_delete (list, 0, aud_playlist_entry_count (list));
    else
        aud_playlist_queue_delete (list, 0, aud_playlist_queue_count (list));

    aud_playlist_entry_insert_batch (list, -1, filenames, tuples, TRUE);
}

static const AudguiListCallbacks list_callbacks = {
 .get_value = list_get_value,
 .activate_row = list_activate_row};

static void entry_cb (GtkEntry * entry, void * unused)
{
    set_search_phrase (gtk_entry_get_text ((GtkEntry *) entry));
    schedule_update (UPDATE_ITEMS);
}

static void refresh_cb (GtkButton * button, GtkWidget * chooser)
{
    gchar * path = gtk_file_chooser_get_filename ((GtkFileChooser *) chooser);
    begin_scan (path);
    g_free (path);

    schedule_update (UPDATE_DICTS);
    gtk_widget_hide (scrolled);
    show_hide_widgets ();
}

static void * search_get_widget (void)
{
    GtkWidget * vbox = gtk_vbox_new (FALSE, 6);
    gtk_container_set_border_width ((GtkContainer *) vbox, 3);

    GtkWidget * entry = gtk_entry_new ();
#if GTK_CHECK_VERSION (2, 16, 0)
    gtk_entry_set_icon_from_stock ((GtkEntry *) entry, GTK_ENTRY_ICON_PRIMARY, GTK_STOCK_FIND);
#endif
#if GTK_CHECK_VERSION (3, 2, 0)
    gtk_entry_set_placeholder_text ((GtkEntry *) entry, _("Search library"));
#endif
    gtk_box_pack_start ((GtkBox *) vbox, entry, FALSE, FALSE, 0);

    help_label = gtk_label_new (_("To import your music library into "
     "Audacious, choose a folder and then click the \"refresh\" icon."));
    gtk_widget_set_size_request (help_label, 194, -1);
    gtk_label_set_line_wrap ((GtkLabel *) help_label, TRUE);
    g_signal_connect (help_label, "destroy", (GCallback) gtk_widget_destroyed, & help_label);
    gtk_widget_set_no_show_all (help_label, TRUE);
    gtk_box_pack_start ((GtkBox *) vbox, help_label, TRUE, FALSE, 0);

    wait_label = gtk_label_new (_("Please wait ..."));
    g_signal_connect (wait_label, "destroy", (GCallback) gtk_widget_destroyed, & wait_label);
    gtk_widget_set_no_show_all (wait_label, TRUE);
    gtk_box_pack_start ((GtkBox *) vbox, wait_label, TRUE, FALSE, 0);

    scrolled = gtk_scrolled_window_new (NULL, NULL);
    g_signal_connect (scrolled, "destroy", (GCallback) gtk_widget_destroyed, & scrolled);
    gtk_scrolled_window_set_shadow_type ((GtkScrolledWindow *) scrolled, GTK_SHADOW_IN);
    gtk_scrolled_window_set_policy ((GtkScrolledWindow *) scrolled,
     GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_no_show_all (scrolled, TRUE);
    gtk_box_pack_start ((GtkBox *) vbox, scrolled, TRUE, TRUE, 0);

    results_list = audgui_list_new (& list_callbacks, NULL, items ? index_count (items) : 0);
    g_signal_connect (results_list, "destroy", (GCallback) gtk_widget_destroyed, & results_list);
    gtk_tree_view_set_headers_visible ((GtkTreeView *) results_list, FALSE);
    audgui_list_add_column (results_list, NULL, COLUMN_NAME, G_TYPE_STRING, TRUE);
    audgui_list_add_column (results_list, NULL, COLUMN_MATCHES, G_TYPE_INT, FALSE);
    gtk_container_add ((GtkContainer *) scrolled, results_list);

    GtkWidget * hbox = gtk_hbox_new (FALSE, 6);
    gtk_box_pack_end ((GtkBox *) vbox, hbox, FALSE, FALSE, 0);

    GtkWidget * chooser = gtk_file_chooser_button_new (_("Choose Folder"),
     GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER);
    gtk_box_pack_start ((GtkBox *) hbox, chooser, TRUE, TRUE, 0);

    gchar * path = get_path ();
    gtk_file_chooser_set_filename ((GtkFileChooser *) chooser, path);
    g_free (path);

    GtkWidget * button = gtk_button_new ();
    gtk_container_add ((GtkContainer *) button, gtk_image_new_from_stock
     (GTK_STOCK_REFRESH, GTK_ICON_SIZE_BUTTON));
    gtk_button_set_relief ((GtkButton *) button, GTK_RELIEF_NONE);
    gtk_box_pack_start ((GtkBox *) hbox, button, FALSE, FALSE, 0);

    g_signal_connect (entry, "changed", (GCallback) entry_cb, NULL);
    g_signal_connect (button, "clicked", (GCallback) refresh_cb, chooser);

    gtk_widget_show_all (vbox);
    gtk_widget_show (results_list);
    show_hide_widgets ();

    return vbox;
}

AUD_GENERAL_PLUGIN
(
    .name = "Search Tool",
    .init = search_init,
    .cleanup = search_cleanup,
    .get_widget = search_get_widget,
    .enabled_by_default = TRUE
)
