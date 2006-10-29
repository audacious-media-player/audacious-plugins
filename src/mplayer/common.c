#include <audacious/rcfile.h>
#include "xmmsmplayer.h"

struct mplayer_cfg *mplayer_read_cfg(){
  RcFile *cfg;
  struct mplayer_cfg *new_cfg;
  new_cfg=(struct mplayer_cfg *)malloc(sizeof(struct mplayer_cfg));
  memset(new_cfg,0,sizeof(struct mplayer_cfg));
  cfg = bmp_rcfile_open("what");
  bmp_rcfile_read_int(cfg,"xmms-mplayer","vo",&(new_cfg->vo));
  bmp_rcfile_read_int(cfg,"xmms-mplayer","ao",&(new_cfg->ao));
  bmp_rcfile_read_bool(cfg,"xmms-mplayer","zoom",&(new_cfg->zoom));
  bmp_rcfile_read_bool(cfg,"xmms-mplayer","framedrop",&(new_cfg->framedrop));
  bmp_rcfile_read_bool(cfg,"xmms-mplayer","idx",&(new_cfg->idx));
  bmp_rcfile_read_bool(cfg,"xmms-mplayer","onewin",&(new_cfg->onewin));
  bmp_rcfile_read_bool(cfg,"xmms-mplayer","xmmsaudio",&(new_cfg->xmmsaudio));
  bmp_rcfile_read_string(cfg,"xmms-mplayer","extra",&(new_cfg->extra));
  return new_cfg;
}
