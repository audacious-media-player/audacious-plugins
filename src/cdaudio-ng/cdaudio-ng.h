
#ifndef CDAUDIO_NG_H
#define CDAUDIO_NG_H


#define DEF_STRING_LEN				256
#define CDDA_DEFAULT				"cdda://default"
#define CDDA_DAE_FRAMES				8
#define CDDA_DEFAULT_CDDB_SERVER	"freedb.org"
#define CDDA_DEFAULT_CDDB_PORT		8880


typedef struct {

	char				performer[DEF_STRING_LEN];
	char				name[DEF_STRING_LEN];
	char				genre[DEF_STRING_LEN];
	lsn_t				startlsn;
	lsn_t				endlsn;

} trackinfo_t;

typedef struct {

	lsn_t				startlsn;
	lsn_t				endlsn;
	lsn_t				currlsn;
	lsn_t				seektime;	/* in miliseconds */
	InputPlayback		*pplayback;
	GThread				*thread;

} dae_params_t;


#endif	// CDAUDIO_NG_H
