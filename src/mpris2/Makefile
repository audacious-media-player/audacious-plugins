PLUGIN = mpris2${PLUGIN_SUFFIX}

SRCS = object-core.c object-player.c plugin.cc
CLEAN = object-core.c object-core.h object-player.c object-player.h

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${GENERAL_PLUGIN_DIR}

LD = ${CXX}

CPPFLAGS += -I../.. ${GLIB_CFLAGS} ${GIO_CFLAGS}
CFLAGS += ${PLUGIN_CFLAGS}
LIBS += -lm ${GLIB_LIBS} ${GIO_LIBS}

pre-depend: object-core.c object-core.h object-player.c object-player.h

object-core.h: mpris2.xml
	gdbus-codegen --interface-prefix org.mpris. --c-namespace Mpris --generate-c-code object-core mpris2.xml

object-core.c: object-core.h
	# nothing to do here

object-player.h: mpris2-player.xml
	gdbus-codegen --interface-prefix org.mpris. --c-namespace Mpris --generate-c-code object-player mpris2-player.xml

object-player.c: object-player.h
	# nothing to do here
