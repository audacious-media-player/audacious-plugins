#ifndef _NEON_PLUGIN_H
#define _NEON_PLUGIN_H

#include <glib.h>
#include <audacious/vfs.h>
#include <ne_session.h>
#include <ne_request.h>
#include <ne_uri.h>
#include "rb.h"


static void init(void);
static void fini(void);

VFSFile *neon_vfs_fopen_impl(const gchar* path, const gchar* mode);
gint neon_vfs_fclose_impl(VFSFile* file);
size_t neon_vfs_fread_impl(gpointer ptr_, size_t size, size_t nmemb, VFSFile* file);
size_t neon_vfs_fwrite_impl(gconstpointer ptr, size_t size, size_t nmemb, VFSFile* file);
gint neon_vfs_getc_impl(VFSFile* file);
gint neon_vfs_ungetc_impl(gint c, VFSFile* file);
void neon_vfs_rewind_impl(VFSFile* file);
glong neon_vfs_ftell_impl(VFSFile* file);
gboolean neon_vfs_feof_impl(VFSFile* file);
gint neon_vfs_truncate_impl(VFSFile* file, glong size);
gint neon_vfs_fseek_impl(VFSFile* file, glong offset, gint whence);
gchar *neon_vfs_metadata_impl(VFSFile* file, const gchar * field);
off_t neon_vfs_fsize_impl(VFSFile* file);

ne_uri purl;

typedef enum {
    NEON_READER_INIT=0,
    NEON_READER_RUN=1,
    NEON_READER_ERROR,
    NEON_READER_EOF,
    NEON_READER_TERM
} neon_reader_t;

struct reader_status {
    GMutex* mutex;
    GCond* cond;
    gboolean reading;
    neon_reader_t status;
};

struct neon_handle {
    gchar* url;                         /* The URL, as passed to us */
    ne_uri* purl;                       /* The URL, parsed into a structure */
    struct ringbuf rb;                  /* Ringbuffer for our data */
    unsigned char redircount;           /* Redirect count for the opened URL */
    long pos;                           /* Current position in the stream (number of last byte delivered to the player) */
    unsigned long content_start;        /* Start position in the stream */
    long content_length;                /* Total content length, counting from content_start, if known. -1 if unknown */
    gboolean can_ranges;                /* TRUE if the webserver advertised accept-range: bytes */
    ne_session* session;
    ne_request* request;
    GThread* reader;
    struct reader_status reader_status;
    gboolean eof;
};


#endif
