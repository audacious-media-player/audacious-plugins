/*  Audacious
 *  Copyright (C) 2005-2015  Audacious development team.
 *
 *  BMP - Cross-platform multimedia player
 *  Copyright (C) 2003-2004  BMP development team.
 *
 *  Based on XMMS:
 *  Copyright (C) 1998-2003  XMMS development team.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; under version 3 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses>.
 *
 *  The Audacious team does not consider modular code linking to
 *  Audacious or using our public API to be a derived work.
 */

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include <glib.h>
#include <QPainter>

#include <libaudcore/audstrings.h>
#include <libaudcore/runtime.h>

#include "plugin.h"
#include "skins_cfg.h"
#include "skin.h"
#include "util.h"

struct SkinPixmapIdMapping {
    const char *name;
    const char *alt_name;
};

static const SkinPixmapIdMapping skin_pixmap_id_map[] = {
    {"main"},
    {"cbuttons"},
    {"titlebar"},
    {"shufrep"},
    {"text"},
    {"volume"},
    {"balance", "volume"},
    {"monoster"},
    {"playpaus"},
    {"nums_ex", "numbers"},
    {"posbar"},
    {"pledit"},
    {"eqmain"},
    {"eq_ex"}
};

static_assert (aud::n_elems (skin_pixmap_id_map) == SKIN_PIXMAP_COUNT,
 "update skin_pixmap_id_map!");

static const uint32_t default_vis_colors[24] = {
    qRgb (9, 34, 53),
    qRgb (10, 18, 26),
    qRgb (0, 54, 108),
    qRgb (0, 58, 116),
    qRgb (0, 62, 124),
    qRgb (0, 66, 132),
    qRgb (0, 70, 140),
    qRgb (0, 74, 148),
    qRgb (0, 78, 156),
    qRgb (0, 82, 164),
    qRgb (0, 86, 172),
    qRgb (0, 92, 184),
    qRgb (0, 98, 196),
    qRgb (0, 104, 208),
    qRgb (0, 110, 220),
    qRgb (0, 116, 232),
    qRgb (0, 122, 244),
    qRgb (0, 128, 255),
    qRgb (0, 128, 255),
    qRgb (0, 104, 208),
    qRgb (0, 80, 160),
    qRgb (0, 56, 112),
    qRgb (0, 32, 64),
    qRgb (200, 200, 200)
};

Skin skin;

static bool skin_load_pixmap_id (SkinPixmapId id, const char * path)
{
    StringBuf filename = skin_pixmap_locate (path, skin_pixmap_id_map[id].name,
     skin_pixmap_id_map[id].alt_name);

    if (! filename)
    {
        AUDERR ("Skin does not contain a \"%s\" pixmap.\n", skin_pixmap_id_map[id].name);
        return false;
    }

    QImage & image = skin.pixmaps[id];
    image.load ((const char *) filename);

    if (! image.isNull () && image.format () != QImage::Format_RGB32)
        image = image.convertToFormat (QImage::Format_RGB32);

    if (image.isNull ())
    {
        AUDERR ("Error loading pixmap: %s\n", (const char *) filename);
        return false;
    }

    return true;
}

static int color_diff (uint32_t a, uint32_t b)
{
    return abs (qRed (a) - qRed (b)) + abs (qGreen (a) - qGreen (b)) + abs (qBlue (a) - qBlue (b));
}

static void skin_get_textcolors (const QImage & image)
{
    /*
     * Try to extract reasonable background and foreground colors
     * from the font pixmap
     */

    /* Get a pixel from the middle of the space character */
    skin.colors[SKIN_TEXTBG] = image.pixel (152, 3);

    int max_d = -1;
    for (int y = 0; y < 6; y ++)
    {
        for (int x = 1; x < 150; x ++)
        {
            uint32_t c = image.pixel (x, y);
            int d = color_diff (skin.colors[SKIN_TEXTBG], c);
            if (d > max_d)
            {
                skin.colors[SKIN_TEXTFG] = c;
                max_d = d;
            }
        }
    }
}

static void skin_get_eq_spline_colors (const QImage & image)
{
    if (image.height () < 313)
        return;

    for (int i = 0; i < 19; i ++)
        skin.eq_spline_colors[i] = image.pixel (115, i + 294);
}

static void skin_load_viscolor (const char * path)
{
    memcpy (skin.vis_colors, default_vis_colors, sizeof skin.vis_colors);

    VFSFile file = open_local_file_nocase (path, "viscolor.txt");
    if (! file)
        return;

    Index<char> buffer = file.read_all ();
    buffer.append (0);  /* null-terminated */

    char * string = buffer.begin ();

    for (int line = 0; string && line < 24; line ++)
    {
        char * next = text_parse_line (string);
        Index<int> array = string_to_int_array (string);

        if (array.len () >= 3)
            skin.vis_colors[line] = qRgb (array[0], array[1], array[2]);

        string = next;
    }
}

static void skin_numbers_generate_dash (QImage & image)
{
    int w = image.width ();
    if (w < 99 || w >= 108)
        return;

    int h = image.height ();
    QImage temp (108, h, QImage::Format_RGB32);

    {
        QPainter p (& temp);
        p.drawImage (0, 0, image, 0, 0, 99, h);
        p.drawImage (99, 0, image, 90, 0, 9, h);
        p.drawImage (101, 6, image, 20, 6, 5, 1);
    }

    image = std::move (temp);
}

static bool skin_load_pixmaps (const char * path)
{
    AUDDBG ("Loading pixmaps in %s\n", path);

    /* eq_ex.bmp was added after Winamp 2.0 so some skins do not include it */
    for (int i = 0; i < SKIN_PIXMAP_COUNT; i ++)
        if (! skin_load_pixmap_id ((SkinPixmapId) i, path) && i != SKIN_EQ_EX)
            return false;

    skin_get_textcolors (skin.pixmaps[SKIN_TEXT]);
    skin_get_eq_spline_colors (skin.pixmaps[SKIN_EQMAIN]);
    skin_numbers_generate_dash (skin.pixmaps[SKIN_NUMBERS]);

    return true;
}

static bool skin_load_data (const char * path)
{
    AUDDBG ("Attempt to load skin \"%s\"\n", path);

    if (! g_file_test (path, G_FILE_TEST_EXISTS))
        return false;

    StringBuf archive_path;
    if (file_is_archive (path))
    {
        AUDDBG ("Attempt to load archive\n");
        archive_path.steal (archive_decompress (path));

        if (! archive_path)
        {
            AUDDBG ("Unable to extract skin archive (%s)\n", path);
            return false;
        }

        path = archive_path;
    }

    bool success = skin_load_pixmaps (path);

    if (success)
    {
        skin_load_hints (path);
        skin_load_pl_colors (path);
        skin_load_viscolor (path);
        skin_load_masks (path);
    }
    else
        AUDDBG ("Skin loading failed\n");

    if (archive_path)
        del_directory (archive_path);

    return success;
}

bool skin_load (const char * path)
{
    /* save current skin data */
    Skin old_skin (std::move (skin));

    /* reset to defaults */
    skin = Skin ();

    if (skin_load_data (path))
    {
        aud_set_str ("skins", "skin", path);
        return true;
    }

    AUDWARN ("Unable to load skin (%s).\n", (const char *) path);

    /* restore old skin data */
    skin = std::move (old_skin);
    return false;
}

void skin_install_skin (const char * path)
{
    GError * err = nullptr;
    char * data;
    size_t len;

    if (! g_file_get_contents (path, & data, & len, & err))
    {
        AUDERR ("Failed to read %s: %s\n", path, err->message);
        g_error_free (err);
        return;
    }

    const char * user_skin_dir = skins_get_user_skin_dir ();
    make_directory (user_skin_dir);

    StringBuf base = filename_get_base (path);
    StringBuf target = filename_build ({user_skin_dir, base});

    if (g_file_set_contents (target, data, len, & err))
        aud_set_str ("skins", "skin", target);
    else
    {
        AUDERR ("Failed to write %s: %s\n", path, err->message);
        g_error_free (err);
    }

    g_free (data);
}

void skin_draw_pixbuf (QPainter & p, SkinPixmapId id, int xsrc, int ysrc, int
 xdest, int ydest, int width, int height)
{
    if (! skin.pixmaps[id].isNull ())
        p.drawImage (xdest, ydest, skin.pixmaps[id], xsrc, ysrc, width, height);
}

static void skin_draw_playlistwin_frame_top (QPainter & cr, int width, bool focus)
{
    /* The title bar skin consists of 2 sets of 4 images, 1 set
     * for focused state and the other for unfocused.
     * The 4 images are:
     *
     * a. right corner (25,20)
     * b. left corner  (25,20)
     * c. tiler        (25,20)
     * d. title        (100,20)
     *
     * min allowed width = 100+25+25 = 150
     */

    /* get y offset of the pixmap set to use */
    int y = focus ? 0 : 21;

    /* left corner */
    skin_draw_pixbuf (cr, SKIN_PLEDIT, 0, y, 0, 0, 25, 20);

    /* titlebar title */
    skin_draw_pixbuf (cr, SKIN_PLEDIT, 26, y, (width - 100) / 2, 0, 100, 20);

    /* titlebar right corner  */
    skin_draw_pixbuf (cr, SKIN_PLEDIT, 153, y, width - 25, 0, 25, 20);

    /* tile draw the remaining frame */

    /* compute tile count */
    int c = (width - (100 + 25 + 25)) / 25;

    for (int i = 0; i < c / 2; i ++)
    {
        /* left of title */
        skin_draw_pixbuf (cr, SKIN_PLEDIT, 127, y, 25 + i * 25, 0, 25, 20);

        /* right of title */
        skin_draw_pixbuf (cr, SKIN_PLEDIT, 127, y, (width + 100) / 2 + i * 25, 0, 25, 20);
    }

    if (c & 1)
    {
        /* Odd tile count, so one remaining to draw. Here we split
         * it into two and draw half on either side of the title */
        skin_draw_pixbuf (cr, SKIN_PLEDIT, 127, y, ((c / 2) * 25) + 25, 0, 12, 20);
        skin_draw_pixbuf (cr, SKIN_PLEDIT, 127, y, (width / 2) + ((c / 2) * 25) + 50, 0, 13, 20);
    }
}

static void skin_draw_playlistwin_frame_bottom (QPainter & cr, int width, int height)
{
    /* The bottom frame skin consists of 1 set of 4 images.
     * The 4 images are:
     *
     * a. left corner with menu buttons (125,38)
     * b. visualization window (75,38)
     * c. right corner with play buttons (150,38)
     * d. frame tile (25,38)
     *
     * (min allowed width = 125+150+25=300
     */

    /* bottom left corner (menu buttons) */
    skin_draw_pixbuf (cr, SKIN_PLEDIT, 0, 72, 0, height - 38, 125, 38);

    int c = (width - 275) / 25;

    /* draw visualization window, if width allows */
    if (c >= 3)
    {
        c -= 3;
        skin_draw_pixbuf (cr, SKIN_PLEDIT, 205, 0, width - (150 + 75), height - 38, 75, 38);
    }

    /* Bottom right corner (playbuttons etc) */
    skin_draw_pixbuf (cr, SKIN_PLEDIT, 126, 72, width - 150, height - 38, 150, 38);

    /* Tile draw the remaining undrawn portions */
    for (int i = 0; i < c; i ++)
        skin_draw_pixbuf (cr, SKIN_PLEDIT, 179, 0, 125 + i * 25, height - 38, 25, 38);
}

static void skin_draw_playlistwin_frame_sides (QPainter & cr, int width, int height)
{
    /* The side frames consist of 2 tile images. 1 for the left, 1 for
     * the right.
     * a. left  (12,29)
     * b. right (19,29)
     */

    /* frame sides */
    for (int i = 0; i < (height - (20 + 38)) / 29; i ++)
    {
        /* left */
        skin_draw_pixbuf (cr, SKIN_PLEDIT, 0, 42, 0, 20 + i * 29, 12, 29);

        /* right */
        skin_draw_pixbuf (cr, SKIN_PLEDIT, 32, 42, width - 19, 20 + i * 29, 19, 29);
    }
}

void skin_draw_playlistwin_frame (QPainter & cr, int width, int height, bool focus)
{
    skin_draw_playlistwin_frame_top (cr, width, focus);
    skin_draw_playlistwin_frame_bottom (cr, width, height);
    skin_draw_playlistwin_frame_sides (cr, width, height);
}

void skin_draw_playlistwin_shaded (QPainter & cr, int width, bool focus)
{
    /* The shade mode titlebar skin consists of 4 images:
     * a) left corner               offset (72,42) size (25,14)
     * b) right corner, focused     offset (99,57) size (50,14)
     * c) right corner, unfocused   offset (99,42) size (50,14)
     * d) bar tile                  offset (72,57) size (25,14)
     */

    /* left corner */
    skin_draw_pixbuf (cr, SKIN_PLEDIT, 72, 42, 0, 0, 25, 14);

    /* bar tile */
    for (int i = 0; i < (width - 75) / 25; i ++)
        skin_draw_pixbuf (cr, SKIN_PLEDIT, 72, 57, (i * 25) + 25, 0, 25, 14);

    /* right corner */
    skin_draw_pixbuf (cr, SKIN_PLEDIT, 99, focus ? 42 : 57, width - 50, 0, 50, 14);
}

void skin_draw_mainwin_titlebar (QPainter & cr, bool shaded, bool focus)
{
    /* The titlebar skin consists of 2 sets of 2 images, one for for
     * shaded and the other for unshaded mode, giving a total of 4.
     * The images are exactly 275x14 pixels, aligned and arranged
     * vertically on each other in the pixmap in the following order:
     *
     * a) unshaded, focused      offset (27, 0)
     * b) unshaded, unfocused    offset (27, 15)
     * c) shaded, focused        offset (27, 29)
     * d) shaded, unfocused      offset (27, 42)
     */

    int y_offset = shaded ? (focus ? 29 : 42) : (focus ? 0 : 15);

    skin_draw_pixbuf (cr, SKIN_TITLEBAR, 27, y_offset, 0, 0,
     skin.hints.mainwin_width, 14);
}

/* parse a subset of Pango font descriptions */
QFont * qfont_from_string (const char * name)
{
    auto family = str_copy (name);
    int size = 0;
    QFont::Weight weight = QFont::Normal;
    QFont::Style style = QFont::StyleNormal;
    QFont::Stretch stretch = QFont::Unstretched;

    while (1)
    {
        /* check for attributes */
        bool attr_found = false;
        const char * space = strrchr (family, ' ');

        if (space)
        {
            const char * attr = space + 1;
            int num = str_to_int (attr);

            attr_found = true;

            if (num > 0)
                size = num;
            else if (! strcmp (attr, "Light"))
                weight = QFont::Light;
            else if (! strcmp (attr, "Bold"))
                weight = QFont::Bold;
            else if (! strcmp (attr, "Oblique"))
                style = QFont::StyleOblique;
            else if (! strcmp (attr, "Italic"))
                style = QFont::StyleItalic;
            else if (! strcmp (attr, "Condensed"))
                stretch = QFont::Condensed;
            else if (! strcmp (attr, "Expanded"))
                stretch = QFont::Expanded;
            else
                attr_found = false;
        }

        if (! attr_found)
        {
            auto font = new QFont ((const char *) family);

            /* check for a recognized font family */
            if (! space || font->exactMatch ())
            {
                if (size > 0)
                    font->setPointSize (size);
                if (weight != QFont::Normal)
                    font->setWeight (weight);
                if (style != QFont::StyleNormal)
                    font->setStyle (style);
                if (stretch != QFont::Unstretched)
                    font->setStretch (stretch);

                return font;
            }

            delete font;
        }

        family.resize (space - family);
    }
}
