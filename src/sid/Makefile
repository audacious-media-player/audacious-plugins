PLUGIN = sid${PLUGIN_SUFFIX}

SRCS = xs_init.c	\
       xs_support.c	\
       xs_config.c	\
       xs_length.c	\
       xs_md5.c \
       xs_stil.c	\
       xs_filter.c	\
       xs_sidplay2.cc	\
       xs_slsup.c	\
       xs_player.c	\
       xmms-sid.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${INPUT_PLUGIN_DIR}

LD = ${CXX}
CFLAGS += ${PLUGIN_CFLAGS}
CXXFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS} -DDATADIR="\"$(datadir)/\"" -I../.. ${SIDPLAYFP_CFLAGS} ${GLIB_CFLAGS}
LIBS += -lm ${SIDPLAYFP_LIBS} ${BUILDERS_LIBS} ${GLIB_LIBS}
