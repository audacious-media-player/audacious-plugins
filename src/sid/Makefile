PLUGIN = sid${PLUGIN_SUFFIX}

SRCS = xs_config.cc	\
       xs_sidplay2.cc	\
       xmms-sid.cc

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${INPUT_PLUGIN_DIR}

LD := ${CXX}
CFLAGS += ${PLUGIN_CFLAGS}
CXXFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS} -DSIDDATADIR="\"$(datadir)/\"" -I../.. ${SIDPLAYFP_CFLAGS}
LIBS += -lm ${SIDPLAYFP_LIBS}

# libsidplayfp is not c++17 clean yet
# https://sourceforge.net/p/sidplay-residfp/bugs/38/
CXXFLAGS := $(filter-out -std=gnu++17,$(CXXFLAGS))
CXXFLAGS += -std=gnu++11
