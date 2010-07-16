#ifndef __VORBIS_H__
#define __VORBIS_H__

#include <vorbis/vorbisfile.h>

#include <audacious/plugin.h>

extern ov_callbacks vorbis_callbacks;

gboolean vorbis_update_song_tuple (const Tuple * tuple, VFSFile * fd);

typedef struct {
    gint http_buffer_size;
    gint http_prebuffer;
    gboolean use_proxy;
    gchar *proxy_host;
    gint proxy_port;
    gboolean proxy_use_auth;
    gchar *proxy_user, *proxy_pass;
    gboolean save_http_stream;
    gchar *save_http_path;
    gboolean tag_override;
    gchar *tag_format;
    gboolean title_encoding_enabled;
    gchar *title_encoding;
} vorbis_config_t;

extern vorbis_config_t vorbis_cfg;

#endif                          /* __VORBIS_H__ */
