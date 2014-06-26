/*
 * Audacious - a cross-platform multimedia player
 * Copyright (c) 2007-2011  Audacious development team.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; under version 3 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses>.
 *
 * The Audacious team does not consider modular code linking to
 * Audacious or using our public API to be a derived work.
 */

#ifndef SKINS_UI_VIS_H
#define SKINS_UI_VIS_H

#include <gtk/gtk.h>

typedef enum {
    VIS_ANALYZER, VIS_SCOPE, VIS_VOICEPRINT, VIS_OFF
} VisType;

typedef enum {
    ANALYZER_NORMAL, ANALYZER_FIRE, ANALYZER_VLINES
} AnalyzerMode;

typedef enum {
    ANALYZER_LINES, ANALYZER_BARS
} AnalyzerType;

typedef enum {
    SCOPE_DOT, SCOPE_LINE, SCOPE_SOLID
} ScopeMode;

typedef enum {
    VOICEPRINT_NORMAL, VOICEPRINT_FIRE, VOICEPRINT_ICE
} VoiceprintMode;

typedef enum {
    VU_NORMAL, VU_SMOOTH
} VUMode;

typedef enum {
    FALLOFF_SLOWEST, FALLOFF_SLOW, FALLOFF_MEDIUM, FALLOFF_FAST, FALLOFF_FASTEST
} FalloffSpeed;

GtkWidget * ui_vis_new (void);
void ui_vis_set_colors (void);
void ui_vis_clear_data (GtkWidget * widget);
void ui_vis_timeout_func (GtkWidget * widget, unsigned char * data);

GtkWidget * ui_svis_new (void);
void ui_svis_clear_data (GtkWidget * widget);
void ui_svis_timeout_func (GtkWidget * widget, unsigned char * data);

#endif
