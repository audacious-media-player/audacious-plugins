PLUGIN = scrobbler2${PLUGIN_SUFFIX}

SRCS = scrobbler.c \
	   scrobbler_communication.c \
	   scrobbler_xml_parsing.c
			

include ../../buildsys.mk
include ../../extra.mk

PACKAGE_NAME = audacious

plugindir := ${plugindir}/${GENERAL_PLUGIN_DIR}

CFLAGS += ${PLUGIN_CFLAGS} -Wall
CPPFLAGS += ${PLUGIN_CPPFLAGS}  ${GTK_CFLAGS} ${GLIB_CFLAGS} ${CURL_CFLAGS} ${XML_CFLAGS} -I../..
LIBS += ${GTK_LIBS} ${GLIB_LIBS}  ${CURL_LIBS} ${XML_LIBS}
