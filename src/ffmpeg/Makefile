include ../../mk/rules.mk
include ../../mk/init.mk

OBJECTIVE_LIBS = libffmpeg$(SHARED_SUFFIX)

SUBDIRS = libavcodec libavformat libavutil

LIBDIR = $(plugindir)/$(INPUT_PLUGIN_DIR)

LIBADD = $(GTK_LIBS) $(GLIB_LIBS) $(PANGO_LIBS)

SOURCES = ffmpeg.c \
	libavcodec/aasc.c \
	libavcodec/adpcm.c \
	libavcodec/adx.c \
	libavcodec/alac.c \
	libavcodec/allcodecs.c \
	libavcodec/audioconvert.c \
	libavcodec/bitstream.c \
	libavcodec/bitstream_filter.c \
	libavcodec/cabac.c \
	libavcodec/cook.c \
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
	libavcodec/tta.c \
	libavcodec/utils.c \
	libavcodec/wavpack.c \
	libavcodec/wmadec.c \
	libavcodec/wnv1.c \
	libavcodec/ws-snd1.c \
	libavformat/4xm.c \
	libavformat/adtsenc.c \
	libavformat/aiff.c \
	libavformat/allformats.c \
	libavformat/amr.c \
	libavformat/asf-enc.c \
	libavformat/asf.c \
	libavformat/au.c \
	libavformat/audio.c \
	libavformat/avio.c \
	libavformat/aviobuf.c \
	libavformat/barpainet.c \
	libavformat/crc.c \
	libavformat/cutils.c \
	libavformat/daud.c \
	libavformat/dsicin.c \
	libavformat/electronicarts.c \
	libavformat/ffm.c \
	libavformat/file.c \
	libavformat/flic.c \
	libavformat/flvdec.c \
	libavformat/flvenc.c \
	libavformat/framehook.c \
	libavformat/gifdec.c \
	libavformat/grab.c \
	libavformat/idcin.c \
	libavformat/idroq.c \
	libavformat/img2.c \
	libavformat/ipmovie.c \
	libavformat/isom.c \
	libavformat/matroska.c \
	libavformat/mm.c \
	libavformat/mmf.c \
	libavformat/mp3.c \
	libavformat/mpeg.c \
	libavformat/mpegts.c \
	libavformat/mpegtsenc.c \
	libavformat/mpjpeg.c \
	libavformat/mtv.c \
	libavformat/mxf.c \
	libavformat/nsvdec.c \
	libavformat/nut.c \
	libavformat/nuv.c \
	libavformat/os_support.c \
	libavformat/pnm.c \
	libavformat/psxstr.c \
	libavformat/raw.c \
	libavformat/riff.c \
	libavformat/rm.c \
	libavformat/segafilm.c \
	libavformat/sgi.c \
	libavformat/sierravmd.c \
	libavformat/smacker.c \
	libavformat/sol.c \
	libavformat/swf.c \
	libavformat/tiertexseq.c \
	libavformat/tta.c \
	libavformat/utils.c \
	libavformat/wav.c \
	libavformat/wc3movie.c \
	libavformat/westwood.c \
	libavformat/wv.c \
	libavformat/yuv.c \
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
	libavutil/softfloat.c

OBJECTS = ${SOURCES:.c=.o}

CFLAGS += $(PICFLAGS) $(GTK_CFLAGS) $(GLIB_CFLAGS) $(PANGO_CFLAGS) -I. -I../.. -I./libavformat \
	-I./libavcodec -I./libavutil -DHAVE_AV_CONFIG_H -DCONFIG_DEMUXERS

include ../../mk/objective.mk
