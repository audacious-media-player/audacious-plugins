PLUGIN = timidity${PLUGIN_SUFFIX}
SRCS = callbacks.c			\
       interface.c			\
       xmms-timidity.c			\
       libtimidity/common.c		\
       libtimidity/instrum.c		\
       libtimidity/instrum_dls.c	\
       libtimidity/mix.c		\
       libtimidity/output.c		\
       libtimidity/playmidi.c		\
       libtimidity/readmidi.c		\
       libtimidity/resample.c		\
       libtimidity/stream.c		\
       libtimidity/tables.c		\
       libtimidity/timidity.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := $(plugindir)/${INPUT_PLUGIN_DIR}

CPPFLAGS += ${PLUGIN_CPPFLAGS} ${MOWGLI_CFLAGS} ${DBUS_CFLAGS} ${GTK_CFLAGS} ${GLIB_CFLAGS} ${PANGO_CFLAGS} -I../.. -I./libtimidity
CFLAGS += ${PLUGIN_CFLAGS}
LIBS += ${GTK_LIBS} ${GLIB_LIBS} ${PANGO_LIBS}
