#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include "actuators.h"

#define DECLARE_ACTUATOR(a) extern struct pn_actuator_desc builtin_##a;

/* **************** containers **************** */

DECLARE_ACTUATOR (container_simple);
DECLARE_ACTUATOR (container_once);
DECLARE_ACTUATOR (container_cycle);

/* **************** cmaps **************** */

DECLARE_ACTUATOR (cmap_bwgradient);
DECLARE_ACTUATOR (cmap_gradient);

/* **************** freq **************** */
DECLARE_ACTUATOR (freq_dots);
DECLARE_ACTUATOR (freq_drops);

/* **************** general **************** */
DECLARE_ACTUATOR (general_fade);
DECLARE_ACTUATOR (general_blur);

/* **************** wave **************** */
DECLARE_ACTUATOR (wave_horizontal);
DECLARE_ACTUATOR (wave_vertical);
DECLARE_ACTUATOR (wave_normalize);
DECLARE_ACTUATOR (wave_smooth);
DECLARE_ACTUATOR (wave_radial);

/* **************** xform **************** */
DECLARE_ACTUATOR (xform_spin);
DECLARE_ACTUATOR (xform_ripple);
DECLARE_ACTUATOR (xform_bump_spin);

/* **************** builtin_table **************** */
struct pn_actuator_desc *builtin_table[] =
{
  /* **************** containers **************** */
  &builtin_container_simple,
  &builtin_container_once,
  &builtin_container_cycle,
  /* **************** cmaps **************** */
  &builtin_cmap_bwgradient,
  &builtin_cmap_gradient,
  /* **************** freq **************** */
  &builtin_freq_dots,
  &builtin_freq_drops,
  /* **************** general **************** */
  &builtin_general_fade,
  &builtin_general_blur,
  /* **************** wave **************** */
  &builtin_wave_horizontal,
  &builtin_wave_vertical,
  &builtin_wave_normalize,
  &builtin_wave_smooth,
  &builtin_wave_radial,
  /* **************** xform **************** */
  &builtin_xform_spin,
  &builtin_xform_ripple,
  &builtin_xform_bump_spin,
  /* **************** the end! **************** */
  NULL
};
