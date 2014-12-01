/*
*
* Author: Giacomo Lozito <james@develia.org>, (C) 2005-2007
*
* This program is free software; you can redistribute it and/or modify it
* under the terms of the GNU General Public License as published by the
* Free Software Foundation; either version 2 of the License, or (at your
* option) any later version.
*
* This program is distributed in the hope that it will be useful, but
* WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
* General Public License for more details.
*
* You should have received a copy of the GNU General Public License along
* with this program; if not, write to the Free Software Foundation, Inc.,
* 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
*
*/

#ifndef _I_AOSD_CFG_H
#define _I_AOSD_CFG_H 1

#include <libaudcore/objects.h>

/* in this release only one user font is supported */
#define AOSD_TEXT_FONTS_NUM 1

/* transparency mode values */
#define AOSD_MISC_TRANSPARENCY_FAKE 0
#define AOSD_MISC_TRANSPARENCY_REAL 1

#define AOSD_DECO_STYLE_MAX_COLORS 2

enum
{
  AOSD_DECO_STYLE_RECT,
  AOSD_DECO_STYLE_ROUNDRECT,
  AOSD_DECO_STYLE_CONCAVERECT,
  AOSD_DECO_STYLE_NONE,
  AOSD_NUM_DECO_STYLES
};

enum
{
  AOSD_POSITION_PLACEMENT_TOPLEFT = 1,
  AOSD_POSITION_PLACEMENT_TOP,
  AOSD_POSITION_PLACEMENT_TOPRIGHT,
  AOSD_POSITION_PLACEMENT_MIDDLELEFT,
  AOSD_POSITION_PLACEMENT_MIDDLE,
  AOSD_POSITION_PLACEMENT_MIDDLERIGHT,
  AOSD_POSITION_PLACEMENT_BOTTOMLEFT,
  AOSD_POSITION_PLACEMENT_BOTTOM,
  AOSD_POSITION_PLACEMENT_BOTTOMRIGHT
};

enum
{
  AOSD_TRIGGER_PB_START = 0,
  AOSD_TRIGGER_PB_TITLECHANGE,
  AOSD_TRIGGER_PB_PAUSEON,
  AOSD_TRIGGER_PB_PAUSEOFF,
  AOSD_NUM_TRIGGERS
};

typedef struct
{
  int red;
  int green;
  int blue;
  int alpha;
}
aosd_color_t;


/* config portion containing osd decoration information */
typedef struct
{
  int code;
  aosd_color_t colors[AOSD_DECO_STYLE_MAX_COLORS];
}
aosd_cfg_osd_decoration_t;


/* config portion containing osd text information */
typedef struct
{
  String fonts_name[AOSD_TEXT_FONTS_NUM];
  aosd_color_t fonts_color[AOSD_TEXT_FONTS_NUM];
  bool fonts_draw_shadow[AOSD_TEXT_FONTS_NUM];
  aosd_color_t fonts_shadow_color[AOSD_TEXT_FONTS_NUM];
}
aosd_cfg_osd_text_t;


/* config portion containing osd animation information */
typedef struct
{
  int timing_display;
  int timing_fadein;
  int timing_fadeout;
}
aosd_cfg_osd_animation_t;


/* config portion containing osd position information */
typedef struct
{
  int placement;
  int offset_x;
  int offset_y;
  int maxsize_width;
  int multimon_id;
}
aosd_cfg_osd_position_t;


/* config portion containing osd trigger information */
typedef struct
{
  int enabled[AOSD_NUM_TRIGGERS];
}
aosd_cfg_osd_trigger_t;


/* config portion containing osd miscellaneous information */
typedef struct
{
  int transparency_mode;
}
aosd_cfg_osd_misc_t;


/* config portion containing all information */
typedef struct
{
  aosd_cfg_osd_position_t position;
  aosd_cfg_osd_animation_t animation;
  aosd_cfg_osd_text_t text;
  aosd_cfg_osd_decoration_t decoration;
  aosd_cfg_osd_trigger_t trigger;
  aosd_cfg_osd_misc_t misc;
}
aosd_cfg_t;


/* API */
void aosd_cfg_load (aosd_cfg_t & cfg);
void aosd_cfg_save (const aosd_cfg_t & cfg);

extern aosd_cfg_t global_config;

#endif /* !_I_AOSD_CFG_H */
