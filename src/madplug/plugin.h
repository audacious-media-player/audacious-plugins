/*
 * mad plugin for audacious
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef AUD_MAD_H
#define AUD_MAD_H

//#define DEBUG 1
#undef DEBUG

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "MADPlug"

#include "config.h"

#undef PACKAGE
#define PACKAGE "audacious-plugins"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <audacious/plugin.h>
#include <audacious/titlestring.h>
#include <audacious/strings.h>
#include <audacious/i18n.h>
#include <id3tag.h>
#include <mad.h>

#include "xing.h"

struct mad_info_t
{
    InputPlayback *playback;
    int seek;

    /* state */
    guint current_frame;/**< current mp3 frame */
    mad_timer_t pos;    /**< current play position */

    /* song info */
    guint vbr;      /**< bool: is vbr? */
    guint bitrate;  /**< avg. bitrate */
    guint freq;     /**< sample freq. */
    guint mpeg_layer;   /**< mpeg layer */
    guint mode;     /**< mpeg stereo mode */
    guint channels;
    gint frames;    /**< total mp3 frames or -1 */
    gint fmt;       /**< sample format */
    gint size;      /**< file size in bytes or -1 */
    gchar *title;   /**< title for xmms */
    mad_timer_t duration;
            /**< total play time */
    struct id3_tag *tag;
    struct id3_file *id3file;
    struct xing xing;
    TitleInput *tuple;          /* audacious tuple data */

    /* replay parameters */
    gboolean has_replaygain;
    double replaygain_album_scale;  // -1 if not set
    double replaygain_track_scale;
    gchar *replaygain_album_str;
    gchar *replaygain_track_str;
    double replaygain_album_peak;   // -1 if not set
    double replaygain_track_peak;
    gchar *replaygain_album_peak_str;
    gchar *replaygain_track_peak_str;
    double mp3gain_undo;        // -1 if not set
    double mp3gain_minmax;
    gchar *mp3gain_undo_str;
    gchar *mp3gain_minmax_str;

    /* data access */
    gchar *url;
    gchar *filename;
    VFSFile *infile;
    gint offset;

    gint remote;
    struct streamdata_t *sdata;
                  /**< stream data for remote connections */
};

struct audmad_config_t
{
    gint http_buffer_size;
    gboolean fast_play_time_calc;
    gboolean use_xing;
    gboolean dither;
    gboolean hard_limit;
    gchar *pregain_db;          // gain applied to samples at decoding stage.
    gdouble pregain_scale;      // pow(10, pregain/20)
    struct
    {
        gboolean enable;
        gboolean track_mode;
        gchar *default_db;      // gain used if no RG.
        gdouble default_scale;
    } replaygain;
    gboolean title_override;
    gchar *id3_format;
};

void audmad_config_compute(struct audmad_config_t *config);
// compute scale values from "_db" strings

extern gpointer decode_loop(gpointer arg);
extern void audmad_error(gchar * fmt, ...);
extern void audmad_configure();
extern InputPlugin *mad_plugin;
extern struct audmad_config_t audmad_config;

#endif                          /* !AUD_MAD_H */
