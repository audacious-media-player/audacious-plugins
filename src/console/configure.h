/*
 * Audacious: Cross platform multimedia player
 * Copyright (c) 2005  Audacious Team
 *
 * Driver for Game_Music_Emu library. See details at:
 * http://www.slack.net/~ant/libs/
 */

#ifndef AUD_CONSOLE_CONFIGURE_H
#define AUD_CONSOLE_CONFIGURE_H 1

#include <glib.h>

typedef struct {
	gint loop_length;           /* length of tracks that lack timing information */
	gboolean resample;          /* whether or not to resample */
	gint resample_rate;         /* rate to resample at */
	gint treble;                /* -100 to +100 */
	gint bass;                  /* -100 to +100 */
	gboolean ignore_spc_length; /* if true, ignore length from SPC tags */
	gint echo;                  /* 0 to +100 */
	gboolean inc_spc_reverb;    /* if true, increases the default reverb */
} AudaciousConsoleConfig;

extern AudaciousConsoleConfig audcfg;

#ifdef __cplusplus
extern "C" {
#endif

void console_cfg_load(void);
void console_cfg_save(void);
void console_cfg_ui(void);

#ifdef __cplusplus
}
#endif

#endif /* AUD_CONSOLE_CONFIGURE_H */
