PLUGIN = sid${PLUGIN_SUFFIX}

SRCS = xs_config.cc	\
       xs_sidplay2.cc	\
       xs_slsup.cc	\
       xmms-sid.cc

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${INPUT_PLUGIN_DIR}

LD = ${CXX}
CFLAGS += ${PLUGIN_CFLAGS}
CXXFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS} -DSIDDATADIR="\"$(datadir)/\"" -I../.. ${SIDPLAYFP_CFLAGS} ${GLIB_CFLAGS}
LIBS += -lm ${SIDPLAYFP_LIBS} ${GLIB_LIBS}
