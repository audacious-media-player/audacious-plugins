PLUGIN = moonstone${PLUGIN_SUFFIX}

SRCS = moonstone.cc \
       main_window.cc \
       info_bar.cc \
       tool_bar.cc \
       time_slider.cc \
       playlist.cc \
       playlist_tabs.cc \
       playlist_header.cc \
       playlist_model.cc

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${GENERAL_PLUGIN_DIR}

LD = ${CXX}

CFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS} -I../.. ${QT_CFLAGS}
LIBS += ${QT_LIBS} -laudqt
