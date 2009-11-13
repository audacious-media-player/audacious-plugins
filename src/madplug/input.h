/*
 * mad plugin for audacious
 * Copyright (C) 2005-2007 William Pitcock, Yoshiki Yazawa
 *
 * Portions derived from xmms-mad:
 * Copyright (C) 2001-2002 Sam Clegg - See COPYING
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; under version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef INPUT_H
#define INPUT_H

#include "plugin.h"
gboolean input_init(struct mad_info_t *songinfo, const gchar * url, VFSFile *fd);
gboolean input_term(struct mad_info_t *songinfo);
gboolean input_get_info (struct mad_info_t * songinfo);
gint input_get_data(struct mad_info_t *songinfo, guchar * buffer,
                    gint buffer_size);
gchar *input_id3_get_string(struct id3_tag *tag, const gchar *frame_name);

#endif                          /* ! INPUT_H */
