PLUGIN = skins${PLUGIN_SUFFIX}

SRCS = actions.cc \
       button.cc \
       dock.cc \
       drag-handle.cc \
       eq-graph.cc \
       eq-slider.cc \
       equalizer.cc \
       hslider.cc \
       main.cc \
       menu-ops.cc \
       menurow.cc \
       menus.cc \
       monostereo.cc \
       number.cc \
       playlist.cc \
       playlist-slider.cc \
       playlist-widget.cc \
       playstatus.cc \
       plugin.cc \
       plugin-window.cc \
       search-select.cc \
       skin.cc \
       skin-ini.cc \
       skins_cfg.cc \
       skinselector.cc \
       surface.cc \
       svis.cc \
       textbox.cc \
       util.cc \
       view.cc \
       vis-callbacks.cc \
       vis.cc \
       widget.cc \
       window.cc

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${GENERAL_PLUGIN_DIR}

LD = ${CXX}

CPPFLAGS += ${PLUGIN_CPPFLAGS} -I../.. ${GTK_CFLAGS}
CFLAGS += ${PLUGIN_CFLAGS}
LIBS += -lm ${GTK_LIBS} -laudgui
