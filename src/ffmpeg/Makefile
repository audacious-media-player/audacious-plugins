include ../../mk/rules.mk
include ../../mk/init.mk

OBJECTIVE_LIBS = libffmpeg$(SHARED_SUFFIX)

SUBDIRS = libavcodec libavformat libavutil

LIBDIR = $(plugindir)/$(INPUT_PLUGIN_DIR)

LIBADD = ./libavformat/libavformat.a $(GTK_LIBS) $(GLIB_LIBS) $(PANGO_LIBS)

SOURCES = ffmpeg.c \
	libavcodec/aasc.c \
	libavcodec/adpcm.c \
	libavcodec/adx.c \
	libavcodec/alac.c \
	libavcodec/allcodecs.c \
	libavcodec/audioconvert.c \
	libavcodec/avs.c \
	libavcodec/bitstream.c \
	libavcodec/bitstream_filter.c \
	libavcodec/cabac.c \
	libavcodec/cook.c \
	libavcodec/cscd.c \
	libavcodec/dpcm.c \
	libavcodec/dsicinav.c \
	libavcodec/dsputil.c \
	libavcodec/eval.c \
	libavcodec/faandct.c \
	libavcodec/fdctref.c \
	libavcodec/fft.c \
	libavcodec/ffv1.c \
	libavcodec/flac.c \
	libavcodec/flashsv.c \
	libavcodec/fraps.c \
	libavcodec/g726.c \
	libavcodec/golomb.c \
	libavcodec/kmvc.c \
	libavcodec/lcl.c \
	libavcodec/loco.c \
	libavcodec/lzo.c \
	libavcodec/mace.c \
	libavcodec/mdct.c \
	libavcodec/mpegaudio.c \
	libavcodec/mpegaudiodec.c \
	libavcodec/opt.c \
	libavcodec/parser.c \
	libavcodec/pcm.c \
	libavcodec/pthread.c \
	libavcodec/qdm2.c \
	libavcodec/qtrle.c \
	libavcodec/ra144.c \
	libavcodec/ra288.c \
	libavcodec/rangecoder.c \
	libavcodec/resample.c \
	libavcodec/resample2.c \
	libavcodec/rpza.c \
	libavcodec/shorten.c \
	libavcodec/simple_idct.c \
	libavcodec/smc.c \
	libavcodec/sonic.c \
	libavcodec/truespeech.c \
	libavcodec/tscc.c \
	libavcodec/tta.c \
	libavcodec/utils.c \
	libavcodec/wavpack.c \
	libavcodec/wmadec.c \
	libavcodec/wnv1.c \
	libavcodec/ws-snd1.c \
	libavutil/adler32.c \
	libavutil/crc.c \
	libavutil/fifo.c \
	libavutil/integer.c \
	libavutil/intfloat_readwrite.c \
	libavutil/lls.c \
	libavutil/log.c \
	libavutil/mathematics.c \
	libavutil/md5.c \
	libavutil/mem.c \
	libavutil/rational.c \
	libavutil/softfloat.c \


OBJECTS = ${SOURCES:.c=.o}

CFLAGS += $(PICFLAGS) $(GTK_CFLAGS) $(GLIB_CFLAGS) $(PANGO_CFLAGS) -I. -I../.. -I./libavformat \
	-I./libavcodec -I./libavutil -std=c99 -DHAVE_AV_CONFIG_H

include ../../mk/objective.mk
