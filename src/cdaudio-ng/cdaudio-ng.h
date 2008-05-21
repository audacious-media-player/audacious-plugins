/*
 * Audacious CD Digital Audio plugin
 *
 * Copyright (c) 2007 Calin Crisan <ccrisan@gmail.com>
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; under version 3 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses>.
 */


#ifndef CDAUDIO_NG_H
#define CDAUDIO_NG_H


#define DEF_STRING_LEN			256
#define CDDA_DUMMYPATH			"cdda://"
#define CDDA_DAE_FRAMES			8
#define CDDA_DEFAULT_LIMIT_SPEED	1
#define CDDA_DEFAULT_CDDB_SERVER	"freedb.org"
#define CDDA_DEFAULT_CDDB_PORT		8880
#define CDDA_DEFAULT_PROXY_PORT		8080


typedef struct {

	gchar				performer[DEF_STRING_LEN];
	gchar				name[DEF_STRING_LEN];
	gchar				genre[DEF_STRING_LEN];
	lsn_t				startlsn;
	lsn_t				endlsn;

} trackinfo_t;

typedef struct {

	lsn_t				startlsn;
	lsn_t				endlsn;
	lsn_t				currlsn;
	lsn_t				seektime;	/* in miliseconds */
	InputPlayback                   *pplayback;
	GThread				*thread;

} dae_params_t;


#endif	// CDAUDIO_NG_H

