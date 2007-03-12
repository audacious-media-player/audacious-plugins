include ../../../mk/rules.mk
include ../../../mk/init.mk

OBJECTIVE_LIBS_NOINST=	libavformat.a

SOURCES = \
4xm.c               electronicarts.c    mov.c               rm.c	\
adtsenc.c           ffm.c               movenc.c            rtp.c	\
aiff.c              file.c              mp3.c               rtpproto.c	\
allformats.c        flic.c              mpeg.c              rtsp.c	\
amr.c               flvdec.c            mpegts.c            segafilm.c	\
asf-enc.c           flvenc.c            mpegtsenc.c         sgi.c	\
asf.c               framehook.c         mpjpeg.c            sierravmd.c	\
au.c                gif.c               mtv.c               smacker.c	\
audio.c             gifdec.c            mxf.c               sol.c	\
avidec.c            grab.c              nsvdec.c            swf.c	\
avienc.c            grab_bktr.c         nut.c               tcp.c	\
avio.c              gxf.c               nuv.c               tiertexseq.c\
aviobuf.c           gxfenc.c            ogg.c               tta.c	\
avisynth.c          http.c              ogg2.c              udp.c	\
avs.c               idcin.c             oggparseflac.c      utils.c	\
barpainet.c         idroq.c             oggparseogm.c       v4l2.c	\
crc.c               img.c               oggparsetheora.c    voc.c	\
cutils.c            img2.c              oggparsevorbis.c    wav.c	\
daud.c              ipmovie.c           os_support.c        wc3movie.c	\
dc1394.c            isom.c              png.c               westwood.c	\
dsicin.c            jpeg.c              pnm.c               wv.c	\
dv.c                matroska.c          psxstr.c            yuv.c	\
dv1394.c            mm.c                raw.c               yuv4mpeg.c	\
dvenc.c             mmf.c               riff.c

CFLAGS+=	$(PICFLAGS) -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_GNU_SOURCE -c -I../../.. -I../libavformat -I../libavutil -I..

OBJECTS=${SOURCES:.c=.o}

include ../../../mk/objective.mk

libavformat.a: $(OBJECTS)
	$(AR) cq $@ $(OBJECTS)
