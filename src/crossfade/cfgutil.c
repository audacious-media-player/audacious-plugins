/* 
 *  XMMS Crossfade Plugin
 *  Copyright (C) 2000-2007  Peter Eisenlohr <peter@eisenlohr.org>
 *
 *  based on the original OSS Output Plugin
 *  Copyright (C) 1998-2000  Peter Alm, Mikael Alm, Olle Hallnas, Thomas Nilsson and 4Front Technologies
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *  
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307,
 *  USA.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#undef PRESET_SUPPORT

#include "crossfade.h"
#include "configure.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#  define ConfigFile ConfigDb
#  define xmms_cfg_read_int           aud_cfg_db_get_int
#  define xmms_cfg_read_string        aud_cfg_db_get_string
#  define xmms_cfg_read_boolean       aud_cfg_db_get_bool
#  define xmms_cfg_write_int          aud_cfg_db_set_int
#  define xmms_cfg_write_string       aud_cfg_db_set_string
#  define xmms_cfg_write_boolean      aud_cfg_db_set_bool
#  define xmms_cfg_remove_key         aud_cfg_db_unset_key
#  define xmms_cfg_open_default_file  aud_cfg_db_open
#  define xmms_cfg_write_default_file aud_cfg_db_close
void xmms_cfg_dummy(ConfigDb *dummy) { }
#  define xmms_cfg_free xmms_cfg_dummy

/* init with DEFAULT_CFG to make sure all string pointers are set to NULL */
config_t _xfg = CONFIG_DEFAULT;  /* also used in configure.c */
static config_t *xfg = &_xfg;

/*****************************************************************************/

static void
update_plugin_config(gchar **config_string, gchar *name, plugin_config_t *pc, gboolean save);

static void
g_free_f(gpointer data, gpointer user_data)
{
    g_free(data);
}
        
/*****************************************************************************/

static gchar *strip(gchar *s)
{
    gchar *p;
    if (!s)
        return NULL;

    for (; *s == ' '; s++);
    if (!*s)
        return s;

    for (p = s + strlen(s) - 1; *p == ' '; p--);
    *++p = 0;
    return s;
}

static void
update_plugin_config(gchar **config_string, gchar *name, plugin_config_t *pc, gboolean save)
{
    plugin_config_t default_pc = DEFAULT_OP_CONFIG;

    gchar *buffer = NULL;
    gchar out[1024];

    gboolean plugin_found = FALSE;
    gchar *plugin, *next_plugin;
    gchar *args;

    if (pc && !save)
        *pc = default_pc;

    if (!config_string || !*config_string || !name || !pc)
    {
        AUDDBG("[crossfade] update_plugin_config: missing arg!\n");
        return;
    }

    buffer = g_strdup(*config_string);
    out[0] = 0;

    for (plugin = buffer; plugin; plugin = next_plugin)
    {
        if ((next_plugin = strchr(plugin, ';')))
            *next_plugin++ = 0;

        if ((args = strchr(plugin, '=')))
            *args++ = 0;

        plugin = strip(plugin);
        if (!*plugin || !args || !*args)
            continue;

        if (save)
        {
            if (strcmp(plugin, name) == 0)
                continue;
                
            if (*out)
                strcat(out, "; ");
                
            strcat(out, plugin);
            strcat(out, "=");
            strcat(out, args);
            continue;
        }
        else if (strcmp(plugin, name))
            continue;

        args = strip(args);
        sscanf(args, "%d,%d,%d,%d", &pc->throttle_enable, &pc->max_write_enable, &pc->max_write_len, &pc->force_reopen);
        pc->max_write_len &= -4;
        plugin_found = TRUE;
    }

    if (save)
    {
        /* only save if settings differ from defaults */
        if ((pc->throttle_enable  != default_pc.throttle_enable)
         || (pc->max_write_enable != default_pc.max_write_enable)
         || (pc->max_write_len    != default_pc.max_write_len)
         || (pc->force_reopen     != default_pc.force_reopen))
        {
            if (*out)
                strcat(out, "; ");

            sprintf(out + strlen(out), "%s=%d,%d,%d,%d", name,
                pc->throttle_enable ? 1 : 0, pc->max_write_enable ? 1 : 0, pc->max_write_len, pc->force_reopen);
        }
        if (*config_string)
            g_free(*config_string);

        *config_string = g_strdup(out);
    }

    g_free(buffer);
}

/*****************************************************************************/

#ifdef PRESET_SUPPORT
static void
scan_presets(gchar *filename)
{
    struct stat stats;
    FILE *fh;
    gchar *data, **lines, *tmp, *name;
    int i;

    if (lstat(filename, &stats))
    {
        AUDDBG("[crossfade] scan_presets: \"%s\":\n", filename);
        PERROR("[crossfade] scan_presets: lstat");
        return;
    }
    if (stats.st_size <= 0)
        return;

    if (!(data = g_malloc(stats.st_size + 1)))
    {
        AUDDBG("[crossfade] scan_presets: g_malloc(%ld) failed!\n", stats.st_size);
        return;
    }

    if (!(fh = fopen(filename, "r")))
    {
        PERROR("[crossfade] scan_presets: fopen");
        g_free(data);
        return;
    }

    if (fread(data, stats.st_size, 1, fh) != 1)
    {
        AUDDBG("[crossfade] scan_presets: fread() failed!\n");
        g_free(data);
        fclose(fh);
        return;
    }
    fclose(fh);
    data[stats.st_size] = 0;

    lines = g_strsplit(data, "\n", 0);
    g_free(data);

    if (!lines)
    {
        AUDDBG("[crossfade] scan_presets: g_strsplit() failed!\n");
        return;
    }

    g_list_foreach(config->presets, g_free_f, NULL);
    g_list_free(config->presets);
    config->presets = NULL;

    for (i = 0; lines[i]; i++)
    {
        if (lines[i][0] == '[')
        {
            if ((tmp = strchr(lines[i], ']')))
            {
                *tmp = 0;
                if ((name = g_strdup(lines[i] + 1)))
                    config->presets = g_list_append(config->presets, name);
            }
        }
    }

    g_strfreev(lines);
}
#endif

static void
read_fade_config(ConfigFile *cfgfile, gchar *section, gchar *key, fade_config_t *fc)
{
    gchar *s = NULL;
    gint n;

    if (!cfgfile || !section || !key || !fc)
        return;

    xmms_cfg_read_string(cfgfile, section, key, &s);
    if (!s)
        return;

    n = sscanf(s,
           "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",
           &fc->type,
           &fc->pause_len_ms,
           &fc->simple_len_ms,
           &fc->out_enable,
           &fc->out_len_ms,
           &fc->out_volume,
           &fc->ofs_type,
           &fc->ofs_type_wanted,
           &fc->ofs_custom_ms,
           &fc->in_locked,
           &fc->in_enable,
           &fc->in_len_ms,
           &fc->in_volume,
           &fc->flush_pause_enable,
           &fc->flush_pause_len_ms, &fc->flush_in_enable, &fc->flush_in_len_ms, &fc->flush_in_volume);

    g_free(s);
}

static void
write_fade_config(ConfigFile *cfgfile, gchar *section, gchar *key, fade_config_t *fc)
{
    gchar *s;

    if (!cfgfile || !section || !key || !fc)
        return;

    s = g_strdup_printf("%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",
                fc->type,
                fc->pause_len_ms,
                fc->simple_len_ms,
                fc->out_enable,
                fc->out_len_ms,
                fc->out_volume,
                fc->ofs_type,
                fc->ofs_type_wanted,
                fc->ofs_custom_ms,
                fc->in_locked,
                fc->in_enable,
                fc->in_len_ms,
                fc->in_volume,
                fc->flush_pause_enable,
                fc->flush_pause_len_ms, fc->flush_in_enable, fc->flush_in_len_ms, fc->flush_in_volume);

    if (!s)
        return;

    xmms_cfg_write_string(cfgfile, section, key, s);
    g_free(s);
}

void
xfade_load_config()
{
#ifdef PRESET_SUPPORT
    gchar *filename;
#endif
    gchar *section = "Crossfade";
    ConfigFile *cfgfile;

    if ((cfgfile = xmms_cfg_open_default_file()))
    {
        /* *INDENT-OFF* */
        /* config items used in v0.1 */
        xmms_cfg_read_string (cfgfile, section, "output_plugin",        &config->op_name);
        xmms_cfg_read_string (cfgfile, section, "op_config_string",     &config->op_config_string);
        xmms_cfg_read_int    (cfgfile, section, "buffer_size",          &config->mix_size_ms);
        xmms_cfg_read_int    (cfgfile, section, "sync_size",            &config->sync_size_ms);
        xmms_cfg_read_int    (cfgfile, section, "preload_size",         &config->preload_size_ms);
        xmms_cfg_read_int    (cfgfile, section, "songchange_timeout",   &config->songchange_timeout);
        xmms_cfg_read_boolean(cfgfile, section, "enable_mixer",         &config->enable_mixer);
        xmms_cfg_read_boolean(cfgfile, section, "mixer_reverse",        &config->mixer_reverse);
        xmms_cfg_read_boolean(cfgfile, section, "enable_debug",         &config->enable_debug);
        xmms_cfg_read_boolean(cfgfile, section, "enable_monitor",       &config->enable_monitor);

        /* config items introduced by v0.2 */
        xmms_cfg_read_boolean(cfgfile, section, "gap_lead_enable",      &config->gap_lead_enable);
        xmms_cfg_read_int    (cfgfile, section, "gap_lead_len_ms",      &config->gap_lead_len_ms);
        xmms_cfg_read_int    (cfgfile, section, "gap_lead_level",       &config->gap_lead_level);
        xmms_cfg_read_boolean(cfgfile, section, "gap_trail_enable",     &config->gap_trail_enable);
        xmms_cfg_read_int    (cfgfile, section, "gap_trail_len_ms",     &config->gap_trail_len_ms);
        xmms_cfg_read_int    (cfgfile, section, "gap_trail_level",      &config->gap_trail_level);
        xmms_cfg_read_int    (cfgfile, section, "gap_trail_locked",     &config->gap_trail_locked);

        /* config items introduced by v0.2.1 */
        xmms_cfg_read_boolean(cfgfile, section, "buffer_size_auto",     &config->mix_size_auto);

        /* config items introduced by v0.2.3 */
        xmms_cfg_read_boolean(cfgfile, section, "album_detection",      &config->album_detection);

        /* config items introduced by v0.2.4 */
        xmms_cfg_read_boolean(cfgfile, section, "http_workaround",      &config->enable_http_workaround);
        xmms_cfg_read_boolean(cfgfile, section, "enable_op_max_used",   &config->enable_op_max_used);
        xmms_cfg_read_int    (cfgfile, section, "op_max_used_ms",       &config->op_max_used_ms);

        /* config items introduced by v0.2.6 */
        xmms_cfg_read_string (cfgfile, section, "effect_plugin",        &config->ep_name);
        xmms_cfg_read_boolean(cfgfile, section, "effect_enable",        &config->ep_enable);

        /* config items introduced by v0.3.0 */
        xmms_cfg_read_boolean(cfgfile, section, "volnorm_enable",       &config->volnorm_enable);
        xmms_cfg_read_boolean(cfgfile, section, "volnorm_use_qa",       &config->volnorm_use_qa);
        xmms_cfg_read_int    (cfgfile, section, "volnorm_target",       &config->volnorm_target);
        xmms_cfg_read_boolean(cfgfile, section, "output_keep_opened",   &config->output_keep_opened);
        xmms_cfg_read_boolean(cfgfile, section, "mixer_software",       &config->mixer_software);
        xmms_cfg_read_int    (cfgfile, section, "mixer_vol_left",       &config->mixer_vol_left);
        xmms_cfg_read_int    (cfgfile, section, "mixer_vol_right",      &config->mixer_vol_right);

        /* config items introduced by v0.3.2 */
        xmms_cfg_read_boolean(cfgfile, section, "no_xfade_if_same_file",&config->no_xfade_if_same_file);

        /* config items introduced by v0.3.3 */
        xmms_cfg_read_boolean(cfgfile, section, "gap_crossing",         &config->gap_crossing);

        /* fade configs */
        read_fade_config(cfgfile, section, "fc_xfade",  &config->fc[FADE_CONFIG_XFADE]);
        read_fade_config(cfgfile, section, "fc_manual", &config->fc[FADE_CONFIG_MANUAL]);
        read_fade_config(cfgfile, section, "fc_album",  &config->fc[FADE_CONFIG_ALBUM]);
        read_fade_config(cfgfile, section, "fc_start",  &config->fc[FADE_CONFIG_START]);
        read_fade_config(cfgfile, section, "fc_stop",   &config->fc[FADE_CONFIG_STOP]);
        read_fade_config(cfgfile, section, "fc_eop",    &config->fc[FADE_CONFIG_EOP]);
        read_fade_config(cfgfile, section, "fc_seek",   &config->fc[FADE_CONFIG_SEEK]);
        read_fade_config(cfgfile, section, "fc_pause",  &config->fc[FADE_CONFIG_PAUSE]);

        xmms_cfg_free(cfgfile);
        AUDDBG("[crossfade] load_config: configuration loaded\n");
    }
    else
        AUDDBG("[crossfade] load_config: error loading config, using defaults\n");

#ifdef PRESET_SUPPORT
    filename = g_strconcat(g_get_home_dir(), "/.audacious/crossfade-presets", NULL);
    scan_presets(filename);
    g_free(filename);
#endif
}

void
xfade_save_config()
{
    gchar *section = "Crossfade";
    ConfigFile *cfgfile;

    if ((cfgfile = xmms_cfg_open_default_file()))
    {
        /* obsolete config items */
        xmms_cfg_remove_key(cfgfile, section, "underrun_pct");
        xmms_cfg_remove_key(cfgfile, section, "enable_crossfade");
        xmms_cfg_remove_key(cfgfile, section, "enable_gapkiller");
        xmms_cfg_remove_key(cfgfile, section, "mixer_use_master");
        xmms_cfg_remove_key(cfgfile, section, "late_effect");
        xmms_cfg_remove_key(cfgfile, section, "gap_lead_length");

        /* config items used in v0.1 */
        xmms_cfg_write_string (cfgfile, section, "output_plugin",        config->op_name ? config->op_name : DEFAULT_OP_NAME);
        xmms_cfg_write_string (cfgfile, section, "op_config_string",     config->op_config_string ? config->op_config_string : DEFAULT_OP_CONFIG_STRING);
        xmms_cfg_write_int    (cfgfile, section, "buffer_size",          config->mix_size_ms);
        xmms_cfg_write_int    (cfgfile, section, "sync_size",            config->sync_size_ms);
        xmms_cfg_write_int    (cfgfile, section, "preload_size",         config->preload_size_ms);
        xmms_cfg_write_int    (cfgfile, section, "songchange_timeout",   config->songchange_timeout);
        xmms_cfg_write_boolean(cfgfile, section, "enable_mixer",         config->enable_mixer);
        xmms_cfg_write_boolean(cfgfile, section, "mixer_reverse",        config->mixer_reverse);
        xmms_cfg_write_boolean(cfgfile, section, "enable_debug",         config->enable_debug);
        xmms_cfg_write_boolean(cfgfile, section, "enable_monitor",       config->enable_monitor);

        /* config items introduced by v0.2 */
        xmms_cfg_write_boolean(cfgfile, section, "gap_lead_enable",      config->gap_lead_enable);
        xmms_cfg_write_int    (cfgfile, section, "gap_lead_len_ms",      config->gap_lead_len_ms);
        xmms_cfg_write_int    (cfgfile, section, "gap_lead_level",       config->gap_lead_level);
        xmms_cfg_write_boolean(cfgfile, section, "gap_trail_enable",     config->gap_trail_enable);
        xmms_cfg_write_int    (cfgfile, section, "gap_trail_len_ms",     config->gap_trail_len_ms);
        xmms_cfg_write_int    (cfgfile, section, "gap_trail_level",      config->gap_trail_level);
        xmms_cfg_write_int    (cfgfile, section, "gap_trail_locked",     config->gap_trail_locked);

        /* config items introduced by v0.2.1 */
        xmms_cfg_write_boolean(cfgfile, section, "buffer_size_auto",     config->mix_size_auto);

        /* config items introduced by v0.2.3 */
        xmms_cfg_write_boolean(cfgfile, section, "album_detection",      config->album_detection);

        /* config items introduced by v0.2.4 */
        xmms_cfg_write_boolean(cfgfile, section, "http_workaround",      config->enable_http_workaround);
        xmms_cfg_write_boolean(cfgfile, section, "enable_op_max_used",   config->enable_op_max_used);
        xmms_cfg_write_int    (cfgfile, section, "op_max_used_ms",       config->op_max_used_ms);

        /* config items introduced by v0.3.0 */
#ifdef VOLUME_NORMALIZER
        xmms_cfg_write_boolean(cfgfile, section, "volnorm_enable",       config->volnorm_enable);
        xmms_cfg_write_boolean(cfgfile, section, "volnorm_use_qa",       config->volnorm_use_qa);
        xmms_cfg_write_int    (cfgfile, section, "volnorm_target",       config->volnorm_target);
#endif
        xmms_cfg_write_boolean(cfgfile, section, "output_keep_opened",   config->output_keep_opened);
        xmms_cfg_write_boolean(cfgfile, section, "mixer_software",       config->mixer_software);
        xmms_cfg_write_int    (cfgfile, section, "mixer_vol_left",       config->mixer_vol_left);
        xmms_cfg_write_int    (cfgfile, section, "mixer_vol_right",      config->mixer_vol_right);

        /* config items introduced by v0.3.2 */
        xmms_cfg_write_boolean(cfgfile, section, "no_xfade_if_same_file",config->no_xfade_if_same_file);

        /* config items introduced by v0.3.2 */
        xmms_cfg_write_boolean(cfgfile, section, "gap_crossing",         config->gap_crossing);

        /* fade configs */
        write_fade_config(cfgfile, section, "fc_xfade",  &config->fc[FADE_CONFIG_XFADE]);
        write_fade_config(cfgfile, section, "fc_manual", &config->fc[FADE_CONFIG_MANUAL]);
        write_fade_config(cfgfile, section, "fc_album",  &config->fc[FADE_CONFIG_ALBUM]);
        write_fade_config(cfgfile, section, "fc_start",  &config->fc[FADE_CONFIG_START]);
        write_fade_config(cfgfile, section, "fc_stop",   &config->fc[FADE_CONFIG_STOP]);
        write_fade_config(cfgfile, section, "fc_eop",    &config->fc[FADE_CONFIG_EOP]);
        write_fade_config(cfgfile, section, "fc_seek",   &config->fc[FADE_CONFIG_SEEK]);
        write_fade_config(cfgfile, section, "fc_pause",  &config->fc[FADE_CONFIG_PAUSE]);
        /* *INDENT-ON* */
        
        xmms_cfg_write_default_file(cfgfile);
        xmms_cfg_free(cfgfile);
        AUDDBG("[crossfade] save_config: configuration saved\n");
    }
    else
        AUDDBG("[crossfade] save_config: error saving configuration!\n");
}

#define SAFE_FREE(x) if(x) { g_free(x); x = NULL; }
void
xfade_free_config()
{
    SAFE_FREE(xfg->op_config_string);
    SAFE_FREE(xfg->op_name);

    g_list_foreach(config->presets, g_free_f, NULL);
    g_list_free(config->presets);
    config->presets = NULL;
}

void
xfade_load_plugin_config(gchar *config_string, gchar *plugin_name, plugin_config_t *plugin_config)
{
    update_plugin_config(&config_string, plugin_name, plugin_config, FALSE);
}

void
xfade_save_plugin_config(gchar **config_string, gchar *plugin_name, plugin_config_t *plugin_config)
{
    update_plugin_config(config_string, plugin_name, plugin_config, TRUE);
}

/*** helpers *****************************************************************/

gint
xfade_cfg_out_skip(fade_config_t *fc)
{
    if (!fc)
        return 0;
        
    switch (fc->config)
    {
        case FADE_CONFIG_TIMING:
            return fc->out_skip_ms;
    }
    return 0;
}

gint
xfade_cfg_fadeout_len(fade_config_t *fc)
{
    if (!fc)
        return 0;
        
    switch (fc->type)
    {
        case FADE_TYPE_SIMPLE_XF:
            return fc->simple_len_ms;
            
        case FADE_TYPE_ADVANCED_XF:
            return fc->out_enable ? fc->out_len_ms : 0;
            
        case FADE_TYPE_FADEOUT:
        case FADE_TYPE_PAUSE_ADV:
            return fc->out_len_ms;
    }
    return 0;
}

gint
xfade_cfg_fadeout_volume(fade_config_t *fc)
{
    gint volume;
    if (!fc)
        return 0;
        
    switch (fc->type)
    {
        case FADE_TYPE_ADVANCED_XF:
        case FADE_TYPE_FADEOUT:
            volume = fc->out_volume;
            if (volume <   0) volume =   0;
            if (volume > 100) volume = 100;
            return volume;
    }
    return 0;
}

gint
xfade_cfg_offset(fade_config_t *fc)
{
    if (!fc)
        return 0;
    
    switch (fc->type)
    {
        case FADE_TYPE_FLUSH:
            return fc->flush_pause_enable ? fc->flush_pause_len_ms : 0;
            
        case FADE_TYPE_PAUSE:
            return fc->pause_len_ms;
            
        case FADE_TYPE_SIMPLE_XF:
            return -fc->simple_len_ms;
            
        case FADE_TYPE_ADVANCED_XF:
            switch (fc->ofs_type)
            {
                case FC_OFFSET_LOCK_OUT: return -fc->out_len_ms;
                case FC_OFFSET_LOCK_IN:  return -fc->in_len_ms;
                case FC_OFFSET_CUSTOM:   return  fc->ofs_custom_ms;
            }
            return 0;
            
        case FADE_TYPE_FADEOUT:
        case FADE_TYPE_PAUSE_ADV:
            return fc->ofs_custom_ms;
    }
    return 0;
}

gint
xfade_cfg_in_skip(fade_config_t *fc)
{
    if (!fc)
        return 0;
        
    switch (fc->config)
    {
        case FADE_CONFIG_TIMING:
            return fc->in_skip_ms;
    }
    return 0;
}

gint
xfade_cfg_fadein_len(fade_config_t *fc)
{
    if (!fc)
        return 0;
        
    switch (fc->type)
    {
        case FADE_TYPE_FLUSH:
            return fc->flush_in_enable ? fc->flush_in_len_ms : 0;
            
        case FADE_TYPE_SIMPLE_XF:
            return fc->simple_len_ms;
            
        case FADE_TYPE_ADVANCED_XF:
            return fc->in_locked
                ? (fc->out_enable ? fc->out_len_ms : 0)
                : (fc->in_enable  ? fc->in_len_ms  : 0);
            
        case FADE_TYPE_FADEIN:
        case FADE_TYPE_PAUSE_ADV:
            return fc->in_len_ms;
    }
    return 0;
}

gint
xfade_cfg_fadein_volume(fade_config_t *fc)
{
    gint volume;
    if (!fc)
        return 0;

    switch (fc->type)
    {
        case FADE_TYPE_FLUSH:
            volume = fc->flush_in_volume;
            break;
            
        case FADE_TYPE_ADVANCED_XF:
            volume = fc->in_locked ? fc->out_volume : fc->in_volume;
            break;
            
        case FADE_TYPE_FADEIN:
            volume = fc->in_volume;
            break;
            
        default:
            volume = 0;
    }

    if (volume <   0) volume =   0;
    if (volume > 100) volume = 100;
    return volume;
}

gboolean
xfade_cfg_gap_trail_enable(config_t *cfg)
{
    return xfg->gap_trail_locked ? xfg->gap_lead_enable : xfg->gap_trail_enable;
}

gint
xfade_cfg_gap_trail_len(config_t *cfg)
{
    if (!xfade_cfg_gap_trail_enable(cfg))
        return 0;

    return xfg->gap_trail_locked ? xfg->gap_lead_len_ms : xfg->gap_trail_len_ms;
}

gint
xfade_cfg_gap_trail_level(config_t *cfg)
{
    return xfg->gap_trail_locked ? xfg->gap_lead_level : xfg->gap_trail_level;
}

gint
xfade_mix_size_ms(config_t *cfg)
{
    if (xfg->mix_size_auto)
    {
        gint i, min_size = 0;

        for (i = 0; i < MAX_FADE_CONFIGS; i++)
        {
            gint size = xfade_cfg_fadeout_len(&xfg->fc[i]);
            gint offset = xfade_cfg_offset(&xfg->fc[i]);

            if (xfg->fc[i].type == FADE_TYPE_PAUSE_ADV)
                size += xfade_cfg_fadein_len(&xfg->fc[i]);

            if (size < -offset)
                size = -offset;

            if (size > min_size)
                min_size = size;
        }
        return min_size += xfade_cfg_gap_trail_len(cfg) + xfg->songchange_timeout;
    }
    else
        return xfg->mix_size_ms;
}

/*****************************************************************************/
