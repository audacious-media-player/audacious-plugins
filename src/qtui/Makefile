PLUGIN = qtui${PLUGIN_SUFFIX}

SRCS = qtui.cc \
       main_window.cc \
       main_window.moc \
       playlist.cc \
       playlist.moc \
       playlist_model.cc \
       playlist_model.moc \
       playlist_tabs.cc \
       playlist_tabs.moc \
       filter_input.cc \
       filter_input.moc \
       ui_main_window.h \
       ui_playlist_tabs.h \
       utils.cc

include ../../buildsys.mk
include ../../extra.mk

# These overrides variables in buildsys.mk and so must come after it.
PACKAGE_NAME = audacious
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
       QtUi/AUTHORS \
       QtUi/index.theme

plugindir := ${plugindir}/${GENERAL_PLUGIN_DIR}

LD = ${CXX}

CFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS} -I../.. ${QT_CFLAGS}
LIBS += ${QT_LIBS} -laudqt

%.moc: %.h
	moc $< -o $@

ui_%.h: %.ui
	uic $< -o $@

rc_%.cc: %.qrc
	rcc $< -o $@
