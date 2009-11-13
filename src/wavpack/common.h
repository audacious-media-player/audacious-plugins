#ifndef WAVPACK_COMMON_H
#define WAVPACK_COMMON_H

#include <audacious/plugin.h>
#include <wavpack/wavpack.h>

#define MAX_LEN     2048
#define MAX_LEN2    128

typedef struct {
    gchar    title           [MAX_LEN];
    gchar    artist          [MAX_LEN];
    gchar    album           [MAX_LEN];
    gchar    comment         [MAX_LEN];
    gchar    genre           [MAX_LEN];
    gchar    track           [MAX_LEN2];
    gchar    year            [MAX_LEN2];
    gint     _genre;
} WavPackTag;

typedef struct {
    gboolean clipPreventionEnabled;
    gboolean dynBitrateEnabled;
    gboolean replaygainEnabled;
    gboolean albumReplaygainEnabled;
} WavPackConfig;

extern WavPackConfig wv_cfg;
extern WavpackStreamReader wv_readers;
void wv_get_tags(WavPackTag * tag, WavpackContext * ctx);
void wv_read_config(void);
void wv_file_info_box(const gchar * fn);

#endif /* WAVPACK_COMMON_H */
