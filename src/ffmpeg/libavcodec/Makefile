include ../../../mk/rules.mk
include ../../../mk/init.mk

OBJECTIVE_LIBS_NOINST=	libavcodec.a

SOURCES = \
4xm.c                     h261.c                    raw.c		\
8bps.c                    h263.c                    resample.c	\
a52dec.c                  h263dec.c                 resample2.c	\
aasc.c                    h264.c                    roqvideo.c	\
ac3dec.c                  h264idct.c                rpza.c	\
ac3enc.c                  huffyuv.c                 rtjpeg.c	\
adpcm.c                   idcinvideo.c              rv10.c	\
adx.c                     imgconvert.c              shorten.c	\
alac.c                    imgresample.c             simple_idct.c \
allcodecs.c               indeo2.c                  smacker.c	\
amr.c                     indeo3.c                  smc.c	\
apiexample.c              interplayvideo.c          snow.c	\
asv1.c                    jfdctfst.c                sonic.c	\
audioconvert.c            jfdctint.c                svq1.c	\
avs.c                     jpeg_ls.c                 svq3.c	\
jrevdct.c                 targa.c	\
bitstream.c               kmvc.c                    tiertexseqv.c \
bitstream_filter.c        lcl.c                     tiff.c	\
bmp.c                     libgsm.c                  truemotion1.c	\
cabac.c                   loco.c                    truemotion2.c	\
cavs.c                    lzo.c                     truespeech.c	\
cavsdsp.c                 mace.c                    tscc.c	\
cinepak.c                 mdct.c                    tta.c	\
cljr.c                    mdec.c                    ulti.c	\
cook.c                    mjpeg.c                   utils.c	\
cscd.c                    mmvideo.c                 vc1.c	\
cyuv.c                    motion_est.c              vc1dsp.c	\
dct-test.c                motion_est_template.c     vcr1.c	\
dpcm.c                    motion_test.c             vmdav.c	\
dsicinav.c                mp3lameaudio.c            vmnc.c	\
dsputil.c                 mpeg12.c                  vorbis.c	\
dtsdec.c                  mpegaudio.c               vorbis_data.c \
dv.c                      mpegaudiodec.c            vorbis_enc.c \
dvbsub.c                  mpegvideo.c               vp3.c	\
dvbsubdec.c               msmpeg4.c                 vp3dsp.c	\
dvdsub.c                  msrle.c                   vp5.c	\
dvdsubenc.c               msvideo1.c                vp56.c	\
error_resilience.c        nuv.c                     vp56data.c	\
eval.c                    oggvorbis.c               vp6.c	\
faac.c                    opt.c                     vqavideo.c	\
faad.c                    \
faandct.c                 parser.c                  wavpack.c	\
fdctref.c                 pcm.c                     wmadec.c	\
fft-test.c                png.c                     wmv2.c	\
fft.c                     pnm.c                     wnv1.c	\
ffv1.c                    pthread.c                 ws-snd1.c	\
flac.c                    qdm2.c                    x264.c	\
flacenc.c                 qdrw.c                    xan.c	\
flashsv.c                 qpeg.c                    xl.c	\
flicvideo.c               qtrle.c                   xvid_rc.c	\
fraps.c                   ra144.c                   xvidff.c	\
g726.c                    ra288.c                   xvmcvideo.c	\
gifdec.c                  rangecoder.c              zmbv.c	\
golomb.c                  ratecontrol.c

CFLAGS+=	$(PICFLAGS) -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_GNU_SOURCE -c -I../../.. -I../libavformat -I../libavutil -I..

OBJECTS=${SOURCES:.c=.o}

include ../../../mk/objective.mk

libavcodec.a: $(OBJECTS)
	$(AR) cq $@ $(OBJECTS)
