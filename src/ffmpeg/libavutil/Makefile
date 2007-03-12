include ../../../mk/rules.mk
include ../../../mk/init.mk

OBJECTIVE_LIBS_NOINST=	libavutil.a

SOURCES = \
adler32.c              integer.c              log.c                  mem.c		\
crc.c                  intfloat_readwrite.c   mathematics.c          rational.c		\
fifo.c                 lls.c                  md5.c                  softfloat.c

CFLAGS+=	$(PICFLAGS) -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_GNU_SOURCE -c -I../../.. -I../libavformat -I../libavutil -I.. -DHAVE_AV_CONFIG_H

OBJECTS=${SOURCES:.c=.o}

include ../../../mk/objective.mk

libavutil.a: $(OBJECTS)
	$(AR) cq $@ $(OBJECTS)
