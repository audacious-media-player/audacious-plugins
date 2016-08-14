PLUGIN = scrobbler${PLUGIN_SUFFIX}

SRCS = scrobbler.cc \
	   scrobbler_communication.cc \
	   scrobbler_xml_parsing.cc \
	   config_window.cc


include ../../buildsys.mk
include ../../extra.mk

PACKAGE_NAME = audacious

plugindir := ${plugindir}/${GENERAL_PLUGIN_DIR}

LD = ${CXX}

CFLAGS += ${PLUGIN_CFLAGS} -Wall
CPPFLAGS += ${PLUGIN_CPPFLAGS} ${GLIB_CFLAGS} ${CURL_CFLAGS} ${XML_CFLAGS} -I../..
LIBS += ${GLIB_LIBS} ${CURL_LIBS} ${XML_LIBS}
