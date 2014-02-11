/*
 * Audacious: Cross platform multimedia player
 * Copyright (c) 2005-2014 Audacious Team
 *
 */


#ifndef MPG123_PREFS_H
#define MPG123_PREFS_H

#include <libaudcore/core.h>

typedef struct {
	bool_t full_scan;     /* Set to read all frame headers when calculating file length */
} AudaciousMPG123Config;

extern AudaciousMPG123Config mpg123cfg;

bool_t mpg123_cfg_load (void);
void   mpg123_cfg_save (void);

#endif /* MPG123_PREFS */