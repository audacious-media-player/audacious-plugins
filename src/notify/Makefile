PLUGIN = notify${PLUGIN_SUFFIX}

SRCS = event.cc notify.cc osd.cc

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${GENERAL_PLUGIN_DIR}

LD = ${CXX}

CPPFLAGS += -I../.. ${PLUGIN_CPPFLAGS} ${NOTIFY_CFLAGS}
CFLAGS += ${PLUGIN_CFLAGS}
LIBS += ${NOTIFY_LIBS}

ifeq ($(USE_GTK),yes)
CPPFLAGS += ${GTK_CFLAGS}
LIBS += ${GTK_LIBS} -laudgui
endif

ifeq ($(USE_QT),yes)
CPPFLAGS += ${QT_CFLAGS}
LIBS += ${QT_LIBS} -laudqt
endif
