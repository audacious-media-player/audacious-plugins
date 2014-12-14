/*
 * ui_infoarea.h
 * Copyright 2010-2012 William Pitcock and John Lindgren
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions, and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions, and the following disclaimer in the documentation
 *    provided with the distribution.
 *
 * This software is provided "as is" and without any warranty, express or
 * implied. In no event shall the authors be liable for any damages arising from
 * the use of this software.
 */

#ifndef __UI_INFOAREA_H
#define __UI_INFOAREA_H

#include <gtk/gtk.h>

GtkWidget * ui_infoarea_new ();
void ui_infoarea_show_vis (bool show);

#endif
