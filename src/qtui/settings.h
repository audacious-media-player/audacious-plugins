/*
 * settings.h
 * Copyright 2015 Eugene Paskevich
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

#ifndef SETTINGS_H
#define SETTINGS_H

#include <libaudcore/templates.h>

#define DEFAULT_COLUMNS "playing title artist album queued length"

struct PluginPreferences;
extern const PluginPreferences qtui_prefs;

extern const char * const qtui_defaults[];

#endif
