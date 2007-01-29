/* ghosd -- OSD with fake transparency, cairo, and pango.
 * Copyright (C) 2006 Evan Martin <martine@danga.com>
 */

#include "config.h"

#include <pango/pangocairo.h>
#include "ghosd-internal.h"
#include "ghosd-text.h"

static void
render_text(Ghosd *ghosd, cairo_t *cr, void* data) {
  PangoLayout *layout = data;

  /* drop shadow! */
  cairo_set_source_rgba(cr, 0, 0, 0, 0.8);
  cairo_move_to(cr, 4, 4);
  pango_cairo_show_layout(cr, layout);

  /* and the actual text. */
  cairo_set_source_rgba(cr, 1, 1, 1, 1.0);
  cairo_move_to(cr, 0, 0);
  pango_cairo_show_layout(cr, layout);
}

Ghosd*
ghosd_text_new(const char *markup, int x, int y) {
  Ghosd *ghosd;
  PangoContext *context;
  PangoLayout *layout;
  PangoRectangle ink_rect;

  g_type_init();

  context = pango_cairo_font_map_create_context(
              PANGO_CAIRO_FONT_MAP(pango_cairo_font_map_get_default()));
  layout = pango_layout_new(context);

  pango_layout_set_markup(layout, markup, -1);

  pango_layout_get_pixel_extents(layout, &ink_rect, NULL);

  const int width = ink_rect.x + ink_rect.width+5;
  const int height = ink_rect.y + ink_rect.height+5;

  ghosd = ghosd_new();
  ghosd_set_position(ghosd, x, y, width, height);
  ghosd_set_render(ghosd, render_text, layout, g_object_unref);
  
  return ghosd;
}

/* vim: set ts=2 sw=2 et cino=(0 : */
