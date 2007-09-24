include ../../../extra.mk

STATIC_LIB_NOINST = libffwma.a

SRCS=	allcodecs.c allformats.c \
		asf.c avio.c aviobuf.c \
		common.c cutils.c dsputil.c \
		fft.c file.c futils.c mdct.c \
		os_support.c \
		parser.c simple_idct.c \
		utils.c wmadec.c mem.c uri.c

include ../../../buildsys.mk

CFLAGS += $(PLUGIN_CFLAGS)
CPPFLAGS += $(GTK_CFLAGS) $(GLIB_CFLAGS) $(MOWGLI_CFLAGS) $(DBUS_CFLAGS) $(PANGO_CFLAGS) -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_GNU_SOURCE -I../../..

