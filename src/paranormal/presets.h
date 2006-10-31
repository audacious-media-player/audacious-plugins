#ifndef _PRESETS_H
#define _PRESETS_H

#include <glib.h>

#include "actuators.h"

struct pn_actuator *load_preset (const char *filename);
gboolean save_preset (const char *filename, const struct pn_actuator *actuator);

#endif /* _PRESETS_H */
