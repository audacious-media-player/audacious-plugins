include ../../../mk/rules.mk
include ../../../mk/init.mk

OBJECTIVE_LIBS_NOINST=	libavcodec.a

SOURCES = \
	raw.c		\
	resample.c	\
	resample2.c	\
aasc.c          \
rpza.c	\
ac3enc.c                  \
adpcm.c                   \
adx.c                     shorten.c	\
alac.c                    simple_idct.c \
allcodecs.c               \
smc.c	\
apiexample.c              \
sonic.c	\
audioconvert.c            \
avs.c                     \
bitstream.c               kmvc.c                    \
bitstream_filter.c        lcl.c                     \
bmp.c                     \
cabac.c                   loco.c                    \
lzo.c                     truespeech.c	\
mace.c                    tscc.c	\
mdct.c                    tta.c	\
cook.c                    utils.c	\
cscd.c                    \
dct-test.c                \
dpcm.c                    \
dsicinav.c                \
dsputil.c                 \
mpegaudio.c               \
mpegaudiodec.c            \
dvbsub.c                  \
dvbsubdec.c	\
dvdsub.c                  msrle.c                   \
dvdsubenc.c               msvideo1.c                \
eval.c                    oggvorbis.c               \
faac.c                    opt.c                     \
faandct.c                 parser.c                  wavpack.c	\
fdctref.c                 pcm.c                     wmadec.c	\
fft-test.c                png.c                     \
fft.c                     wnv1.c	\
ffv1.c                    pthread.c                 ws-snd1.c	\
flac.c                    qdm2.c                    \
flacenc.c                 \
flashsv.c                 \
flicvideo.c               qtrle.c                   \
fraps.c                   ra144.c                   \
g726.c                    ra288.c                   \
gifdec.c                  rangecoder.c              \
	golomb.c

CFLAGS+=	$(PICFLAGS) -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_GNU_SOURCE -c -I../../.. -I../libavformat -I../libavutil -I.. -DHAVE_AV_CONFIG_H

OBJECTS=${SOURCES:.c=.o}

include ../../../mk/objective.mk

libavcodec.a: $(OBJECTS)
	$(AR) cq $@ $(OBJECTS)
