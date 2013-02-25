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
  char * ap_seq_backend;
  int ap_opts_transpose_value;
  int ap_opts_drumshift_value;
  int ap_opts_length_precalc;
  int ap_opts_comments_extract;
  int ap_opts_lyrics_extract;
}
amidiplug_cfg_ap_t;

typedef struct
{
  char * alsa_seq_wports;
  int alsa_mixer_card_id;
  char * alsa_mixer_ctl_name;
  int alsa_mixer_ctl_id;
}
amidiplug_cfg_alsa_t;

typedef struct
{
  char * fsyn_soundfont_file;
  int fsyn_soundfont_load;
  int fsyn_synth_samplerate;
  int fsyn_synth_gain;
  int fsyn_synth_polyphony;
  int fsyn_synth_reverb;
  int fsyn_synth_chorus;
}
amidiplug_cfg_fsyn_t;

struct amidiplug_cfg_backend_s
{
  amidiplug_cfg_alsa_t * alsa;
  amidiplug_cfg_fsyn_t * fsyn;
};

typedef struct amidiplug_cfg_backend_s amidiplug_cfg_backend_t;

extern amidiplug_cfg_ap_t * amidiplug_cfg_ap;
extern amidiplug_cfg_backend_t * amidiplug_cfg_backend;

void i_configure_gui (void);
void i_configure_cfg_ap_read (void);
void i_configure_cfg_ap_save (void);
void i_configure_cfg_ap_free (void);
void i_configure_cfg_backend_alloc (void);
void i_configure_cfg_backend_read (void);
void i_configure_cfg_backend_save (void);
void i_configure_cfg_backend_free (void);

#endif /* !_I_CONFIGURE_H */
