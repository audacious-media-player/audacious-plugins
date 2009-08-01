#define LASTFM_HANDSHAKE_URL "http://ws.audioscrobbler.com/radio/handshake.php?version=1.1.1&platform=linux&username=%s&passwordmd5=%s&debug=0&language=jp"
#define LASTFM_ADJUST_URL "http://ws.audioscrobbler.com/radio/adjust.php?session=%s&url=%s&debug=0"
#define LASTFM_METADATA_URL "http://ws.audioscrobbler.com/radio/np.php?session=%s&debug=0"

#define LASTFM_CURL_TIMEOUT 10


#define LASTFM_LOGIN_OK                 0
#define LASTFM_LOGIN_ERROR              1
#define LASTFM_MISSING_LOGIN_DATA       2
#define LASTFM_SESSION_MISSING          4
#define LASTFM_ADJUST_OK                8
#define LASTFM_ADJUST_FAILED           16
#define METADATA_FETCH_FAILED          64
#define METADATA_FETCH_SUCCEEDED      128
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
    unsigned int lastfm_duration;
    unsigned int lastfm_progress;
} LastFM;
GTimeVal *t0, *t1;

GThread *metadata_thread = NULL;
gint thread_count = 0;
static GMutex *metadata_mutex = NULL;
static gchar *login_url = NULL;
VFSFile *lastfm_aud_vfs_fopen_impl(const gchar * path, const gchar * mode);

size_t lastfm_aud_vfs_fread_impl(gpointer ptr, size_t size, size_t nmemb, VFSFile * file);

size_t lastfm_aud_vfs_fwrite_impl(gconstpointer ptr, size_t size, size_t nmemb, VFSFile * file);

gint lastfm_aud_vfs_getc_impl(VFSFile * stream);

gint lastfm_aud_vfs_ungetc_impl(gint c, VFSFile * stream);

gint lastfm_aud_vfs_fseek_impl(VFSFile * file, glong offset, gint whence);

void lastfm_aud_vfs_rewind_impl(VFSFile * file);

glong lastfm_aud_vfs_ftell_impl(VFSFile * file);

gboolean lastfm_aud_vfs_feof_impl(VFSFile * file);

gint lastfm_aud_vfs_truncate_impl(VFSFile * file, glong size);

off_t lastfm_aud_vfs_fsize_impl(VFSFile * file);

gint lastfm_aud_vfs_fclose_impl(VFSFile * file);

gchar *lastfm_aud_vfs_metadata_impl(VFSFile * file, const gchar * field);

gboolean parse_metadata(LastFM * handle, gchar ** res);

static gpointer lastfm_metadata_thread_func(gpointer arg);

static gboolean lastfm_login(void);

LowlevelPlugin *get_lplugin_info(void);
