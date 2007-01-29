/* ghosd -- OSD with fake transparency, cairo, and pango.
 * Copyright (C) 2006 Evan Martin <martine@danga.com>
 */

#ifndef __GHOSD_TEXT_H__
#define __GHOSD_TEXT_H__

#include <pango/pango-layout.h>

Ghosd* ghosd_text_new(const char *markup, int x, int y);

#endif /* __GHOSD_TEXT_H__ */

/* vim: set ts=2 sw=2 et cino=(0 : */
