#include <audacious/configdb.h>
#include <stdlib.h>
#include "xmmsmplayer.h"

struct mplayer_cfg *mplayer_read_cfg(){
  ConfigDb *cfg;
  struct mplayer_cfg *new_cfg;
  new_cfg=(struct mplayer_cfg *)malloc(sizeof(struct mplayer_cfg));
  memset(new_cfg,0,sizeof(struct mplayer_cfg));
  cfg = bmp_cfg_db_open();
  bmp_cfg_db_get_int(cfg,"xmms-mplayer","vo",&(new_cfg->vo));
  bmp_cfg_db_get_int(cfg,"xmms-mplayer","ao",&(new_cfg->ao));
  bmp_cfg_db_get_bool(cfg,"xmms-mplayer","zoom",&(new_cfg->zoom));
  bmp_cfg_db_get_bool(cfg,"xmms-mplayer","framedrop",&(new_cfg->framedrop));
  bmp_cfg_db_get_bool(cfg,"xmms-mplayer","idx",&(new_cfg->idx));
  bmp_cfg_db_get_bool(cfg,"xmms-mplayer","onewin",&(new_cfg->onewin));
  bmp_cfg_db_get_bool(cfg,"xmms-mplayer","xmmsaudio",&(new_cfg->xmmsaudio));
  bmp_cfg_db_get_string(cfg,"xmms-mplayer","extra",&(new_cfg->extra));
  bmp_cfg_db_close(cfg);
  return new_cfg;
}
