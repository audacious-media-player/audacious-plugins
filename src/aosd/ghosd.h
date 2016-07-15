/* ghosd -- OSD with fake transparency, cairo, and pango.
 * Copyright (C) 2006 Evan Martin <martine@danga.com>
 *
 * With further development by Giacomo Lozito <james@develia.org>
 * for the ghosd-based Audacious OSD
 * - added real transparency with X Composite Extension
 * - added mouse event handling on OSD window
 * - added/changed some other stuff
 */

#ifndef __GHOSD_H__
#define __GHOSD_H__

#include <cairo/cairo.h>

#include <limits.h>  /* INT_MAX */
#include <sys/time.h>  /* timeval */

typedef struct _Ghosd Ghosd;

/* minimal struct to handle button events */
typedef struct
{
  int x, y;
  int send_event;
  int x_root, y_root;
  unsigned int button;
  unsigned long time;
}
GhosdEventButton;

typedef void (*GhosdRenderFunc)(Ghosd *ghosd, cairo_t *cr, void *user_data);
typedef void (*GhosdEventButtonCb)(Ghosd *ghosd, GhosdEventButton *event, void *user_data);

#ifdef __cplusplus
extern "C" {
#endif

Ghosd *ghosd_new(void);
void   ghosd_destroy(Ghosd* ghosd);
Ghosd *ghosd_new_with_argbvisual(void);
int ghosd_check_composite_ext(void);
int ghosd_check_composite_mgr(void);

#define GHOSD_COORD_CENTER INT_MAX
void ghosd_set_transparent(Ghosd *ghosd, int transparent);
void ghosd_set_position(Ghosd *ghosd, int x, int y, int width, int height);
void ghosd_set_render(Ghosd *ghosd, GhosdRenderFunc render_func,
                      void* user_data, void (*user_data_d)(void*));

void ghosd_render(Ghosd *ghosd);
void ghosd_show(Ghosd *ghosd);
void ghosd_hide(Ghosd *ghosd);

void ghosd_set_event_button_cb(Ghosd *ghosd, GhosdEventButtonCb cb, void *user_data );

void ghosd_main_iterations(Ghosd *ghosd);
void ghosd_main_until(Ghosd *ghosd, struct timeval *until);
void ghosd_flash(Ghosd *ghosd, int fade_ms, int total_display_ms);

int ghosd_get_socket(Ghosd *ghosd);

#ifdef __cplusplus
}
#endif

#endif /* __GHOSD_H__ */

/* vim: set ts=2 sw=2 et cino=(0 : */
