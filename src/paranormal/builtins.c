/*
 * paranormal: iterated pipeline-driven visualization plugin
 * Copyright (c) 2006, 2007 William Pitcock <nenolod@dereferenced.org>
 * Portions copyright (c) 2001 Jamie Gennis <jgennis@mindspring.com>
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

#include <config.h>

#include "actuators.h"

#define DECLARE_ACTUATOR(a) extern struct pn_actuator_desc builtin_##a;

/* **************** containers **************** */

DECLARE_ACTUATOR (container_simple);
DECLARE_ACTUATOR (container_once);
DECLARE_ACTUATOR (container_cycle);
DECLARE_ACTUATOR (container_onbeat);

/* **************** cmaps **************** */

DECLARE_ACTUATOR (cmap_bwgradient);
DECLARE_ACTUATOR (cmap_gradient);
DECLARE_ACTUATOR (cmap_dynamic);

/* **************** freq **************** */
DECLARE_ACTUATOR (freq_dots);
DECLARE_ACTUATOR (freq_drops);

/* **************** general **************** */
DECLARE_ACTUATOR (general_fade);
DECLARE_ACTUATOR (general_blur);
DECLARE_ACTUATOR (general_mosaic);
DECLARE_ACTUATOR (general_clear);
DECLARE_ACTUATOR (general_noop);
DECLARE_ACTUATOR (general_invert);
DECLARE_ACTUATOR (general_replace);
DECLARE_ACTUATOR (general_swap);
DECLARE_ACTUATOR (general_copy);
DECLARE_ACTUATOR (general_flip);
DECLARE_ACTUATOR (general_evaluate);

/* **************** misc **************** */
DECLARE_ACTUATOR (misc_floater);

/* **************** wave **************** */
DECLARE_ACTUATOR (wave_horizontal);
DECLARE_ACTUATOR (wave_vertical);
DECLARE_ACTUATOR (wave_normalize);
DECLARE_ACTUATOR (wave_smooth);
DECLARE_ACTUATOR (wave_radial);
DECLARE_ACTUATOR (wave_scope);

/* **************** xform **************** */
DECLARE_ACTUATOR (xform_spin);
DECLARE_ACTUATOR (xform_ripple);
DECLARE_ACTUATOR (xform_bump_spin);
DECLARE_ACTUATOR (xform_halfrender);
DECLARE_ACTUATOR (xform_movement);
DECLARE_ACTUATOR (xform_dynmovement);

/* **************** builtin_table **************** */
struct pn_actuator_desc *builtin_table[] =
{
  /* **************** containers **************** */
  &builtin_container_simple,
  &builtin_container_once,
  &builtin_container_cycle,
  &builtin_container_onbeat,

  /* **************** cmaps **************** */
  &builtin_cmap_bwgradient,
  &builtin_cmap_gradient,
  &builtin_cmap_dynamic,
  /* **************** freq **************** */
  &builtin_freq_dots,
  &builtin_freq_drops,
  /* **************** general **************** */
  &builtin_general_fade,
  &builtin_general_blur,
  &builtin_general_mosaic,
  &builtin_general_clear,
  &builtin_general_noop,
  &builtin_general_invert,
  &builtin_general_replace,
  &builtin_general_swap,
  &builtin_general_copy,
  &builtin_general_flip,
  &builtin_general_evaluate,
  /* **************** misc **************** */
  &builtin_misc_floater,
  /* **************** wave **************** */
  &builtin_wave_horizontal,
  &builtin_wave_vertical,
  &builtin_wave_normalize,
  &builtin_wave_smooth,
  &builtin_wave_radial,
  &builtin_wave_scope,
  /* **************** xform **************** */
  &builtin_xform_spin,
  &builtin_xform_ripple,
  &builtin_xform_bump_spin,
  &builtin_xform_halfrender,
  &builtin_xform_movement,
  &builtin_xform_dynmovement,
  /* **************** the end! **************** */
  NULL
};
