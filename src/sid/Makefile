PLUGIN = sid${PLUGIN_SUFFIX}

SRCS = xs_init.c	\
       xs_support.c	\
       xs_config.c	\
       xs_length.c	\
       xs_stil.c	\
       xs_filter.c	\
       xs_sidplay1.cc	\
       xs_sidplay2.cc	\
       xs_slsup.c	\
       xs_player.c	\
       xmms-sid.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${INPUT_PLUGIN_DIR}

CFLAGS += ${PLUGIN_CFLAGS}
CXXFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS} -D_REENTRANT -I../.. -DAUDACIOUS_PLUGIN ${SIDPLAY1_CFLAGS} ${SIDPLAY2_CFLAGS} ${BUILDERS_CFLAGS} ${GTK_CFLAGS} ${GLIB_CFLAGS}
LIBS += -lm ${BUILDERS_LDFLAGS} ${SIDPLAY1_LIBS} ${SIDPLAY2_LIBS} ${BUILDERS_LIBS} ${GTK_LIBS} ${GLIB_LIBS} -lstdc++
