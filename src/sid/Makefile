PLUGIN = sid${PLUGIN_SUFFIX}

SRCS = xs_init.c	\
       xs_about.c	\
       xs_support.c	\
       xs_config.c	\
       xs_md5.c		\
       xs_length.c	\
       xs_genui.c	\
       xs_glade.c	\
       xs_interface.c	\
       xs_stil.c	\
       xs_fileinfo.c	\
       xs_filter.c	\
       xs_sidplay1.cc	\
       xs_sidplay2.cc	\
       xs_curve.c	\
       xs_slsup.c	\
       xmms-sid.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${INPUT_PLUGIN_DIR}

CFLAGS += ${PLUGIN_CFLAGS}
CXXFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS} -D_REENTRANT -I../../intl -I../.. -DAUDACIOUS_PLUGIN ${MOWGLI_CFLAGS} ${SIDPLAY1_INCLUDES} ${SIDPLAY2_INCLUDES} ${BUILDERS_INCLUDES} ${GTK_CFLAGS} ${GLIB_CFLAGS} ${PANGO_CFLAGS}
LIBS += ${BUILDERS_LDFLAGS} ${SIDPLAY1_LDADD} ${SIDPLAY2_LDADD} ${RESID_LDADD} ${HARDSID_LDADD} ${GTK_LIBS} ${GLIB_LIBS} ${PANGO_LIBS} -lstdc++
