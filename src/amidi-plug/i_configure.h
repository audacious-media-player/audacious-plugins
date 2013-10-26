/*
*
* Author: Giacomo Lozito <james@develia.org>, (C) 2005-2006
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

#ifndef _I_CONFIGURE_H
#define _I_CONFIGURE_H 1

typedef struct
{
    int ap_opts_transpose_value;
    int ap_opts_drumshift_value;
    int ap_opts_length_precalc;
    int ap_opts_comments_extract;
    int ap_opts_lyrics_extract;
}
amidiplug_cfg_ap_t;

extern amidiplug_cfg_ap_t * amidiplug_cfg_ap;

void i_configure_gui (void);
void i_configure_cfg_ap_read (void);
void i_configure_cfg_ap_save (void);
void i_configure_cfg_ap_free (void);
void i_configure_cfg_backend_read (void);

#endif /* !_I_CONFIGURE_H */
