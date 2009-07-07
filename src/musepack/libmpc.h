#ifndef XMMS_MUSEPACK
#define XMMS_MUSEPACK

#include "config.h"

//xmms headers
extern "C"
{
#include <audacious/plugin.h>
#include <audacious/output.h>
#include <audacious/i18n.h>
}

//stdlib headers
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>

//libmpcdec headers
#undef TRUE
#undef FALSE
#include <mpcdec/mpcdec.h>

//GTK+ headers
#include <glib.h>
#include <gtk/gtk.h>

//taglib headers
#include <taglib/tag.h>
#include <taglib/apetag.h>
#include <taglib/mpcfile.h>

#ifndef M_LN10
#define M_LN10    2.3025850929940456840179914546843642
#endif

typedef struct PluginConfig
{
    gboolean clipPrevention;
    gboolean dynamicBitrate;
    gboolean replaygain;
    gboolean albumGain;
    gboolean isEq;
};

typedef struct Widgets
{
    GtkWidget* aboutBox;
    GtkWidget* configBox;
    GtkWidget* bitrateCheck;
    GtkWidget* clippingCheck;
    GtkWidget* replaygainCheck;
    GtkWidget* albumCheck;
    GtkWidget* infoBox;
    GtkWidget* albumEntry;
    GtkWidget* artistEntry;
    GtkWidget* titleEntry;
    GtkWidget* genreEntry;
    GtkWidget* yearEntry;
    GtkWidget* trackEntry;
    GtkWidget* commentEntry;
    GtkWidget* fileEntry;
};

typedef struct MpcDecoder
{
    gchar*     isError;
    double     offset;
    bool       isOutput;
    bool       isAlive;
    bool       isPause;
};

typedef struct TrackInfo
{
    gint   bitrate;
    gchar* display;
    gint   length;
    gint   sampleFreq;
    gint   channels;
};

typedef struct MpcInfo
{
    gchar*    title;
    gchar*    artist;
    gchar*    album;
    gchar*    comment;
    gchar*    genre;
    gchar*    date;
    unsigned  track;
    unsigned  year;
};

static void       toggleSwitch(GtkWidget*, gpointer);
static void       saveConfigBox(GtkWidget*, gpointer);
//static void       mpcSetEq(int, float, float*);
static void       freeTags(MpcInfo&);
static MpcInfo    getTags(const char*);
static void       mpcGtkPrintLabel(GtkWidget*, const char*, ...);
static GtkWidget* mpcGtkTagLabel(const char*, int, int, int, int, GtkWidget*);
static GtkWidget* mpcGtkTagEntry(int, int, int, int, int, GtkWidget*);
static GtkWidget* mpcGtkLabel(GtkWidget*);
static GtkWidget* mpcGtkButton(const char*, GtkWidget*);
static void       removeTags(GtkWidget*, gpointer);
static void       saveTags(GtkWidget*, gpointer);
static void       closeInfoBox(GtkWidget*, gpointer);
static char*      mpcGenerateTitle(const MpcInfo&, char*);
static void       lockAcquire();
static void       lockRelease();
static void*      decodeStream(InputPlayback*);
static int        processBuffer(InputPlayback*, MPC_SAMPLE_FORMAT*, char*, mpc_decoder&);
//static void*      endThread(char*, FILE*, bool);
static bool       isAlive();
static void       setAlive(bool);
static double     getOffset();
static void       setOffset(double);
static bool       isPause();
static void       setReplaygain(mpc_streaminfo&, mpc_decoder&);

#ifdef MPC_FIXED_POINT
inline static int shiftSigned(MPC_SAMPLE_FORMAT val, int shift)
{
    if (shift > 0)
        val <<= shift;
    else if (shift < 0)
        val >>= -shift;
    return (int) val;
}
#endif

inline static void copyBuffer(MPC_SAMPLE_FORMAT* pInBuf, char* pOutBuf, unsigned pLength)
{
    unsigned pSize = 16;
    int clipMin    = -1 << (pSize - 1);
    int clipMax    = (1 << (pSize - 1)) - 1;
    int floatScale =  1 << (pSize - 1);
    for (unsigned n = 0; n < 2 * pLength; n++)
    {
        int val;
#ifdef MPC_FIXED_POINT
        val = shiftSigned(pInBuf[n], pSize - MPC_FIXED_POINT_SCALE_SHIFT);
#else
        val = (int) (pInBuf[n] * floatScale);
#endif
        if (val < clipMin)
            val = clipMin;
        else if (val > clipMax)
            val = clipMax;
        unsigned shift = 0;
        do
        {
            pOutBuf[n * 2 + (shift / 8)] = (unsigned char) ((val >> shift) & 0xFF);
            shift += 8;
        }
        while (shift < pSize);
    }
}

#endif
