# Makefile
#
# Project: Ampache Browser Audacious Plugin
# License: GNU GPLv3
#
# Copyright (C) 2015 - 2016 Róbert Čerňanský

PLUGIN = ampache${PLUGIN_SUFFIX}

SRCS = ampache.cc

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${GENERAL_PLUGIN_DIR}

LD = ${CXX}
CPPFLAGS += -I../.. -I. ${QT_CFLAGS} ${AMPACHE_CFLAGS}
CFLAGS += ${PLUGIN_CFLAGS}
LIBS += ${QT_LIBS} ${AMPACHE_LIBS}
