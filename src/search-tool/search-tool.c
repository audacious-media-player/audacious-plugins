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
#define SEARCH_DELAY 300

enum {GENRE, ARTIST, ALBUM, TITLE, FIELDS};
enum {UPDATE_ITEMS = 1, UPDATE_DICTS};
enum {COLUMN_NAME, COLUMN_MATCHES};

static const gchar * const field_names[] = {N_("Genre"), N_("Artist"),
 N_("Album"), N_("Title")};

typedef struct {
    gint field;
    gchar * name, * folded;
    GArray * matches;
} Item;

static gint playlist_id;
static gchar * * search_terms;

static GHashTable * added_table;
static GHashTable * dicts[FIELDS];
static struct index * items;

static gboolean adding, dicts_valid;
static gint search_source;

static GtkWidget * help_label, * wait_label, * scrolled, * results_list;

static Item * item_new (gint field, gchar * name)
{
    Item * item = g_slice_new (Item);
    item->field = field;
    item->name = name;
    item->folded = g_utf8_casefold (name, -1);
    item->matches = g_array_new (FALSE, FALSE, sizeof (gint));
    return item;
}

static void item_free (Item * item)
{
    str_unref (item->name);
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

        str_unref (title);
    }
}

static gint create_playlist (void)
{
    aud_playlist_insert (0);
    aud_playlist_set_title (0, _("Library"));
    playlist_id = aud_playlist_get_unique_id (0);
    return 0;
}

static gint get_playlist (gboolean require_added, gboolean require_scanned)
{
    if (playlist_id < 0)
        return -1;

    gint list = aud_playlist_by_unique_id (playlist_id);

    if (list < 0)
    {
        playlist_id = -1;
        return -1;
    }

    if (require_added && aud_playlist_add_in_progress (list))
        return -1;
    if (require_scanned && aud_playlist_scan_in_progress (list))
        return -1;

    return list;
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

static void destroy_dicts (void)
{
    if (items)
        index_delete (items, 0, index_count (items));

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
        dicts[f] = g_hash_table_new_full (g_str_hash, g_str_equal, str_unref,
         (GDestroyNotify) item_free);

    gint entries = aud_playlist_entry_count (list);

    for (gint e = 0; e < entries; e ++)
    {
        gchar * fields[FIELDS];

        Tuple * tuple = aud_playlist_entry_get_tuple (list, e, TRUE);
        fields[GENRE] = tuple ? tuple_get_str (tuple, FIELD_GENRE, NULL) : NULL;
        if (tuple)
            tuple_unref (tuple);

        aud_playlist_entry_describe (list, e, & fields[TITLE], & fields[ARTIST],
         & fields[ALBUM], TRUE);

        for (gint f = 0; f < FIELDS; f ++)
        {
            if (! fields[f])
                continue;

            Item * item = g_hash_table_lookup (dicts[f], fields[f]);

            if (item)
                str_unref (fields[f]);
            else
            {
                item = item_new (f, fields[f]);
                g_hash_table_insert (dicts[f], str_ref (item->name), item);
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

static gint item_compare (const void * _a, const void * _b)
{
    const Item * a = _a, * b = _b;

    if (a->field != b->field)
        return (a->field > b->field) ? 1 : -1;

    return string_compare (a->name, b->name);
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

static void begin_add (const gchar * path)
{
    gint list = get_playlist (FALSE, FALSE);

    if (list < 0)
        list = create_playlist ();

    aud_set_string ("search-tool", "path", path);

    gchar * uri = filename_to_uri (path);
    g_return_if_fail (uri);
    gchar * prefix = g_str_has_suffix (uri, "/") ? g_strdup (uri) : g_strconcat (uri, "/", NULL);

    destroy_added_table ();
    added_table = g_hash_table_new_full (g_str_hash, g_str_equal, str_unref, NULL);

    gint entries = aud_playlist_entry_count (list);

    for (gint entry = 0; entry < entries; entry ++)
    {
        gchar * filename = aud_playlist_entry_get_filename (list, entry);

        if (g_str_has_prefix (filename, prefix) && ! g_hash_table_lookup_extended
         (added_table, filename, NULL, NULL))
        {
            aud_playlist_entry_set_selected (list, entry, FALSE);
            g_hash_table_insert (added_table, filename, NULL);
        }
        else
        {
            aud_playlist_entry_set_selected (list, entry, TRUE);
            str_unref (filename);
        }
    }

    aud_playlist_delete_selected (list);
    aud_playlist_remove_failed (list);

    struct index * add = index_new ();
    index_append (add, str_get (uri));
    aud_playlist_entry_insert_filtered (list, -1, add, NULL, filter_cb, NULL, FALSE);

    g_free (uri);
    g_free (prefix);
    adding = TRUE;
}

static void show_hide_widgets (void)
{
    if (! help_label || ! wait_label || ! scrolled)
        return;

    if (playlist_id < 0)
    {
        gtk_widget_hide (wait_label);
        gtk_widget_hide (scrolled);
        gtk_widget_show (help_label);
    }
    else
    {
        gtk_widget_hide (help_label);

        if (dicts_valid)
        {
            gtk_widget_hide (wait_label);
            gtk_widget_show (scrolled);
        }
        else
        {
            gtk_widget_hide (scrolled);
            gtk_widget_show (wait_label);
        }
    }
}

static gint search_timeout (void * unused)
{
    do_search ();

    if (results_list)
    {
        audgui_list_delete_rows (results_list, 0, audgui_list_row_count (results_list));
        audgui_list_insert_rows (results_list, 0, index_count (items));
    }

    if (search_source)
    {
        g_source_remove (search_source);
        search_source = 0;
    }

    return FALSE;
}

static void schedule_search (void)
{
    if (search_source)
        g_source_remove (search_source);

    search_source = g_timeout_add (SEARCH_DELAY, search_timeout, NULL);
}

static void update_dicts (void)
{
    gint list = get_playlist (TRUE, TRUE);

    if (list >= 0)
    {
        create_dicts (list);
        dicts_valid = TRUE;
        schedule_search ();
    }
    else
    {
        destroy_dicts ();
        dicts_valid = FALSE;
    }

    if (results_list)
        audgui_list_delete_rows (results_list, 0, audgui_list_row_count (results_list));

    show_hide_widgets ();
}

static void add_complete_cb (void * unused, void * unused2)
{
    if (adding)
    {
        gint list = get_playlist (TRUE, FALSE);

        if (list >= 0 && ! aud_playlist_add_in_progress (list))
        {
            adding = FALSE;
            destroy_added_table ();
            aud_playlist_sort_by_scheme (list, PLAYLIST_SORT_PATH);
        }
    }

    if (! dicts_valid && ! aud_playlist_update_pending ())
        update_dicts ();
}

static void scan_complete_cb (void * unused, void * unused2)
{
    if (! dicts_valid && ! aud_playlist_update_pending ())
        update_dicts ();
}

static void playlist_update_cb (void * data, void * unused)
{
    if (! dicts_valid)
        update_dicts ();
    else
    {
        gint list = get_playlist (TRUE, TRUE);
        gint at, count;

        if (list < 0 || aud_playlist_updated_range (list, & at, & count) >=
         PLAYLIST_UPDATE_METADATA)
            update_dicts ();
    }
}

static gboolean search_init (void)
{
    find_playlist ();

    set_search_phrase ("");
    items = index_new ();

    update_dicts ();

    hook_associate ("playlist add complete", add_complete_cb, NULL);
    hook_associate ("playlist scan complete", scan_complete_cb, NULL);
    hook_associate ("playlist update", playlist_update_cb, NULL);

    return TRUE;
}

static void search_cleanup (void)
{
    hook_dissociate ("playlist add complete", add_complete_cb);
    hook_dissociate ("playlist scan complete", scan_complete_cb);
    hook_dissociate ("playlist update", playlist_update_cb);

    if (search_source)
    {
        g_source_remove (search_source);
        search_source = 0;
    }

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
        g_value_take_string (value, g_strdup_printf ("%s: %s",
         _(field_names[item->field]), item->name));
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
    schedule_search ();
}

static void refresh_cb (GtkButton * button, GtkWidget * chooser)
{
    gchar * path = gtk_file_chooser_get_filename ((GtkFileChooser *) chooser);
    begin_add (path);
    g_free (path);

    update_dicts ();
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
    audgui_list_add_column (results_list, NULL, COLUMN_NAME, G_TYPE_STRING, -1);
    audgui_list_add_column (results_list, NULL, COLUMN_MATCHES, G_TYPE_INT, 2);
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
    .get_widget = search_get_widget
)
