/* This program is licensed under the GNU Library General Public License, version 2,
 * a copy of which is included with this program (with filename LICENSE.LGPL).
 *
 * (c) 2000-2001 Michael Smith <msmith@labyrinth.net.au>
 *
 * VCEdit header.
 *
 */

#ifndef __VCEDIT_H
#define __VCEDIT_H

#include <ogg/ogg.h>
#include <vorbis/codec.h>
#include <libaudcore/plugin.h>

typedef struct {
	ogg_sync_state	 *oy;
	ogg_stream_state *os;

	vorbis_comment	 *vc;
	vorbis_info       vi;

	VFSFile		 *in;
	long		  serial;
	unsigned char	 *mainbuf;
	unsigned char	 *bookbuf;
	int		  mainlen;
	int		  booklen;
	const char 	         *lasterror;
	char             *vendor;
	int               prevW;
	int               extrapage;
	int               eosin;
} vcedit_state;

extern vcedit_state *vcedit_new_state(void);
extern void vcedit_clear(vcedit_state *state);
extern vorbis_comment *vcedit_comments(vcedit_state *state);
extern int vcedit_open(vcedit_state *state, VFSFile &in);
extern int vcedit_write(vcedit_state *state, VFSFile &out);
extern const char *vcedit_error(vcedit_state *state);

#endif /* __VCEDIT_H */

