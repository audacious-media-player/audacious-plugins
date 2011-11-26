#include <glib.h>
#include <string.h>

#include <audacious/i18n.h>
#include <audacious/misc.h>
#include <audacious/playlist.h>
#include <audacious/plugin.h>
#include <libaudcore/audstrings.h>

#include "config.h"

#define MAX_RESULTS 12

enum {GENRE, ARTIST, ALBUM, TITLE, FIELDS};

static const gchar * const field_names[] = {N_("Genre"), N_("Artist"),
 N_("Album"), N_("Title")};

typedef struct {
    gchar * name, * folded;
    GArray * matches;
} Item;

static gint playlist_id;
static GHashTable * added_table;
static GHashTable * dicts[FIELDS];
static struct index * items;

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

    for (gint p = 0; p < aud_playlist_count (); p ++)
    {
        gchar * title = aud_playlist_get_title (p);

        if (! strcmp (title, _("Library")))
        {
            playlist_id = aud_playlist_get_unique_id (p);
            break;
        }

        g_free (title);
    }
}

static void create_playlist (void)
{
    aud_playlist_insert (0);
    aud_playlist_set_title (0, _("Library"));
    playlist_id = aud_playlist_get_unique_id (0);
}

static gint get_playlist (void)
{
    if (aud_playlist_by_unique_id (playlist_id) < 0)
        create_playlist ();

    return aud_playlist_by_unique_id (playlist_id);
}

static gchar * get_path (void)
{
    gchar * path = aud_get_string ("search-tool", "path");
    if (g_file_test (path, G_FILE_TEST_EXISTS))
        goto FOUND;

    g_free (path);
    path = g_build_filename (g_get_home_dir (), "Music", NULL);
    if (g_file_test (path, G_FILE_TEST_EXISTS))
        goto FOUND;

    g_free (path);
    path = g_strdup (g_get_home_dir ());

FOUND:
    aud_set_string ("search-tool", "path", path);
    return path;
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

        Tuple * tuple = aud_playlist_entry_get_tuple (list, e, FALSE);
        const gchar * genre = tuple ? tuple_get_string (tuple, FIELD_GENRE, NULL) : NULL;
        fields[GENRE] = genre ? g_strdup (genre) : NULL;
        if (tuple)
            tuple_free (tuple)

        aud_playlist_entry_describe (list, e, & fields[TITLE], & fields[ARTIST],
         & fields[ALBUM], FALSE);

        for (gint f = 0; f < FIELDS; f ++)
        {
            if (! fields[f])
                continue;

            Item * item = g_hash_table_lookup (dicts[f], fields[f]);

            if (! item)
            {
                item = item_new (f, fields[f]);
                g_hash_table_insert (dicts[f], fields[f], item);
            }

            g_array_append_val (item->matches, e);
        }
    }
}

static void destroy_items (void)
{
    if (! items)
        return;

    index_free (items);
    items = NULL;
}

static gchar * * search_terms;

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

static void create_items (const gchar * phrase)
{
    destroy_items ();
    items = index_new ();

    gchar * folded = g_utf8_casefold (phrase, -1);
    search_terms = g_strsplit (folded, " ", -1);
    g_free (folded);

    for (gint f = 0; f < FIELDS; f ++)
    {
        struct index * index = index_new ();
        g_hash_table_foreach (dicts[f], search_cb, index);

        if (index_count (index) <= MAX_RESULTS)
        {
            index_sort (index, item_compare);
            index_merge_append (items, index);
        }

        index_free (index);
    }

    g_strfreev (search_terms);
    search_terms = NULL;
}

static gboolean filter_cb (const gchar * filename, void * unused)
{
    return added_table && ! g_hash_table_lookup_extended (added_table, filename, NULL, NULL);
}

static void begin_scan (void)
{
    gint list = get_playlist ();
    gchar * path = get_path ();

    aud_playlist_remove_failed (list);

    create_added_table (list);

    struct index * add = index_new ();
    index_append (add, filename_to_uri (path));
    aud_playlist_entry_insert_filtered (list, -1, add, NULL, filter_cb, NULL, FALSE);

    g_free (path);
}

static gboolean search_init (void)
{
    find_playlist ();

    /* temporary ... */
    begin_scan ();
    create_dicts (get_playlist ());
    create_items ("jo");

    for (gint i = 0; i < index_count (items); i ++)
    {
        Item * item = index_get (items, i);
        printf ("%s (%d)\n", item->name, item->matches->len);
    }

    return TRUE;
}

void search_cleanup (void)
{
    destroy_added_table ();
    destroy_dicts ();
    destroy_items ();
}

AUD_GENERAL_PLUGIN
(
    .name = "Search Tool",
    .init = search_init,
    .cleanup = search_cleanup,
)
