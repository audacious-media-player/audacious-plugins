include ../../../mk/rules.mk
include ../../../mk/init.mk

OBJECTIVE_LIBS_NOINST=	libffwma.a

SOURCES=	allcodecs.c allformats.c \
		asf.c avio.c aviobuf.c \
		common.c cutils.c dsputil.c \
		fft.c file.c futils.c mdct.c \
		os_support.c \
		parser.c simple_idct.c \
		utils.c wmadec.c mem.c uri.c

CFLAGS+=	$(PICFLAGS) -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_GNU_SOURCE -c -I../../..

OBJECTS=${SOURCES:.c=.o}

include ../../../mk/objective.mk
