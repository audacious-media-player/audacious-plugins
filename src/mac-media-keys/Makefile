PLUGIN = mac-media-keys${PLUGIN_SUFFIX}

SRCS = MacMediaKeys.mm \
       SPInvocationGrabbing.m \
       SPMediaKeyTap.m

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${GENERAL_PLUGIN_DIR}

LD = ${OBJCXX}

CFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS} -I../..
LIBS += -framework AppKit
