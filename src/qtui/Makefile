PLUGIN = qtui${PLUGIN_SUFFIX}

SRCS = qtui.cc \
       ui_main_window.h \
       main_window.moc \
       main_window.cc \
       rc_images.cc \
       utils.cc

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${GENERAL_PLUGIN_DIR}

LD = ${CXX}

CFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS} -I../.. ${QT_CFLAGS}
LIBS += ${QT_LIBS}

%.moc: %.h
	moc $< -o $@

ui_%.h: %.ui
	uic $< -o $@

rc_%.cc: %.qrc
	rcc $< -o $@
