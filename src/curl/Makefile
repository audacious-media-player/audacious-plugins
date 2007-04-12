include ../../mk/rules.mk
include ../../mk/init.mk

SUBDIRS = 

OBJECTIVE_LIBS = libcurl$(SHARED_SUFFIX)

LIBDIR = $(plugindir)/$(GENERAL_PLUGIN_DIR)

LIBADD += $(GTK_LIBS) $(GLIB_LIBS) $(PANGO_LIBS) $(CURL_LIBS)

SOURCES = curl.c

CFLAGS += $(PICFLAGS) $(GTK_CFLAGS) $(GLIB_CFLAGS) $(PANGO_CFLAGS) $(BEEP_DEFINES) $(CURL_CFLAGS) -I../../intl -I../.. -Wextra -Wuninitialized

OBJECTS = ${SOURCES:.c=.o}

include ../../mk/objective.mk
