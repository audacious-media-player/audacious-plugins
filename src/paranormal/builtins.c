#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

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
  /* **************** the end! **************** */
  NULL
};
