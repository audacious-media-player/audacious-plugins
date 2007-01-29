/* ghosd -- OSD with fake transparency, cairo, and pango.
 * Copyright (C) 2006 Evan Martin <martine@danga.com>
 */

#ifndef __GHOSD_H__
#define __GHOSD_H__

#include <cairo/cairo.h>

#include <values.h>  /* MAXINT */
#include <sys/time.h>  /* timeval */

typedef struct _Ghosd Ghosd;

typedef void (*GhosdRenderFunc)(Ghosd *ghosd, cairo_t *cr, void *user_data);

Ghosd *ghosd_new(void);
void   ghosd_destroy(Ghosd* ghosd);

#define GHOSD_COORD_CENTER MAXINT
void ghosd_set_transparent(Ghosd *ghosd, int transparent);
void ghosd_set_position(Ghosd *ghosd, int x, int y, int width, int height);
void ghosd_set_render(Ghosd *ghosd, GhosdRenderFunc render_func,
                      void* user_data, void (*user_data_d)(void*));

void ghosd_render(Ghosd *ghosd);
void ghosd_show(Ghosd *ghosd);
void ghosd_hide(Ghosd *ghosd);

void ghosd_main_iterations(Ghosd *ghosd);
void ghosd_main_until(Ghosd *ghosd, struct timeval *until);
void ghosd_flash(Ghosd *ghosd, int fade_ms, int total_display_ms);

int ghosd_get_socket(Ghosd *ghosd);

#endif /* __GHOSD_H__ */

/* vim: set ts=2 sw=2 et cino=(0 : */
