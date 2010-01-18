PLUGIN = bluetooth${PLUGIN_SUFFIX}

SRCS = bluetooth.c gui.c marshal.c scan_gui.c agent.c
include ../../buildsys.mk
include ../../extra.mk

pre-depend: marshal.h

marshal.h:
	glib-genmarshal --prefix=marshal --header marshal.list > $@
marshal.c:
	glib-genmarshal --prefix=marshal --body marshal.list > $@

plugindir := ${plugindir}/${GENERAL_PLUGIN_DIR}
CPPFLAGS += ${BEEP_DEFINES} ${DBUS_CFLAGS} ${GTK_CFLAGS} ${DBUS_GLIB_CFLAGS} -I../.. ${BLUEZ_CFLAGS}
LIBS += ${GTK_LIBS} ${BLUEZ_LIBS}

