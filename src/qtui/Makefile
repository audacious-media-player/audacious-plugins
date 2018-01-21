PLUGIN = qtui${PLUGIN_SUFFIX}

SRCS = qtui.cc \
       dialogs-qt.cc \
       main_window.cc \
       menu-ops.cc \
       menus.cc \
       playlist.cc \
       playlist_header.cc \
       playlist_model.cc \
       playlist_tabs.cc \
       info_bar.cc \
       search_bar.cc \
       status_bar.cc \
       tool_bar.cc \
       time_slider.cc \
       settings.cc

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${GENERAL_PLUGIN_DIR}

LD = ${CXX}

CFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS} -I../.. ${QT_CFLAGS}
LIBS += ${QT_LIBS} -laudqt

%.moc: %.h
	moc $< -o $@

rc_%.cc: %.qrc
	rcc $< -o $@
