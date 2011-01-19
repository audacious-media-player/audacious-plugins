/*This code was copied shamelessly from libMTP's examples*/
#include "string.h"
#include "filetype.h"

typedef struct {
    const char *ptype;
    const LIBMTP_filetype_t filetype;
} file_entry_t;

/* This need to be kept constantly updated as new file types arrive. */
static const file_entry_t file_entries[] = {
    { "wav",  LIBMTP_FILETYPE_WAV },
    { "mp3",  LIBMTP_FILETYPE_MP3 },
    { "wma",  LIBMTP_FILETYPE_WMA },
    { "ogg",  LIBMTP_FILETYPE_OGG },
    { "mp4",  LIBMTP_FILETYPE_MP4 },
    { "wmv",  LIBMTP_FILETYPE_WMV },
    { "avi",  LIBMTP_FILETYPE_AVI },
    { "mpeg", LIBMTP_FILETYPE_MPEG },
    { "mpg",  LIBMTP_FILETYPE_MPEG },
    { "asf",  LIBMTP_FILETYPE_ASF },
    { "qt",   LIBMTP_FILETYPE_QT },
    { "mov",  LIBMTP_FILETYPE_QT },
    { "wma",  LIBMTP_FILETYPE_WMA },
    { "jpg",  LIBMTP_FILETYPE_JPEG },
    { "jpeg", LIBMTP_FILETYPE_JPEG },
    { "jfif", LIBMTP_FILETYPE_JFIF },
    { "tif",  LIBMTP_FILETYPE_TIFF },
    { "tiff", LIBMTP_FILETYPE_TIFF },
    { "bmp",  LIBMTP_FILETYPE_BMP },
    { "gif",  LIBMTP_FILETYPE_GIF },
    { "pic",  LIBMTP_FILETYPE_PICT },
    { "pict", LIBMTP_FILETYPE_PICT },
    { "png",  LIBMTP_FILETYPE_PNG },
    { "wmf",  LIBMTP_FILETYPE_WINDOWSIMAGEFORMAT },
    { "ics",  LIBMTP_FILETYPE_VCALENDAR2 },
    { "exe",  LIBMTP_FILETYPE_WINEXEC },
    { "com",  LIBMTP_FILETYPE_WINEXEC },
    { "bat",  LIBMTP_FILETYPE_WINEXEC },
    { "dll",  LIBMTP_FILETYPE_WINEXEC },
    { "sys",  LIBMTP_FILETYPE_WINEXEC },
    { "aac",  LIBMTP_FILETYPE_AAC },
    { "mp2",  LIBMTP_FILETYPE_MP2 },
    { "flac", LIBMTP_FILETYPE_FLAC },
    { "m4a",  LIBMTP_FILETYPE_M4A },
    { "doc",  LIBMTP_FILETYPE_DOC },
    { "xml",  LIBMTP_FILETYPE_XML },
    { "xls",  LIBMTP_FILETYPE_XLS },
    { "ppt",  LIBMTP_FILETYPE_PPT },
    { "mht",  LIBMTP_FILETYPE_MHT },
    { "jp2",  LIBMTP_FILETYPE_JP2 },
    { "jpx",  LIBMTP_FILETYPE_JPX }
};

/* Find the file type based on extension */
LIBMTP_filetype_t
find_filetype (const char * filename)
{
    char *ptype = strrchr(filename,'.');
    unsigned int n;

    if (ptype != NULL)
    {
        /* Skip '.' char */
        ptype++;

        /* Seach entry in the table */
        for (n=0; n<sizeof(file_entries)/sizeof(file_entries[0]); n++)
        {
            if (!strcasecmp (ptype, file_entries[n].ptype))
                return file_entries[n].filetype;
        }
    }

    return LIBMTP_FILETYPE_UNKNOWN;
}

