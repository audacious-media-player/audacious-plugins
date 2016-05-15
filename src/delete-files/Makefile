PLUGIN = delete-files${PLUGIN_SUFFIX}

SRCS = delete-files.cc

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${GENERAL_PLUGIN_DIR}

LD = ${CXX}

CPPFLAGS += -I../.. ${GIO_CFLAGS}
CFLAGS += ${PLUGIN_CFLAGS}
LIBS += ${GIO_LIBS}

ifeq ($(USE_GTK),yes)
CPPFLAGS += ${GTK_CFLAGS}
LIBS += ${GTK_LIBS} -laudgui
endif

ifeq ($(USE_QT),yes)
CPPFLAGS += ${QT_CFLAGS}
LIBS += ${QT_LIBS}
endif
