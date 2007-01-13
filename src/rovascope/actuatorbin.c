#include "paranormal.h"
#include "actuators.h"

struct pn_actuator *
rovascope_get_random_colourmap(void)
{
   struct pn_actuator *out;

   srand(time(NULL));

   out = create_actuator("cmap_bwgradient");
   out->options[2].val.cval.r = 255 - rand() % 255;
   out->options[2].val.cval.g = 255 - rand() % 255;
   out->options[2].val.cval.b = 255 - rand() % 255;

   return out;
}

struct pn_actuator *
rovascope_get_random_transform(void)
{
   struct pn_actuator *out;
   gchar *candidates[] = {
      "d = cos(d) ^ 2;",
      "r = sin(r);",
      "r = sin(r); d = cos(d) ^ 2;",
   };

   srand(time(NULL));

   out = create_actuator("xform_movement");
   out->options[0].val.sval = 
	g_strdup(candidates[rand() % G_N_ELEMENTS(candidates)]);

   return out;
}

struct pn_actuator *
rovascope_get_random_general(void)
{
   struct pn_actuator *out;

   gchar *candidates[] = {
      "general_fade", "general_blur", "general_mosaic", 
      "general_flip", "general_fade", "general_fade",
   };

   out = create_actuator(candidates[rand() % G_N_ELEMENTS(candidates)]);

   return out;
}

struct pn_actuator *
rovascope_get_random_normal_scope(void)
{
   struct pn_actuator *out;

   gchar *candidates[] = {
      "wave_horizontal", "wave_vertical", "wave_radial",
   };

   out = create_actuator(candidates[rand() % G_N_ELEMENTS(candidates)]);

   return out;
}

struct pn_actuator *
rovascope_get_random_actuator(void)
{
   struct pn_actuator *out;
   struct pn_actuator *(*func)();

   void *candidates[] = {
	rovascope_get_random_colourmap,
	rovascope_get_random_general,
	rovascope_get_random_normal_scope,
	rovascope_get_random_transform,
   };

   func = candidates[rand() % G_N_ELEMENTS(candidates)];
   out = func();

   return out;
}
