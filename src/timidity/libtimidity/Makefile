include ../../../extra.mk

STATIC_LIB_NOINST = libtimidity.a

SRCS = \
	common.c \
	instrum.c \
	instrum_dls.c \
	mix.c \
	output.c \
	playmidi.c \
	readmidi.c \
	resample.c \
	stream.c \
	tables.c \
	timidity.c

include ../../../buildsys.mk

CFLAGS += $(PLUGIN_CFLAGS)
CPPFLAGS += -I../../.. $(GLIB_CFLAGS)
