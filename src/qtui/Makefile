PLUGIN = qtui${PLUGIN_SUFFIX}

SRCS = qtui.cc \
       dialogs-qt.cc \
       main_window.cc \
       menu-ops.cc \
       menus.cc \
       playlist.cc \
       playlist_header.cc \
       playlist_model.cc \
       playlist_tabs.cc \
       info_bar.cc \
       search_bar.cc \
       status_bar.cc \
       tool_bar.cc \
       time_slider.cc \
       settings.cc

include ../../buildsys.mk
include ../../extra.mk

# These overrides variables in buildsys.mk and so must come after it.
PACKAGE_NAME = audacious

CUSTOM_ICONS=no
ifeq ($(HAVE_MSWINDOWS),yes)
CUSTOM_ICONS=yes
endif
ifeq ($(HAVE_DARWIN),yes)
CUSTOM_ICONS=yes
endif

ifeq ($(CUSTOM_ICONS),yes)
DATA = QtUi/16/audio-volume-high.png \
       QtUi/16/audio-volume-low.png \
       QtUi/16/audio-volume-medium.png \
       QtUi/16/audio-volume-muted.png \
       QtUi/16/audio-volume-off.png \
       QtUi/16/document-open.png \
       QtUi/16/edit-find.png \
       QtUi/16/list-add.png \
       QtUi/16/media-playback-pause.png \
       QtUi/16/media-playback-start.png \
       QtUi/16/media-playback-stop.png \
       QtUi/16/media-playlist-repeat.png \
       QtUi/16/media-playlist-shuffle.png \
       QtUi/16/media-skip-backward.png \
       QtUi/16/media-skip-forward.png \
       QtUi/22/audio-volume-high.png \
       QtUi/22/audio-volume-low.png \
       QtUi/22/audio-volume-medium.png \
       QtUi/22/audio-volume-muted.png \
       QtUi/22/audio-volume-off.png \
       QtUi/22/document-open.png \
       QtUi/22/edit-find.png \
       QtUi/22/list-add.png \
       QtUi/22/media-playback-pause.png \
       QtUi/22/media-playback-start.png \
       QtUi/22/media-playback-stop.png \
       QtUi/22/media-playlist-repeat.png \
       QtUi/22/media-playlist-shuffle.png \
       QtUi/22/media-skip-backward.png \
       QtUi/22/media-skip-forward.png \
       QtUi/32/audio-volume-high.png \
       QtUi/32/audio-volume-low.png \
       QtUi/32/audio-volume-medium.png \
       QtUi/32/audio-volume-muted.png \
       QtUi/32/audio-volume-off.png \
       QtUi/32/document-open.png \
       QtUi/32/edit-find.png \
       QtUi/32/list-add.png \
       QtUi/32/media-playback-pause.png \
       QtUi/32/media-playback-start.png \
       QtUi/32/media-playback-stop.png \
       QtUi/32/media-playlist-repeat.png \
       QtUi/32/media-playlist-shuffle.png \
       QtUi/32/media-skip-backward.png \
       QtUi/32/media-skip-forward.png \
       QtUi/44/audio-volume-high.png \
       QtUi/44/audio-volume-low.png \
       QtUi/44/audio-volume-medium.png \
       QtUi/44/audio-volume-muted.png \
       QtUi/44/audio-volume-off.png \
       QtUi/44/document-open.png \
       QtUi/44/edit-find.png \
       QtUi/44/list-add.png \
       QtUi/44/media-playback-pause.png \
       QtUi/44/media-playback-start.png \
       QtUi/44/media-playback-stop.png \
       QtUi/44/media-playlist-repeat.png \
       QtUi/44/media-playlist-shuffle.png \
       QtUi/44/media-skip-backward.png \
       QtUi/44/media-skip-forward.png \
       QtUi/AUTHORS \
       QtUi/index.theme
endif

plugindir := ${plugindir}/${GENERAL_PLUGIN_DIR}

LD = ${CXX}

CFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS} -I../.. ${QT_CFLAGS}
LIBS += ${QT_LIBS} -laudqt

%.moc: %.h
	moc $< -o $@

rc_%.cc: %.qrc
	rcc $< -o $@
