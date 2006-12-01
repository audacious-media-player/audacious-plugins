// Game_Music_Emu 0.5.1. http://www.slack.net/~ant/

// separate file to avoid linking all emulators if this is not used

#include "gme.h"

/* Copyright (C) 2006 Shay Green. This module is free software; you
can redistribute it and/or modify it under the terms of the GNU Lesser
General Public License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version. This
module is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more
details. You should have received a copy of the GNU Lesser General Public
License along with this module; if not, write to the Free Software Foundation,
Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA */

#include "blargg_source.h"

gme_type_t const* gme_type_list()
{
	static gme_type_t const gme_type_list_ [] =
	{
		gme_ay_type,
		gme_gbs_type,
		gme_gym_type,
		gme_hes_type,
		gme_kss_type,
		gme_nsf_type,
		gme_nsfe_type,
		gme_sap_type,
		gme_spc_type,
		gme_vgm_type,
		gme_vgz_type,
		0
	};
	return gme_type_list_;
}
