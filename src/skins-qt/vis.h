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

#include <stdint.h>
#include "widget.h"

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

class SkinnedVis : public Widget
{
public:
    SkinnedVis ();
    void set_colors ();
    void clear ();
    void render (const unsigned char * data);

private:
    void draw (QPainter & cr);

    uint32_t m_voice_color[256];
    uint32_t m_voice_color_fire[256];
    uint32_t m_voice_color_ice[256];
    uint32_t m_pattern_fill[76 * 2];

    bool m_active, m_voiceprint_advance;
    float m_data[75], m_peak[75], m_peak_speed[75];
    unsigned char m_voiceprint_data[76 * 16];
};

class SmallVis : public Widget
{
public:
    SmallVis ();
    void clear ();
    void render (const unsigned char * data);

private:
    void draw (QPainter & cr);

    bool m_active;
    int m_data[75];
};

#endif
