#ifndef XS_PLAYER_H
#define XS_PLAYER_H

#include "xmms-sid.h"
#include "xs_config.h"

#ifdef __cplusplus
extern "C" {
#endif

struct t_xs_status;

typedef struct {
	gint		plrIdent;
	gboolean	(*plrProbe)(t_xs_file *);
	gboolean	(*plrInit)(struct t_xs_status *);
	void		(*plrClose)(struct t_xs_status *);
	gboolean	(*plrInitSong)(struct t_xs_status *);
	guint		(*plrFillBuffer)(struct t_xs_status *, gchar *, guint);
	gboolean	(*plrLoadSID)(struct t_xs_status *, gchar *);
	void		(*plrDeleteSID)(struct t_xs_status *);
	t_xs_tuneinfo*	(*plrGetSIDInfo)(gchar *);
	gboolean	(*plrUpdateSIDInfo)(struct t_xs_status *);
	void		(*plrFlush)(struct t_xs_status *);
} t_xs_player;


typedef struct t_xs_status {
	gint		audioFrequency,		/* Audio settings */
			audioChannels,
			audioBitsPerSample,
			oversampleFactor;	/* Factor of oversampling */
	AFormat		audioFormat;
	gboolean	oversampleEnable;	/* TRUE after sidEngine initialization,
						if xs_cfg.oversampleEnable == TRUE and
						emulation backend supports oversampling.
						*/
	void		*sidEngine;		/* SID-emulation internal engine data */
	t_xs_player	*sidPlayer;		/* Selected player engine */
	gboolean	isError, isPlaying, isInitialized;
	gint		currSong,		/* Current sub-tune */
			lastTime;

	t_xs_tuneinfo	*tuneInfo;
} t_xs_status;


/* Global variables
 */
extern InputPlugin	xs_plugin_ip;

extern t_xs_status	xs_status;
XS_MUTEX_H(xs_status);


#ifdef __cplusplus
}
#endif
#endif /* XS_PLAYER_H */
