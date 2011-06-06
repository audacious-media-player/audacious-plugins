/*
   XMMS-SID - SIDPlay input plugin for X MultiMedia System (XMMS)

   Configuration dialog

   Programmed and designed by Matti 'ccr' Hamalainen <ccr@tnsp.org>
   (C) Copyright 1999-2009 Tecnic Software productions (TNSP)

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License along
   with this program; if not, write to the Free Software Foundation, Inc.,
   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/
#include "xs_config.h"

#ifdef AUDACIOUS_PLUGIN

#include <audacious/configdb.h>
#include <audacious/drct.h>
#include <audacious/plugin.h>

#define XS_CONFIG_FILE          mcs_handle_t
#define XS_CONFIG_OPEN          aud_cfg_db_open
#define XS_CONFIG_FREE          aud_cfg_db_close

#define XS_CFG_SET_STRING(q,z)  aud_cfg_db_set_string(cfg, XS_CONFIG_IDENT, q, z)
#define XS_CFG_SET_FLOAT(q,z)   aud_cfg_db_set_float(cfg, XS_CONFIG_IDENT, q, z)
#define XS_CFG_SET_INT(q,z)     aud_cfg_db_set_int(cfg, XS_CONFIG_IDENT, q, z)
#define XS_CFG_SET_BOOL(q,z)    aud_cfg_db_set_bool(cfg, XS_CONFIG_IDENT, q, z)
#define XS_CFG_GET_STRING(q,z)  aud_cfg_db_get_string(cfg, XS_CONFIG_IDENT, q, z)
#define XS_CFG_GET_FLOAT(q,z)   aud_cfg_db_get_float(cfg, XS_CONFIG_IDENT, q, z)
#define XS_CFG_GET_INT(q,z)     aud_cfg_db_get_int(cfg, XS_CONFIG_IDENT, q, z)
#define XS_CFG_GET_BOOL(q,z)    aud_cfg_db_get_bool(cfg, XS_CONFIG_IDENT, q, z)
#else
#include <xmms/configfile.h>
#define XS_CONFIG_FILE          ConfigFile
#define XS_CONFIG_OPEN          xmms_cfg_open_default_file
#define XS_CONFIG_FREE          xmms_cfg_free

#define XS_CFG_SET_STRING(q,z)  xmms_cfg_write_string(cfg, XS_CONFIG_IDENT, q, z)
#define XS_CFG_SET_FLOAT(q,z)   xmms_cfg_write_float(cfg, XS_CONFIG_IDENT, q, z)
#define XS_CFG_SET_INT(q,z)     xmms_cfg_write_int(cfg, XS_CONFIG_IDENT, q, z)
#define XS_CFG_SET_BOOL(q,z)    xmms_cfg_write_boolean(cfg, XS_CONFIG_IDENT, q, z)
#define XS_CFG_GET_STRING(q,z)  xmms_cfg_read_string(cfg, XS_CONFIG_IDENT, q, z)
#define XS_CFG_GET_FLOAT(q,z)   xmms_cfg_read_float(cfg, XS_CONFIG_IDENT, q, z)
#define XS_CFG_GET_INT(q,z)     xmms_cfg_read_int(cfg, XS_CONFIG_IDENT, q, z)
#define XS_CFG_GET_BOOL(q,z)    xmms_cfg_read_boolean(cfg, XS_CONFIG_IDENT, q, z)
#endif
#include <stdio.h>
#include <ctype.h>
#include "xs_glade.h"
#include "xs_interface.h"
#include "xs_support.h"


/*
 * Global widgets
 */
#if 0
static GtkWidget *xs_configwin = NULL,
    *xs_sldb_fileselector = NULL,
    *xs_stil_fileselector = NULL,
    *xs_hvsc_selector = NULL,
    *xs_filt_importselector = NULL,
    *xs_filt_exportselector = NULL;
#endif

#define LUW(x)    lookup_widget(xs_configwin, x)

/* Samplerates
 */
static const gchar *xs_samplerates_table[] = {
    "8000", "11025", "22050",
    "44100", "48000", "64000",
    "96000"
};

static const gint xs_nsamplerates_table = (sizeof(xs_samplerates_table) / sizeof(xs_samplerates_table[0]));

/*
 * Configuration specific stuff
 */
XS_MUTEX(xs_cfg);
struct xs_cfg_t xs_cfg;

static xs_cfg_item_t xs_cfgtable[] = {
{ CTYPE_INT,    &xs_cfg.audioBitsPerSample,     "audioBitsPerSample" },
{ CTYPE_INT,    &xs_cfg.audioChannels,          "audioChannels" },
{ CTYPE_INT,    &xs_cfg.audioFrequency,         "audioFrequency" },

{ CTYPE_BOOL,   &xs_cfg.mos8580,                "mos8580" },
{ CTYPE_BOOL,   &xs_cfg.forceModel,             "forceModel" },
{ CTYPE_BOOL,   &xs_cfg.emulateFilters,         "emulateFilters" },
{ CTYPE_FLOAT,  &xs_cfg.sid1Filter.fs,          "filterFs" },
{ CTYPE_FLOAT,  &xs_cfg.sid1Filter.fm,          "filterFm" },
{ CTYPE_FLOAT,  &xs_cfg.sid1Filter.ft,          "filterFt" },
{ CTYPE_INT,    &xs_cfg.memoryMode,             "memoryMode" },
{ CTYPE_INT,    &xs_cfg.clockSpeed,             "clockSpeed" },
{ CTYPE_BOOL,   &xs_cfg.forceSpeed,             "forceSpeed" },

{ CTYPE_INT,    &xs_cfg.playerEngine,           "playerEngine" },

{ CTYPE_INT,    &xs_cfg.sid2Builder,            "sid2Builder" },
{ CTYPE_INT,    &xs_cfg.sid2OptLevel,           "sid2OptLevel" },
{ CTYPE_INT,    &xs_cfg.sid2NFilterPresets,     "sid2NFilterPresets" },

{ CTYPE_BOOL,   &xs_cfg.oversampleEnable,       "oversampleEnable" },
{ CTYPE_INT,    &xs_cfg.oversampleFactor,       "oversampleFactor" },

{ CTYPE_BOOL,   &xs_cfg.playMaxTimeEnable,      "playMaxTimeEnable" },
{ CTYPE_BOOL,   &xs_cfg.playMaxTimeUnknown,     "playMaxTimeUnknown" },
{ CTYPE_INT,    &xs_cfg.playMaxTime,            "playMaxTime" },
{ CTYPE_BOOL,   &xs_cfg.playMinTimeEnable,      "playMinTimeEnable" },
{ CTYPE_INT,    &xs_cfg.playMinTime,            "playMinTime" },
{ CTYPE_BOOL,   &xs_cfg.songlenDBEnable,        "songlenDBEnable" },
{ CTYPE_STR,    &xs_cfg.songlenDBPath,          "songlenDBPath" },

{ CTYPE_BOOL,   &xs_cfg.stilDBEnable,           "stilDBEnable" },
{ CTYPE_STR,    &xs_cfg.stilDBPath,             "stilDBPath" },
{ CTYPE_STR,    &xs_cfg.hvscPath,               "hvscPath" },

#ifndef AUDACIOUS_PLUGIN
{ CTYPE_INT,    &xs_cfg.subsongControl,         "subsongControl" },
{ CTYPE_BOOL,   &xs_cfg.detectMagic,            "detectMagic" },
#endif

{ CTYPE_BOOL,   &xs_cfg.titleOverride,          "titleOverride" },
{ CTYPE_STR,    &xs_cfg.titleFormat,            "titleFormat" },

{ CTYPE_BOOL,   &xs_cfg.subAutoEnable,          "subAutoEnable" },
{ CTYPE_BOOL,   &xs_cfg.subAutoMinOnly,         "subAutoMinOnly" },
{ CTYPE_INT,    &xs_cfg.subAutoMinTime,         "subAutoMinTime" },
};

static const gint xs_cfgtable_max = (sizeof(xs_cfgtable) / sizeof(xs_cfgtable[0]));


static xs_wid_item_t xs_widtable[] = {
{ WTYPE_BGROUP, CTYPE_INT,      "cfg_res_16bit",        &xs_cfg.audioBitsPerSample,     XS_RES_16BIT },
{ WTYPE_BGROUP, CTYPE_INT,      "cfg_res_8bit",         &xs_cfg.audioBitsPerSample,     XS_RES_8BIT },
{ WTYPE_BGROUP, CTYPE_INT,      "cfg_chn_mono",         &xs_cfg.audioChannels,          XS_CHN_MONO },
{ WTYPE_BGROUP, CTYPE_INT,      "cfg_chn_stereo",       &xs_cfg.audioChannels,          XS_CHN_STEREO },
{ WTYPE_BGROUP, CTYPE_INT,      "cfg_chn_autopan",      &xs_cfg.audioChannels,          XS_CHN_AUTOPAN },
{ WTYPE_COMBO,  CTYPE_INT,      "cfg_samplerate",       &xs_cfg.audioFrequency,         XS_AUDIO_FREQ },
{ WTYPE_BUTTON, CTYPE_BOOL,     "cfg_oversample",       &xs_cfg.oversampleEnable,       0 },
{ WTYPE_SPIN,   CTYPE_INT,      "cfg_oversample_factor",&xs_cfg.oversampleFactor,       0 },

{ WTYPE_BGROUP, CTYPE_INT,      "cfg_emu_sidplay1",     &xs_cfg.playerEngine,           XS_ENG_SIDPLAY1 },
{ WTYPE_BGROUP, CTYPE_INT,      "cfg_emu_sidplay2",     &xs_cfg.playerEngine,           XS_ENG_SIDPLAY2 },
{ WTYPE_BGROUP, CTYPE_INT,      "cfg_emu_mem_real",     &xs_cfg.memoryMode,             XS_MPU_REAL },
{ WTYPE_BGROUP, CTYPE_INT,      "cfg_emu_mem_banksw",   &xs_cfg.memoryMode,             XS_MPU_BANK_SWITCHING },
{ WTYPE_BGROUP, CTYPE_INT,      "cfg_emu_mem_transrom", &xs_cfg.memoryMode,             XS_MPU_TRANSPARENT_ROM },
{ WTYPE_BGROUP, CTYPE_INT,      "cfg_emu_mem_playsid",  &xs_cfg.memoryMode,             XS_MPU_PLAYSID_ENVIRONMENT },

{ WTYPE_BUTTON, CTYPE_BOOL,     "cfg_emu_mos8580",      &xs_cfg.mos8580,                0 },
{ WTYPE_BUTTON, CTYPE_BOOL,     "cfg_emu_sid_force",    &xs_cfg.forceModel,             0 },
{ WTYPE_BGROUP, CTYPE_INT,      "cfg_emu_clock_ntsc",   &xs_cfg.clockSpeed,             XS_CLOCK_NTSC },
{ WTYPE_BGROUP, CTYPE_INT,      "cfg_emu_clock_pal",    &xs_cfg.clockSpeed,             XS_CLOCK_PAL },
{ WTYPE_BUTTON, CTYPE_BOOL,     "cfg_emu_clock_force",  &xs_cfg.forceSpeed,             0 },
{ WTYPE_BUTTON, CTYPE_BOOL,     "cfg_emu_sp2_opt",      &xs_cfg.sid2OptLevel,           0 },

{ WTYPE_BGROUP, CTYPE_INT,      "cfg_emu_sp2_resid",    &xs_cfg.sid2Builder,            XS_BLD_RESID },
{ WTYPE_BGROUP, CTYPE_INT,      "cfg_emu_sp2_hardsid",  &xs_cfg.sid2Builder,            XS_BLD_HARDSID },

{ WTYPE_BUTTON, CTYPE_BOOL,     "cfg_emu_filters",      &xs_cfg.emulateFilters,         0 },
{ WTYPE_SCALE,  CTYPE_FLOAT,    "cfg_sp1_filter_fs",    &xs_cfg.sid1Filter.fs,          0 },
{ WTYPE_SCALE,  CTYPE_FLOAT,    "cfg_sp1_filter_fm",    &xs_cfg.sid1Filter.fm,          0 },
{ WTYPE_SCALE,  CTYPE_FLOAT,    "cfg_sp1_filter_ft",    &xs_cfg.sid1Filter.ft,          0 },

{ WTYPE_BUTTON, CTYPE_BOOL,     "cfg_maxtime_enable",   &xs_cfg.playMaxTimeEnable,      0 },
{ WTYPE_BUTTON, CTYPE_BOOL,     "cfg_maxtime_unknown",  &xs_cfg.playMaxTimeUnknown,     0 },
{ WTYPE_SPIN,   CTYPE_INT,      "cfg_maxtime",          &xs_cfg.playMaxTime,            0 },
{ WTYPE_BUTTON, CTYPE_BOOL,     "cfg_mintime_enable",   &xs_cfg.playMinTimeEnable,      0 },
{ WTYPE_SPIN,   CTYPE_INT,      "cfg_mintime",          &xs_cfg.playMinTime,            0 },
{ WTYPE_BUTTON, CTYPE_BOOL,     "cfg_sld_enable",       &xs_cfg.songlenDBEnable,        0 },
{ WTYPE_TEXT,   CTYPE_STR,      "cfg_sld_dbpath",       &xs_cfg.songlenDBPath,          0 },

{ WTYPE_BUTTON, CTYPE_BOOL,     "cfg_stil_enable",      &xs_cfg.stilDBEnable,           0 },
{ WTYPE_TEXT,   CTYPE_STR,      "cfg_stil_dbpath",      &xs_cfg.stilDBPath,             0 },
{ WTYPE_TEXT,   CTYPE_STR,      "cfg_hvsc_path",        &xs_cfg.hvscPath,               0 },

#ifndef AUDACIOUS_PLUGIN
{ WTYPE_BGROUP, CTYPE_INT,      "cfg_subctrl_none",     &xs_cfg.subsongControl,         XS_SSC_NONE },
{ WTYPE_BGROUP, CTYPE_INT,      "cfg_subctrl_seek",     &xs_cfg.subsongControl,         XS_SSC_SEEK },
{ WTYPE_BGROUP, CTYPE_INT,      "cfg_subctrl_popup",    &xs_cfg.subsongControl,         XS_SSC_POPUP },
{ WTYPE_BGROUP, CTYPE_INT,      "cfg_subctrl_patch",    &xs_cfg.subsongControl,         XS_SSC_PATCH },

{ WTYPE_BUTTON, CTYPE_BOOL,     "cfg_detectmagic",      &xs_cfg.detectMagic,            0 },
#endif

{ WTYPE_BUTTON, CTYPE_BOOL,     "cfg_ftitle_override",  &xs_cfg.titleOverride,          0 },
{ WTYPE_TEXT,   CTYPE_STR,      "cfg_ftitle_format",    &xs_cfg.titleFormat,            0 },

{ WTYPE_BUTTON, CTYPE_BOOL,     "cfg_subauto_enable",   &xs_cfg.subAutoEnable,          0 },
{ WTYPE_BUTTON, CTYPE_BOOL,     "cfg_subauto_min_only", &xs_cfg.subAutoMinOnly,         0 },
{ WTYPE_SPIN,   CTYPE_INT,      "cfg_subauto_mintime",  &xs_cfg.subAutoMinTime,         0 },
};

static const gint xs_widtable_max = (sizeof(xs_widtable) / sizeof(xs_widtable[0]));


/* Reset/initialize the configuration
 */
void xs_init_configuration(void)
{
    /* Lock configuration mutex */
    XSDEBUG("initializing configuration ...\n");
    XS_MUTEX_LOCK(xs_cfg);

    memset(&xs_cfg, 0, sizeof(xs_cfg));

    /* Initialize values with sensible defaults */
    xs_cfg.audioBitsPerSample = XS_RES_16BIT;
    xs_cfg.audioChannels = XS_CHN_MONO;
    xs_cfg.audioFrequency = XS_AUDIO_FREQ;

    xs_cfg.mos8580 = FALSE;
    xs_cfg.forceModel = FALSE;

    /* Filter values */
    xs_cfg.emulateFilters = TRUE;
    xs_cfg.sid1Filter.fs = XS_SIDPLAY1_FS;
    xs_cfg.sid1Filter.fm = XS_SIDPLAY1_FM;
    xs_cfg.sid1Filter.ft = XS_SIDPLAY1_FT;

#ifdef HAVE_SIDPLAY2
    xs_cfg.playerEngine = XS_ENG_SIDPLAY2;
    xs_cfg.memoryMode = XS_MPU_REAL;
#else
#ifdef HAVE_SIDPLAY1
    xs_cfg.playerEngine = XS_ENG_SIDPLAY1;
    xs_cfg.memoryMode = XS_MPU_BANK_SWITCHING;
#else
#error This should not happen! No emulator engines configured in!
#endif
#endif

    xs_cfg.clockSpeed = XS_CLOCK_PAL;
    xs_cfg.forceSpeed = FALSE;

    xs_cfg.sid2OptLevel = 0;
    xs_cfg.sid2NFilterPresets = 0;

#ifdef HAVE_RESID_BUILDER
    xs_cfg.sid2Builder = XS_BLD_RESID;
#else
#ifdef HAVE_HARDSID_BUILDER
    xs_cfg.sid2Builder = XS_BLD_HARDSID;
#else
#ifdef HAVE_SIDPLAY2
#error This should not happen! No supported SIDPlay2 builders configured in!
#endif
#endif
#endif

    xs_cfg.oversampleEnable = FALSE;
    xs_cfg.oversampleFactor = XS_MIN_OVERSAMPLE;

    xs_cfg.playMaxTimeEnable = FALSE;
    xs_cfg.playMaxTimeUnknown = FALSE;
    xs_cfg.playMaxTime = 150;

    xs_cfg.playMinTimeEnable = FALSE;
    xs_cfg.playMinTime = 15;

    xs_cfg.songlenDBEnable = FALSE;
    xs_pstrcpy(&xs_cfg.songlenDBPath, "~/C64Music/DOCUMENTS/Songlengths.txt");

    xs_cfg.stilDBEnable = FALSE;
    xs_pstrcpy(&xs_cfg.stilDBPath, "~/C64Music/DOCUMENTS/STIL.txt");
    xs_pstrcpy(&xs_cfg.hvscPath, "~/C64Music");

#if defined(HAVE_SONG_POSITION) && !defined(AUDACIOUS_PLUGIN)
    xs_cfg.subsongControl = XS_SSC_PATCH;
#else
    xs_cfg.subsongControl = XS_SSC_POPUP;
#endif
    xs_cfg.detectMagic = FALSE;

#ifndef HAVE_XMMSEXTRA
    xs_cfg.titleOverride = TRUE;
#endif

#ifdef AUDACIOUS_PLUGIN
    xs_pstrcpy(&xs_cfg.titleFormat, "${artist} - ${title} (${copyright}) <${subsong-id}/${subsong-num}> [${sid-model}/${sid-speed}]");
#else
    xs_pstrcpy(&xs_cfg.titleFormat, "%p - %t (%c) <%n/%N> [%m/%C]");
#endif

    xs_cfg.subAutoEnable = FALSE;
    xs_cfg.subAutoMinOnly = TRUE;
    xs_cfg.subAutoMinTime = 15;


    /* Unlock the configuration */
    XS_MUTEX_UNLOCK(xs_cfg);
}


/* Filter configuration handling
 */
#if 0
static void xs_filters_error(const gchar *fmt, ...)
{
    va_list ap;

#ifdef AUDACIOUS_PLUGIN
    va_start(ap, fmt);
    g_logv("AUD-SID", G_LOG_LEVEL_WARNING, fmt, ap);
    va_end(ap);
#else
    gchar *msg;

    va_start(ap, fmt);
    msg = g_strdup_vprintf(fmt, ap);
    va_end(ap);
    xmms_show_message(_(PACKAGE_NAME " Error"), msg, _("OK"), TRUE, NULL, NULL);
    g_free(msg);
#endif
}
#endif


static void xs_filter_free(xs_sid_filter_t *filter)
{
    if (filter) {
        g_free(filter->name);
        filter->name = NULL;
        g_free(filter);
    }
}


#define XS_FITEM (4 * 2)

static gboolean xs_filter_load_into(XS_CONFIG_FILE *cfg, gint nFilter, xs_sid_filter_t *filter)
{
    gchar tmpKey[64], *tmpStr;

    /* Get fields from config */
    g_snprintf(tmpKey, sizeof(tmpKey), "filter%dType", nFilter);
    if (!XS_CFG_GET_INT(tmpKey, &(filter->type)))
        return FALSE;

    g_snprintf(tmpKey, sizeof(tmpKey), "filter%dName", nFilter);
    if (!XS_CFG_GET_STRING(tmpKey, &tmpStr))
        return FALSE;

    filter->name = g_strdup(tmpStr);
    if (filter->name == NULL)
        return FALSE;

    if (filter->type == 1) {
        gint i, j;

        /* Types 1 has points */
        g_snprintf(tmpKey, sizeof(tmpKey), "filter%dNPoints", nFilter);
        if (!XS_CFG_GET_INT(tmpKey, &(filter->npoints)))
            return FALSE;

        g_snprintf(tmpKey, sizeof(tmpKey), "filter%dPoints", nFilter);
        if (!XS_CFG_GET_STRING(tmpKey, &tmpStr))
            return FALSE;

        for (i = 0, j = 0; i < filter->npoints; i++, j += XS_FITEM) {
            if (sscanf(&tmpStr[j], "%4x%4x",
                &(filter->points[i].x),
                &(filter->points[i].y)) != 2)
                return FALSE;
        }
    } else if (filter->type == 3) {
        /* Type 3 has tunables */
        g_snprintf(tmpKey, sizeof(tmpKey), "filter%dData", nFilter);
        if (!XS_CFG_GET_STRING(tmpKey, &tmpStr))
            return FALSE;

        if (sscanf(tmpStr, "%f,%f,%f,%f", &filter->rate, &filter->point,
            &filter->voice_nonlinearity, &filter->cf_treshold) != 4)
            return FALSE;

        g_snprintf(tmpKey, sizeof(tmpKey), "filter%dData3", nFilter);
        if (!XS_CFG_GET_STRING(tmpKey, &tmpStr))
            return FALSE;

        if (sscanf(tmpStr, "%f,%f,%f,%f", &filter->baseresistance,
            &filter->offset, &filter->steepness,
            &filter->minimumfetresistance) != 4)
            return FALSE;

    } else if (filter->type == 4) {
        /* Type 4 has fewer tunables */
        g_snprintf(tmpKey, sizeof(tmpKey), "filter%dData4", nFilter);
        if (!XS_CFG_GET_STRING(tmpKey, &tmpStr))
            return FALSE;

        if (sscanf(tmpStr, "%f,%f", &filter->k, &filter->b) != 2)
            return FALSE;
    } else {
        xs_error("Unknown filter type %d for '%s' (#%d).\n", filter->type, filter->name, nFilter);
        return FALSE;
    }

    return TRUE;
}


static xs_sid_filter_t * xs_filter_load(XS_CONFIG_FILE *cfg, gint nFilter)
{
    xs_sid_filter_t *filter;

    /* Allocate filter struct */
    if ((filter = g_malloc0(sizeof(xs_sid_filter_t))) == NULL)
        return NULL;

    if (!xs_filter_load_into(cfg, nFilter, filter)) {
        xs_error("Error loading filter %d from configuration.\n", nFilter);
        xs_filter_free(filter);
        return NULL;
    } else
        return filter;
}


#if 0
static gboolean xs_filter_save(XS_CONFIG_FILE *cfg, xs_sid_filter_t *pFilter, gint nFilter)
{
    gchar *tmpValue, tmpKey[64];
    gint i, j;

    /* Allocate memory for value string */
    tmpValue = g_malloc(sizeof(gchar) * XS_FITEM * (pFilter->npoints + 1));
    if (tmpValue == NULL)
        return FALSE;

    /* Make value string */
    for (i = 0, j = 0; i < pFilter->npoints; i++, j += XS_FITEM) {
        g_snprintf(&tmpValue[j], XS_FITEM+1, "%04x%04x",
            pFilter->points[i].x,
            pFilter->points[i].y);
    }

    /* Write into the configuration */
    g_snprintf(tmpKey, sizeof(tmpKey), "filter%dName", nFilter);
    XS_CFG_SET_STRING(tmpKey, pFilter->name);

    g_snprintf(tmpKey, sizeof(tmpKey), "filter%dNPoints", nFilter);
    XS_CFG_SET_INT(tmpKey, pFilter->npoints);

    g_snprintf(tmpKey, sizeof(tmpKey), "filter%dPoints", nFilter);
    XS_CFG_SET_STRING(tmpKey, tmpValue);

    g_free(tmpValue);
    return TRUE;
}


/* Filter exporting and importing. These functions export/import
 * filter settings to/from SIDPlay2 INI-type files.
 */
static gboolean xs_fgetitem(gchar *inLine, size_t *linePos, gchar sep, gchar *tmpStr, size_t tmpMax)
{
    size_t i;

    for (i = 0; i < tmpMax - 1 && inLine[*linePos] &&
        inLine[*linePos] != sep; i++, (*linePos)++)
        tmpStr[i] = inLine[*linePos];

    tmpStr[i] = 0;
    while (--i > 0 && isspace(tmpStr[i])) tmpStr[i] = 0;

    xs_findnext(inLine, linePos);
    return (inLine[*linePos] == sep);
}


static gboolean xs_chkf(xs_sid_filter_t *filter, const gchar *str, const gchar *name, gint type)
{
    if (g_strncasecmp(str, name, strlen(name)))
        return FALSE;
    if (filter->type != type) {
        if (filter->type == -1) {
            filter->type = type;
            return TRUE;
        } else {
            xs_error("Unexpected key '%s' for filter type %d.\n");
            return FALSE;
        }
    } else
        return TRUE;
}

static gboolean xs_filters_import(const gchar *filename, xs_sid_filter_t **pFilters, gint *nFilters)
{
    FILE *inFile;
    gchar inLine[XS_BUF_SIZE], tmpStr[XS_BUF_SIZE];
    gchar *sectName = NULL;
    gboolean inSection, isError = FALSE;
    size_t lineNum, i;
    xs_sid_filter_t *filter = NULL;

fprintf(stderr, "xs_filters_import(%s)\n", filename);

    if ((inFile = fopen(filename, "ra")) == NULL) {
        xs_filters_error("");
        return FALSE;
    }

fprintf(stderr, "importing...\n");

    inSection = FALSE;
    lineNum = 0;
    while (fgets(inLine, XS_BUF_SIZE, inFile) != NULL && !isError) {
        size_t linePos = 0;
        lineNum++;

        xs_findnext(inLine, &linePos);
        if (isalpha(inLine[linePos])) {
            /* A new key/value pair */
            if (!xs_fgetitem(inLine, &linePos, '=', tmpStr, XS_BUF_SIZE)) {
                xs_error("Invalid line '%s' :: expected =", inLine);
                isError = TRUE;
            } else if (inSection) {
                linePos++;
                xs_findnext(inLine, &linePos);

                if (xs_chkf(filter, tmpStr, "points", 1)) {

                } else if (xs_chkf(filter, tmpStr, "point", 1)) {

                } else if (!g_strncasecmp(tmpStr, "type", 4)) {
                    if (filter->type != -1) {
                        xs_error("Filter type %d already set for '%s'\n",
                            filter->type, sectName);
                    }

                } else {
                    xs_error("Unsupported definition '%s' @ '%s'\n",
                        tmpStr, sectName);
                }
            }
        } else if (inLine[linePos] == '[') {
            /* Check for existing section */
            if (inSection) {
                /* Submit definition */
                fprintf(stderr, "filter ends: %s\n", sectName);
                if ((filter = g_malloc0(sizeof(xs_sid_filter_t))) == NULL) {
                    fprintf(stderr, "could not allocate ..\n");
                } else {

                }
                g_free(sectName);
            }

            /* New filter(?) section starts */
            linePos++;
            for (i = 0; i < XS_BUF_SIZE-1 && inLine[linePos] && inLine[linePos] != ']'; i++, linePos++)
                tmpStr[i] = inLine[linePos];
            tmpStr[i] = 0;

            if (inLine[linePos] != ']') {
                fprintf(stderr, "invalid! expected ']': %s\n", inLine);
            } else {
                if (!g_strncasecmp(tmpStr, "filter", 6)) {
                    sectName = strdup(tmpStr + 6);
                    fprintf(stderr, "filter: %s\n", sectName);
                    inSection = TRUE;
                } else {
                    fprintf(stderr, "ignoring section: %s\n", tmpStr);
                    inSection = FALSE;
                }
            }
        } else if (inLine[linePos] != '#' && inLine[linePos] != ';' && inLine[linePos] != 0) {
            /* Syntax error */
            xs_error("Syntax error: '%s'\n", inLine);
            isError = TRUE;
        }
    }

    fclose(inFile);
    return TRUE;
}


static gboolean xs_filter_export(FILE *outFile, xs_sid_filter_t *filter)
{
    fprintf(outFile,
    "[Filter%s]\n"
    "type=%d\n",
    filter->name, filter->type);

    if (filter->type == 1) {
        gint i;
        fprintf(outFile, "points=%d\n", filter->npoints);

        for (i = 0; i < filter->npoints; i++) {
            fprintf(outFile,
            "point%d=%d,%d\n",
            i + 1,
            filter->points[i].x,
            filter->points[i].y);
        }
    } else if (filter->type == 3) {
        fprintf(outFile,
        "DistortionRate            = %f\n"
        "DistortionPoint           = %f\n"
        "VoiceNonlinearity         = %f\n"
        "DistortionCFThreshold     = %f\n",
        filter->rate, filter->point,
        filter->voice_nonlinearity,
        filter->cf_treshold);

        fprintf(outFile,
        "Type3BaseResistance       = %f\n"
        "Type3Offset               = %f\n"
        "Type3Steepness            = %f\n"
        "Type3MinimumFETResistance = %f\n",
        filter->baseresistance, filter->offset,
        filter->steepness, filter->minimumfetresistance);

    } else if (filter->type == 4) {
        fprintf(outFile,
        "Type4K=%f\n"
        "Type4B=%f\n",
        filter->k, filter->b);
    } else {
        xs_error("Filter '%s' has type %d, which is unsupported by export.\n",
        filter->name, filter->type);
        return FALSE;
    }

    fprintf(outFile, "\n");
    return TRUE;
}


static gboolean xs_filters_export(const gchar *filename, xs_sid_filter_t **filters, gint nFilters)
{
    gboolean result = TRUE;
    FILE *outFile;
    gint n;

    /* Open/create the file */
    if ((outFile = fopen(filename, "wa")) == NULL) {
        xs_filters_error("Could not open '%s' for writing! Not exporting.", filename);
        return FALSE;
    }

    /* Header */
    fprintf(outFile,
    "; SIDPlay2 compatible filter definition file\n"
    "; Exported by " PACKAGE_STRING "\n\n");

    /* Write each filter spec in "INI"-style format */
    for (n = 0; n < nFilters; n++) {
        if (!xs_filter_export(outFile, filters[n])) {
            result = FALSE;
            break;
        }
    }

    fclose(outFile);

    if (!result)
        xs_filters_error("Some filters could not be exported!");

    return result;
}
#endif


/* Get the configuration (from file or default)
 */
void xs_read_configuration(void)
{
    XS_CONFIG_FILE *cfg;
    gint i;
    gchar *tmpStr;

    /* Try to open the XMMS configuration file  */
    XS_MUTEX_LOCK(xs_cfg);
    XSDEBUG("loading from config-file ...\n");

    cfg = XS_CONFIG_OPEN();

    if (cfg == NULL) {
        XSDEBUG("Could not open configuration, trying to write defaults...\n");
        xs_write_configuration();
        return;
    }

    /* Read the new settings from XMMS configuration file */
    for (i = 0; i < xs_cfgtable_max; i++) {
        switch (xs_cfgtable[i].itemType) {
        case CTYPE_INT:
            XS_CFG_GET_INT(xs_cfgtable[i].itemName,
                (gint *) xs_cfgtable[i].itemData);
            break;

        case CTYPE_BOOL:
            XS_CFG_GET_BOOL(xs_cfgtable[i].itemName,
                (gboolean *) xs_cfgtable[i].itemData);
            break;

        case CTYPE_FLOAT:
            XS_CFG_GET_FLOAT(xs_cfgtable[i].itemName,
                (gfloat *) xs_cfgtable[i].itemData);
            break;

        case CTYPE_STR:
            if (XS_CFG_GET_STRING(xs_cfgtable[i].itemName,
                (gchar **) &tmpStr)) {
                xs_pstrcpy((gchar **) xs_cfgtable[i].itemData, tmpStr);
                g_free(tmpStr);
            }
            break;
        }
    }

    /* Filters and presets are a special case */
    xs_filter_load_into(cfg, 0, &xs_cfg.sid2Filter);

    if (xs_cfg.sid2NFilterPresets > 0) {
        xs_cfg.sid2FilterPresets = g_malloc0(xs_cfg.sid2NFilterPresets * sizeof(xs_sid_filter_t *));
        if (!xs_cfg.sid2FilterPresets) {
            xs_error("Allocation of sid2FilterPresets structure failed!\n");
        } else {
            for (i = 0; i < xs_cfg.sid2NFilterPresets; i++) {
                xs_cfg.sid2FilterPresets[i] = xs_filter_load(cfg, i);
            }
        }
    }

    XS_CONFIG_FREE(cfg);

    XS_MUTEX_UNLOCK(xs_cfg);
    XSDEBUG("OK\n");
}


/* Write the current configuration
 */
gint xs_write_configuration(void)
{
    XS_CONFIG_FILE *cfg;
    gint i;

    XSDEBUG("writing configuration ...\n");
    XS_MUTEX_LOCK(xs_cfg);

    /* Try to open the XMMS configuration file  */
    cfg = XS_CONFIG_OPEN();

#ifndef AUDACIOUS_PLUGIN
    if (!cfg) cfg = xmms_cfg_new();
    if (!cfg) return -1;
#endif

    /* Write the new settings to XMMS configuration file */
    for (i = 0; i < xs_cfgtable_max; i++) {
        switch (xs_cfgtable[i].itemType) {
        case CTYPE_INT:
            XS_CFG_SET_INT(xs_cfgtable[i].itemName,
                *(gint *) xs_cfgtable[i].itemData);
            break;

        case CTYPE_BOOL:
            XS_CFG_SET_BOOL(xs_cfgtable[i].itemName,
                *(gboolean *) xs_cfgtable[i].itemData);
            break;

        case CTYPE_FLOAT:
            XS_CFG_SET_FLOAT(xs_cfgtable[i].itemName,
                *(gfloat *) xs_cfgtable[i].itemData);
            break;

        case CTYPE_STR:
            XS_CFG_SET_STRING(xs_cfgtable[i].itemName,
                *(gchar **) xs_cfgtable[i].itemData);
            break;
        }
    }


#ifndef AUDACIOUS_PLUGIN
    xmms_cfg_write_default_file(cfg);
#endif
    XS_CONFIG_FREE(cfg);

    XS_MUTEX_UNLOCK(xs_cfg);

    return 0;
}


#if 0
/* Configuration panel was canceled
 */
XS_DEF_WINDOW_CLOSE(cfg_cancel, configwin)


/* Configuration was accepted, save the settings
 */
void xs_cfg_ok(void)
{
    gint i;
    gfloat tmpValue;
    gint tmpInt;
    const gchar *tmpStr;

    if (aud_drct_get_playing ())
        aud_drct_stop ();

    /* Get lock on configuration */
    XS_MUTEX_LOCK(xs_cfg);

    XSDEBUG("get data from widgets to config...\n");

    for (i = 0; i < xs_widtable_max; i++) {
        switch (xs_widtable[i].widType) {
        case WTYPE_BGROUP:
            /* Check if toggle-button is active */
            if (GTK_TOGGLE_BUTTON(LUW(xs_widtable[i].widName))->active) {
                /* Yes, set the constant value */
                *((gint *) xs_widtable[i].itemData) = xs_widtable[i].itemSet;
            }
            break;

        case WTYPE_COMBO:
            /* Get text from text-widget */
            tmpStr = gtk_entry_get_text(GTK_ENTRY(LUW(xs_widtable[i].widName)));
            if (sscanf(tmpStr, "%d", &tmpInt) != 1)
                tmpInt = xs_widtable[i].itemSet;

            *((gint *) xs_widtable[i].itemData) = tmpInt;
            break;

        case WTYPE_SPIN:
        case WTYPE_SCALE:
            /* Get the value */
            switch (xs_widtable[i].widType) {
            case WTYPE_SPIN:
                tmpValue = gtk_spin_button_get_adjustment(GTK_SPIN_BUTTON(LUW(xs_widtable[i].widName)))->value;
                break;

            case WTYPE_SCALE:
                tmpValue = gtk_range_get_adjustment(GTK_RANGE(LUW(xs_widtable[i].widName)))->value;
                break;

            default:
                tmpValue = -1;
                break;
            }

            /* Set the value */
            switch (xs_widtable[i].itemType) {
            case CTYPE_INT:
                *((gint *) xs_widtable[i].itemData) = (gint) tmpValue;
                break;

            case CTYPE_FLOAT:
                *((gfloat *) xs_widtable[i].itemData) = tmpValue;
                break;
            }
            break;

        case WTYPE_BUTTON:
            /* Check if toggle-button is active */
            *((gboolean *) xs_widtable[i].itemData) =
                (GTK_TOGGLE_BUTTON(LUW(xs_widtable[i].widName))->active);
            break;

        case WTYPE_TEXT:
            /* Get text from text-widget */
            xs_pstrcpy((gchar **) xs_widtable[i].itemData,
                gtk_entry_get_text(GTK_ENTRY(LUW(xs_widtable[i].widName))));
            break;
        }
    }

    /* Get filter settings */
    /*
    if (!xs_curve_get_points(XS_CURVE(LUW("")), &xs_cfg.sid2Filter.points, &xs_cfg.sid2Filter.npoints)) {
        xs_error("Warning: Could not get filter curve widget points!\n");
    }
    */

    /* Release lock */
    XS_MUTEX_UNLOCK(xs_cfg);

    /* Close window */
    gtk_widget_destroy(xs_configwin);
    xs_configwin = NULL;

    /* Write settings */
    xs_write_configuration();

    /* Re-initialize */
    xs_reinit();
}


/* Confirmation window
 */
gboolean xs_confirmwin_delete(GtkWidget *widget, GdkEvent *event, gpointer user_data)
{
    (void) widget;
    (void) event;
    (void) user_data;

    return FALSE;
}



/* HVSC songlength-database file selector response-functions
 */
void xs_cfg_sldb_browse(GtkButton * button, gpointer user_data)
{
    (void) button;
    (void) user_data;

    if (xs_sldb_fileselector != NULL) {
        XS_WINDOW_PRESENT(xs_sldb_fileselector);
        return;
    }

    xs_sldb_fileselector = create_xs_sldb_fs();
    XS_MUTEX_LOCK(xs_cfg);
    gtk_file_selection_set_filename(GTK_FILE_SELECTION(xs_sldb_fileselector), xs_cfg.songlenDBPath);
    XS_MUTEX_UNLOCK(xs_cfg);
    gtk_widget_show(xs_sldb_fileselector);
}


void xs_sldb_fs_ok(GtkButton *button, gpointer user_data)
{
    (void) button;
    (void) user_data;

    /* Selection was accepted! */
    gtk_entry_set_text(GTK_ENTRY(LUW("cfg_sld_dbpath")),
               gtk_file_selection_get_filename(GTK_FILE_SELECTION(xs_sldb_fileselector)));

    /* Close file selector window */
    gtk_widget_destroy(xs_sldb_fileselector);
    xs_sldb_fileselector = NULL;
}

XS_DEF_WINDOW_CLOSE(sldb_fs_cancel, sldb_fileselector)
XS_DEF_WINDOW_DELETE(sldb_fs, sldb_fileselector)


/* STIL-database file selector response-functions
 */
void xs_cfg_stil_browse(GtkButton * button, gpointer user_data)
{
    (void) button;
    (void) user_data;

    if (xs_stil_fileselector != NULL) {
        XS_WINDOW_PRESENT(xs_stil_fileselector);
        return;
    }

    xs_stil_fileselector = create_xs_stil_fs();
    XS_MUTEX_LOCK(xs_cfg);
    gtk_file_selection_set_filename(GTK_FILE_SELECTION(xs_stil_fileselector), xs_cfg.stilDBPath);
    XS_MUTEX_UNLOCK(xs_cfg);
    gtk_widget_show(xs_stil_fileselector);
}


void xs_stil_fs_ok(GtkButton *button, gpointer user_data)
{
    (void) button;
    (void) user_data;

    /* Selection was accepted! */
    gtk_entry_set_text(GTK_ENTRY(LUW("cfg_stil_dbpath")),
        gtk_file_selection_get_filename(GTK_FILE_SELECTION(xs_stil_fileselector)));

    /* Close file selector window */
    gtk_widget_destroy(xs_stil_fileselector);
    xs_stil_fileselector = NULL;
}


XS_DEF_WINDOW_CLOSE(stil_fs_cancel, stil_fileselector)
XS_DEF_WINDOW_DELETE(stil_fs, stil_fileselector)


/* HVSC location selector response-functions
 */
void xs_cfg_hvsc_browse(GtkButton * button, gpointer user_data)
{
    (void) button;
    (void) user_data;

    if (xs_hvsc_selector != NULL) {
        XS_WINDOW_PRESENT(xs_hvsc_selector);
        return;
    }

    xs_hvsc_selector = create_xs_hvsc_fs();
    XS_MUTEX_LOCK(xs_cfg);
    gtk_file_selection_set_filename(GTK_FILE_SELECTION(xs_hvsc_selector), xs_cfg.hvscPath);
    XS_MUTEX_UNLOCK(xs_cfg);
    gtk_widget_show(xs_hvsc_selector);
}


void xs_hvsc_fs_ok(GtkButton *button, gpointer user_data)
{
    (void) button;
    (void) user_data;

    /* Selection was accepted! */
    gtk_entry_set_text(GTK_ENTRY(LUW("cfg_hvsc_path")),
        gtk_file_selection_get_filename(GTK_FILE_SELECTION(xs_hvsc_selector)));

    /* Close file selector window */
    gtk_widget_destroy(xs_hvsc_selector);
    xs_hvsc_selector = NULL;
}


XS_DEF_WINDOW_CLOSE(hvsc_fs_cancel, hvsc_selector)
XS_DEF_WINDOW_DELETE(hvsc_fs, hvsc_selector)


/* Filter handling
 */
void xs_cfg_sp1_filter_reset(GtkButton * button, gpointer user_data)
{
    (void) button;
    (void) user_data;

    gtk_adjustment_set_value(gtk_range_get_adjustment(GTK_RANGE(LUW("cfg_sp1_filter_fs"))), XS_SIDPLAY1_FS);
    gtk_adjustment_set_value(gtk_range_get_adjustment(GTK_RANGE(LUW("cfg_sp1_filter_fm"))), XS_SIDPLAY1_FM);
    gtk_adjustment_set_value(gtk_range_get_adjustment(GTK_RANGE(LUW("cfg_sp1_filter_ft"))), XS_SIDPLAY1_FT);
}


void xs_cfg_sp2_filter_update(XSCurve *curve, xs_sid_filter_t *f)
{
    assert(curve);
    assert(f);

    xs_curve_reset(curve);
    xs_curve_set_range(curve, 0,0, XS_SIDPLAY2_NFPOINTS, XS_SIDPLAY2_FMAX);
    if (!xs_curve_set_points(curve, f->points, f->npoints)) {
        // FIXME
        xs_error("Warning: Could not set filter curve widget points!\n");
    }
}


void xs_cfg_sp2_presets_update(void)
{
    GList *tmpList = NULL;
    gint i;

    for (i = 0; i < xs_cfg.sid2NFilterPresets; i++) {
        tmpList = g_list_append(tmpList,
            (gpointer) xs_cfg.sid2FilterPresets[i]->name);
    }

    gtk_combo_set_popdown_strings(GTK_COMBO(LUW("cfg_sp2_filter_combo")), tmpList);
    g_list_free(tmpList);
}


void xs_cfg_sp2_filter_load(GtkButton *button, gpointer user_data)
{
    const gchar *tmpStr;
    gint i, j;

    (void) button;
    (void) user_data;

    XS_MUTEX_LOCK(xs_cfg);

    tmpStr = gtk_entry_get_text(GTK_ENTRY(LUW("cfg_sp2_filter_combo_entry")));
    for (i = 0, j = -1; i < xs_cfg.sid2NFilterPresets; i++) {
        if (!strcmp(tmpStr, xs_cfg.sid2FilterPresets[i]->name)) {
            j = i;
            break;
        }
    }

    if (j != -1) {
        fprintf(stderr, "Updating from '%s'\n", tmpStr);
        xs_cfg_sp2_filter_update(
            XS_CURVE(LUW("cfg_sp2_filter_curve")),
            xs_cfg.sid2FilterPresets[i]);
    } else {
        /* error/warning: no such filter preset */
        fprintf(stderr, "No such filter preset '%s'!\n", tmpStr);
    }

    XS_MUTEX_UNLOCK(xs_cfg);
}


void xs_cfg_sp2_filter_save(GtkButton *button, gpointer user_data)
{
    /*
    1) check if textentry matches any current filter name
        yes) ask if saving over ok?
        no) ...

    2) save current filter to the name
    */
    const gchar *tmpStr;
    gint i, j;

    (void) button;
    (void) user_data;

    XS_MUTEX_LOCK(xs_cfg);

    tmpStr = gtk_entry_get_text(GTK_ENTRY(LUW("cfg_sp2_filter_combo_entry")));
    for (i = 0, j = -1; i < xs_cfg.sid2NFilterPresets; i++) {
        if (!strcmp(tmpStr, xs_cfg.sid2FilterPresets[i]->name)) {
            j = i;
            break;
        }
    }

    if (j != -1) {
        fprintf(stderr, "Found, confirm overwrite?\n");
    }

    fprintf(stderr, "saving!\n");

    xs_cfg_sp2_presets_update();

    XS_MUTEX_UNLOCK(xs_cfg);
}


void xs_cfg_sp2_filter_delete(GtkButton *button, gpointer user_data)
{
    (void) button;
    (void) user_data;
    /*
    1) confirm
    2) delete
    */
}


void xs_cfg_sp2_filter_import(GtkButton *button, gpointer user_data)
{
    (void) button;
    (void) user_data;

    if (xs_filt_importselector != NULL) {
        XS_WINDOW_PRESENT(xs_filt_importselector);
        return;
    }

    xs_filt_importselector = create_xs_filter_import_fs();
    gtk_widget_show(xs_filt_importselector);
}


void xs_filter_import_fs_ok(GtkButton *button, gpointer user_data)
{
    const gchar *tmpStr;
    (void) button;
    (void) user_data;

    XS_MUTEX_LOCK(xs_cfg);

    /* Selection was accepted! */
    tmpStr = gtk_file_selection_get_filename(GTK_FILE_SELECTION(xs_filt_importselector));
    xs_filters_import(tmpStr, xs_cfg.sid2FilterPresets, &xs_cfg.sid2NFilterPresets);
    xs_cfg_sp2_presets_update();

    /* Close file selector window */
    gtk_widget_destroy(xs_filt_importselector);
    xs_filt_importselector = NULL;
    XS_MUTEX_UNLOCK(xs_cfg);
}


XS_DEF_WINDOW_CLOSE(filter_import_fs_cancel, filt_importselector)
XS_DEF_WINDOW_DELETE(filter_import_fs, filt_importselector)


void xs_cfg_sp2_filter_export(GtkButton *button, gpointer user_data)
{
    (void) button;
    (void) user_data;

    if (xs_filt_exportselector != NULL) {
        XS_WINDOW_PRESENT(xs_filt_exportselector);
        return;
    }

    xs_filt_exportselector = create_xs_filter_export_fs();
    gtk_widget_show(xs_filt_exportselector);
}


void xs_filter_export_fs_ok(GtkButton *button, gpointer user_data)
{
    const gchar *tmpStr;
    (void) button;
    (void) user_data;

    XS_MUTEX_LOCK(xs_cfg);

    /* Selection was accepted! */
    tmpStr = gtk_file_selection_get_filename(GTK_FILE_SELECTION(xs_filt_exportselector));
    xs_filters_export(tmpStr, xs_cfg.sid2FilterPresets, xs_cfg.sid2NFilterPresets);

    /* Close file selector window */
    gtk_widget_destroy(xs_filt_exportselector);
    xs_filt_exportselector = NULL;
    XS_MUTEX_UNLOCK(xs_cfg);
}


XS_DEF_WINDOW_CLOSE(filter_export_fs_cancel, filt_exportselector)
XS_DEF_WINDOW_DELETE(filter_export_fs, filt_exportselector)


/* Selection toggle handlers
 */
void xs_cfg_emu_filters_toggled(GtkToggleButton * togglebutton, gpointer user_data)
{
    gboolean isActive = GTK_TOGGLE_BUTTON(togglebutton)->active;

    (void) user_data;

    gtk_widget_set_sensitive(LUW("cfg_filters_notebook"), isActive);
}


void xs_cfg_ftitle_override_toggled(GtkToggleButton * togglebutton, gpointer user_data)
{
    gboolean isActive = GTK_TOGGLE_BUTTON(togglebutton)->active;

    (void) user_data;

    gtk_widget_set_sensitive(LUW("cfg_ftitle_box"), isActive);
}


void xs_cfg_emu_sidplay1_toggled(GtkToggleButton * togglebutton, gpointer user_data)
{
    (void) togglebutton;
    (void) user_data;
}


void xs_cfg_emu_sidplay2_toggled(GtkToggleButton * togglebutton, gpointer user_data)
{
    gboolean isActive = GTK_TOGGLE_BUTTON(togglebutton)->active;

    (void) user_data;

    gtk_widget_set_sensitive(LUW("cfg_emu_mem_real"), isActive);

    gtk_widget_set_sensitive(LUW("cfg_sidplay2_frame"), isActive);

#ifdef HAVE_SIDPLAY2_DISTORTION
    /* Optimization mode removed from distortion patch */
    gtk_widget_set_sensitive(LUW("cfg_emu_sp2_opt"), FALSE);
#else
    gtk_widget_set_sensitive(LUW("cfg_emu_sp2_opt"), isActive);
#endif

    gtk_widget_set_sensitive(LUW("cfg_chn_autopan"), !isActive);

#ifdef HAVE_RESID_BUILDER
    gtk_widget_set_sensitive(LUW("cfg_emu_sp2_resid"), isActive);
#else
    gtk_widget_set_sensitive(LUW("cfg_emu_sp2_resid"), FALSE);
#endif

#ifdef HAVE_HARDSID_BUILDER
    gtk_widget_set_sensitive(LUW("cfg_emu_sp2_hardsid"), isActive);
#else
    gtk_widget_set_sensitive(LUW("cfg_emu_sp2_hardsid"), FALSE);
#endif
}


void xs_cfg_oversample_toggled(GtkToggleButton * togglebutton, gpointer user_data)
{
    gboolean isActive = GTK_TOGGLE_BUTTON(togglebutton)->active;

    (void) user_data;

    gtk_widget_set_sensitive(LUW("cfg_oversample_box"), isActive);
}


void xs_cfg_mintime_enable_toggled(GtkToggleButton * togglebutton, gpointer user_data)
{
    gboolean isActive = GTK_TOGGLE_BUTTON(togglebutton)->active;

    (void) user_data;

    gtk_widget_set_sensitive(LUW("cfg_mintime_box"), isActive);
}


void xs_cfg_maxtime_enable_toggled(GtkToggleButton * togglebutton, gpointer user_data)
{
    gboolean isActive = GTK_TOGGLE_BUTTON(LUW("cfg_maxtime_enable"))->active;

    (void) togglebutton;
    (void) user_data;

    gtk_widget_set_sensitive(LUW("cfg_maxtime_unknown"), isActive);
    gtk_widget_set_sensitive(LUW("cfg_maxtime_box"), isActive);
}


void xs_cfg_sldb_enable_toggled(GtkToggleButton * togglebutton, gpointer user_data)
{
    gboolean isActive = GTK_TOGGLE_BUTTON(togglebutton)->active;

    (void) user_data;

    gtk_widget_set_sensitive(LUW("cfg_sld_box"), isActive);
}


void xs_cfg_stil_enable_toggled(GtkToggleButton * togglebutton, gpointer user_data)
{
    gboolean isActive = GTK_TOGGLE_BUTTON(togglebutton)->active;

    (void) user_data;

    gtk_widget_set_sensitive(LUW("cfg_stil_box1"), isActive);
    gtk_widget_set_sensitive(LUW("cfg_stil_box2"), isActive);
}


void xs_cfg_subauto_enable_toggled(GtkToggleButton * togglebutton, gpointer user_data)
{
    gboolean isActive = GTK_TOGGLE_BUTTON(togglebutton)->active;

    (void) user_data;

    gtk_widget_set_sensitive(LUW("cfg_subauto_min_only"), isActive);
    gtk_widget_set_sensitive(LUW("cfg_subauto_box"), isActive);
}


void xs_cfg_subauto_min_only_toggled(GtkToggleButton * togglebutton, gpointer user_data)
{
    gboolean isActive = GTK_TOGGLE_BUTTON(togglebutton)->active &&
        GTK_TOGGLE_BUTTON(LUW("cfg_subauto_enable"))->active;

    (void) user_data;

    gtk_widget_set_sensitive(LUW("cfg_subauto_box"), isActive);
}


void xs_cfg_mintime_changed(GtkEditable * editable, gpointer user_data)
{
    gint tmpValue;
    GtkAdjustment *tmpAdj;

    (void) user_data;

    tmpAdj = gtk_spin_button_get_adjustment(GTK_SPIN_BUTTON(LUW("cfg_maxtime")));

    tmpValue = (gint) gtk_spin_button_get_adjustment(GTK_SPIN_BUTTON(editable))->value;

    if (tmpValue > tmpAdj->value)
        gtk_adjustment_set_value(tmpAdj, tmpValue);
}


void xs_cfg_maxtime_changed(GtkEditable * editable, gpointer user_data)
{
    gint tmpValue;
    GtkAdjustment *tmpAdj;

    (void) user_data;

    tmpAdj = gtk_spin_button_get_adjustment(GTK_SPIN_BUTTON(LUW("cfg_mintime")));

    tmpValue = (gint) gtk_spin_button_get_adjustment(GTK_SPIN_BUTTON(editable))->value;

    if (tmpValue < tmpAdj->value)
        gtk_adjustment_set_value(tmpAdj, tmpValue);
}


XS_DEF_WINDOW_DELETE(configwin, configwin)


/* Execute the configuration panel
 */
void xs_configure(void)
{
    gint i;
    gfloat tmpValue;
    gchar tmpStr[64];
    GList *tmpList = NULL;
    GtkWidget *tmpCurve;

    /* Check if the window already exists */
    if (xs_configwin) {
        XS_WINDOW_PRESENT(xs_configwin);
        return;
    }

    /* Create the window */
    xs_configwin = create_xs_configwin();

    /* Get lock on configuration */
    XS_MUTEX_LOCK(xs_cfg);

    /* Add samplerates */
    for (i = 0; i < xs_nsamplerates_table; i++) {
        tmpList = g_list_append (tmpList,
            (gpointer) xs_samplerates_table[i]);
    }
    gtk_combo_set_popdown_strings(GTK_COMBO(LUW("cfg_samplerate_combo")), tmpList);
    g_list_free(tmpList);

    /* Create the custom filter curve widget for libSIDPlay2 */
    xs_cfg_sp2_presets_update();
    tmpCurve = xs_curve_new();
    xs_cfg_sp2_filter_update(XS_CURVE(tmpCurve), &xs_cfg.sid2Filter);
    gtk_widget_set_name(tmpCurve, "cfg_sp2_filter_curve");
    gtk_widget_ref(tmpCurve);
    gtk_object_set_data_full(GTK_OBJECT(xs_configwin),
        "cfg_sp2_filter_curve", tmpCurve, (GtkDestroyNotify) gtk_widget_unref);
    gtk_widget_show(tmpCurve);
    gtk_container_add(GTK_CONTAINER(LUW("cfg_sp2_filter_frame")), tmpCurve);


    /* Based on available optional parts, gray out options */
#ifndef HAVE_SIDPLAY1
    gtk_widget_set_sensitive(LUW("cfg_emu_sidplay1"), FALSE);
    gtk_widget_set_sensitive(LUW("cfg_box_filter_sidplay1"), FALSE);
#endif

#ifndef HAVE_SIDPLAY2
    gtk_widget_set_sensitive(LUW("cfg_emu_sidplay2"), FALSE);
    gtk_widget_set_sensitive(LUW("cfg_box_filter_sidplay2"), FALSE);
#endif

    gtk_widget_set_sensitive(LUW("cfg_resid_frame"), FALSE);

#if !defined(HAVE_XMMSEXTRA) && !defined(AUDACIOUS_PLUGIN)
    gtk_widget_set_sensitive(LUW("cfg_ftitle_override"), FALSE);
    xs_cfg.titleOverride = TRUE;
#endif

#if !defined(HAVE_SONG_POSITION) && !defined(AUDACIOUS_PLUGIN)
    gtk_widget_set_sensitive(LUW("cfg_subctrl_patch"), FALSE);
#endif

    xs_cfg_ftitle_override_toggled(GTK_TOGGLE_BUTTON(LUW("cfg_ftitle_override")), NULL);
    xs_cfg_emu_filters_toggled(GTK_TOGGLE_BUTTON(LUW("cfg_emu_filters")), NULL);
    xs_cfg_emu_sidplay1_toggled(GTK_TOGGLE_BUTTON(LUW("cfg_emu_sidplay1")), NULL);
    xs_cfg_emu_sidplay2_toggled(GTK_TOGGLE_BUTTON(LUW("cfg_emu_sidplay2")), NULL);
    xs_cfg_oversample_toggled(GTK_TOGGLE_BUTTON(LUW("cfg_oversample")), NULL);
    xs_cfg_mintime_enable_toggled(GTK_TOGGLE_BUTTON(LUW("cfg_mintime_enable")), NULL);
    xs_cfg_maxtime_enable_toggled(GTK_TOGGLE_BUTTON(LUW("cfg_maxtime_enable")), NULL);
    xs_cfg_sldb_enable_toggled(GTK_TOGGLE_BUTTON(LUW("cfg_sld_enable")), NULL);
    xs_cfg_stil_enable_toggled(GTK_TOGGLE_BUTTON(LUW("cfg_stil_enable")), NULL);
    xs_cfg_subauto_enable_toggled(GTK_TOGGLE_BUTTON(LUW("cfg_subauto_enable")), NULL);
    xs_cfg_subauto_min_only_toggled(GTK_TOGGLE_BUTTON(LUW("cfg_subauto_min_only")), NULL);


    /* Set current data to widgets */
    for (i = 0; i < xs_widtable_max; i++) {
        switch (xs_widtable[i].widType) {
        case WTYPE_BGROUP:
            assert(xs_widtable[i].itemType == CTYPE_INT);
            /* Check if current value matches the given one */
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(LUW(xs_widtable[i].widName)),
                (*((gint *) xs_widtable[i].itemData) == xs_widtable[i].itemSet));
            break;

        case WTYPE_COMBO:
            assert(xs_widtable[i].itemType == CTYPE_INT);
            g_snprintf(tmpStr, sizeof(tmpStr), "%d", *(gint *) xs_widtable[i].itemData);
            gtk_entry_set_text(GTK_ENTRY(LUW(xs_widtable[i].widName)), tmpStr);
            break;

        case WTYPE_SPIN:
        case WTYPE_SCALE:
            /* Get the value */
            switch (xs_widtable[i].itemType) {
            case CTYPE_INT:
                tmpValue = (gfloat) * ((gint *) xs_widtable[i].itemData);
                break;

            case CTYPE_FLOAT:
                tmpValue = *((gfloat *) xs_widtable[i].itemData);
                break;

            default:
                tmpValue = -1;
                assert(0);
                break;
            }

            /* Set the value */
            switch (xs_widtable[i].widType) {
            case WTYPE_SPIN:
                gtk_adjustment_set_value(gtk_spin_button_get_adjustment
                    (GTK_SPIN_BUTTON(LUW(xs_widtable[i].widName))), tmpValue);
                break;

            case WTYPE_SCALE:
                gtk_adjustment_set_value(gtk_range_get_adjustment
                    (GTK_RANGE(LUW(xs_widtable[i].widName))), tmpValue);
                break;
            }
            break;

        case WTYPE_BUTTON:
            assert(xs_widtable[i].itemType == CTYPE_BOOL);
            /* Set toggle-button */
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(LUW(xs_widtable[i].widName)),
                *((gboolean *) xs_widtable[i].itemData));
            break;

        case WTYPE_TEXT:
            assert(xs_widtable[i].itemType == CTYPE_STR);
            /* Set text to text-widget */
            if (*(gchar **) xs_widtable[i].itemData != NULL) {
                gtk_entry_set_text(GTK_ENTRY(LUW(xs_widtable[i].widName)),
                *(gchar **) xs_widtable[i].itemData);
            }
            break;
        }
    }

    /* Release the configuration */
    XS_MUTEX_UNLOCK(xs_cfg);

    /* Show the widget */
    gtk_widget_show(xs_configwin);
}
#endif
