/*
 * Audacious: Cross platform multimedia player
 * Copyright (c) 2005  Audacious Team
 *
 * Driver for Game_Music_Emu library. See details at:
 * http://www.slack.net/~ant/libs/
 */

#ifndef AUD_CONSOLE_CONFIGURE_H
#define AUD_CONSOLE_CONFIGURE_H 1

typedef struct {
	int loop_length;           /* length of tracks that lack timing information */
	bool resample;          /* whether or not to resample */
	int resample_rate;         /* rate to resample at */
	int treble;                /* -100 to +100 */
	int bass;                  /* -100 to +100 */
	bool ignore_spc_length; /* if true, ignore length from SPC tags */
	int echo;                  /* 0 to +100 */
	bool inc_spc_reverb;    /* if true, increases the default reverb */
} AudaciousConsoleConfig;

extern AudaciousConsoleConfig audcfg;

#endif /* AUD_CONSOLE_CONFIGURE_H */
