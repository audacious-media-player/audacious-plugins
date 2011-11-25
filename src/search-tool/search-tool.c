#include "config.h"

#include <glib.h>

#include <audacious/i18n.h>
#include <audacious/misc.h>
#include <audacious/playlist.h>
#include <audacious/plugin.h>
#include <libaudcore/audstrings.h>

enum {GENRE, ARTIST, ALBUM, TITLE, FIELDS};

static const gchar * const field_names[] = {N_("Genres"), N_("Artists"),
 N_("Albums"), N_("Titles")};

static gint playlist_id;
static GHashTable * added_table;
static GHashTable * dicts[FIELDS];

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
         (GDestroyNotify) g_array_unref);

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

            GArray * matches = g_hash_table_lookup (dicts[f], fields[f]);

            if (! matches)
            {
                matches = g_array_new (FALSE, FALSE, sizeof (gint));
                g_hash_table_insert (dicts[f], fields[f], matches);
            }

            g_array_append_val (matches, e);
        }
    }
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

void print_cb (void * key, void * value, void * user)
{
    printf ("%s (%d)\n", (const gchar *) key, ((GArray *) value)->len);
}

static gboolean search_init (void)
{
    find_playlist ();

    /* temporary ... */
    begin_scan ();
    create_dicts (get_playlist ());

    for (gint f = 0; f < FIELDS; f ++)
    {
        printf ("\n%d %s:\n", g_hash_table_size (dicts[f]), field_names[f]);
        g_hash_table_foreach (dicts[f], print_cb, NULL);
    }

    return TRUE;
}

void search_cleanup (void)
{
    destroy_added_table ();
    destroy_dicts ();
}

AUD_GENERAL_PLUGIN
(
    .name = "Search Tool",
    .init = search_init,
    .cleanup = search_cleanup,
)
