#define LASTFM_HANDSHAKE_URL "http://ws.audioscrobbler.com/radio/handshake.php?version=1.1.1&platform=linux&username=%s&passwordmd5=%s&debug=0&language=jp"
#define LASTFM_ADJUST_URL "http://ws.audioscrobbler.com/radio/adjust.php?session=%s&url=%s&debug=0"
#define LASTFM_METADATA_URL "http://ws.audioscrobbler.com/radio/np.php?session=%s&debug=0"

#define LASTFM_CURL_TIMEOUT 10
#define DEBUG 1

typedef struct
{
	VFSFile *proxy_fd;
	gchar *lastfm_session_id;
	gchar *lastfm_mp3_stream_url;
	gchar *lastfm_station_name;
	gchar *lastfm_artist;
	gchar *lastfm_title;
	gchar *lastfm_album;
	gchar *lastfm_cover;
	unsigned lastfm_duration;
	int login_count;
} LastFM;

VFSFile *lastfm_vfs_fopen_impl(const gchar * path, const gchar * mode);

size_t lastfm_vfs_fread_impl(gpointer ptr, size_t size, size_t nmemb, VFSFile * file);

size_t lastfm_vfs_fwrite_impl(gconstpointer ptr, size_t size, size_t nmemb, VFSFile * file);

gint lastfm_vfs_getc_impl(VFSFile * stream);

gint lastfm_vfs_ungetc_impl(gint c, VFSFile * stream);

gint lastfm_vfs_fseek_impl(VFSFile * file, glong offset, gint whence);

void lastfm_vfs_rewind_impl(VFSFile * file);

glong lastfm_vfs_ftell_impl(VFSFile * file);

gboolean lastfm_vfs_feof_impl(VFSFile * file);

gint lastfm_vfs_truncate_impl(VFSFile * file, glong size);

off_t lastfm_vfs_fsize_impl(VFSFile * file);

gint lastfm_vfs_fclose_impl(VFSFile * file);

gchar *lastfm_vfs_metadata_impl(VFSFile * file, const gchar * field);

static gboolean lastfm_get_metadata(LastFM * handle);

static gboolean lastfm_login(void);

LowlevelPlugin *get_lplugin_info(void);

