src_inc = include_directories('.')


# effect plugins
if get_option('background-music')
  subdir('background_music')
endif

if get_option('bitcrusher')
  subdir('bitcrusher')
endif

if get_option('bs2b')
  subdir('bs2b')
endif

if get_option('compressor')
  subdir('compressor')
endif

if get_option('crossfade')
  subdir('crossfade')
endif

if get_option('crystalizer')
  subdir('crystalizer')
endif

if get_option('echo')
  subdir('echo_plugin')
endif

if get_option('mixer')
  subdir('mixer')
endif

if get_option('resample')
  subdir('resample')
endif

if get_option('silence-removal')
  subdir('silence-removal')
endif

if get_option('soxr')
  subdir('soxr')
endif

if get_option('speedpitch')
  subdir('speedpitch')
endif

if get_option('stereo')
  subdir('stereo_plugin')
endif

if get_option('voice-removal')
  subdir('voice_removal')
endif


# transport plugins
if get_option('gio')
  subdir('gio')
endif

if get_option('mms')
  subdir('mms')
endif

if get_option('neon')
  subdir('neon')
endif


# playlist plugins
if get_option('asx')
  subdir('asx')
endif

if get_option('asx3')
  subdir('asx3')
endif

subdir('audpl')

if get_option('cue')
  subdir('cue')
endif

if get_option('m3u')
  subdir('m3u')
endif

if get_option('pls')
  subdir('pls')
endif

if get_option('xspf')
  subdir('xspf')
endif


# Qt/GTK-specific plugins
if conf.has('USE_GTK_OR_QT')
  if get_option('delete-files')
    subdir('delete-files')
  endif

  if get_option('notify')
    subdir('notify')
  endif

  if get_option('skins')
    subdir('skins-data')
  endif
endif


# Qt-specific plugins
if conf.has('USE_QT')
  if get_option('albumart')
    subdir('albumart-qt')
  endif

  if get_option('ampache')
    subdir('ampache')
  endif

  if get_option('blurscope')
    subdir('blur_scope-qt')
  endif

  if get_option('filebrowser')
    subdir('filebrowser-qt')
  endif

  if get_option('gl-spectrum')
    subdir('qtglspectrum')
  endif

  if get_option('hotkey') and not (have_darwin or have_windows or have_cygwin)
    subdir('qthotkey')
  endif

  if get_option('lyrics')
    subdir('lyrics-qt')
  endif

  if get_option('playback-history')
    subdir('playback-history-qt')
  endif

  if get_option('playlist-manager')
    subdir('playlist-manager-qt')
  endif

  if get_option('qtaudio')
    subdir('qtaudio')
  endif

  if get_option('qtui')
    subdir('qtui')
  endif

  if get_option('searchtool')
    subdir('search-tool-qt')
  endif

  if get_option('skins')
    subdir('skins-qt')
  endif

  if get_option('songinfo')
    subdir('song-info-qt')
  endif

  if get_option('spectrum-analyzer')
    subdir('qt-spectrum')
  endif

  if get_option('statusicon')
    subdir('statusicon-qt')
  endif

  if get_option('streamtuner')
    subdir('streamtuner')
  endif

  if get_option('vumeter')
    subdir('vumeter-qt')
  endif

  if get_option('waveform')
    subdir('waveform-qt')
  endif
endif


# GTK-specific plugins
if conf.has('USE_GTK')
  if get_option('albumart')
    subdir('albumart')
  endif

  if get_option('aosd')
    subdir('aosd')
  endif

  if get_option('blurscope')
    subdir('blur_scope')
  endif

  if get_option('filebrowser')
    subdir('filebrowser')
  endif

  if get_option('gl-spectrum') and not have_darwin
    subdir('glspectrum')
  endif

  if get_option('gtkui')
    subdir('gtkui')
  endif

  if get_option('hotkey')
    subdir('hotkey')
  endif

  if get_option('ladspa')
    subdir('ladspa')
  endif

  if get_option('lyrics')
    subdir('lyrics-gtk')
  endif

  if get_option('playback-history')
    subdir('playback-history')
  endif

  if get_option('playlist-manager')
    subdir('playlist-manager')
  endif

  if get_option('searchtool')
    subdir('search-tool')
  endif

  if get_option('skins')
    subdir('skins')
  endif

  if get_option('spectrum-analyzer')
    subdir('cairo-spectrum')
  endif

  if get_option('statusicon')
    subdir('statusicon')
  endif

  if get_option('vumeter')
    subdir('vumeter')
  endif

  # TODO: add waveform for GTK
endif


# general plugins
if get_option('lirc')
  subdir('lirc')
endif

if get_option('mac-now-playing') and have_darwin
  subdir('mac-now-playing')
endif

if get_option('mpris2') and not have_windows
  subdir('mpris2')
endif

if get_option('scrobbler2')
  subdir('scrobbler2')
endif

if get_option('songchange')
  subdir('songchange')
endif


# input plugins
if get_option('aac')
  subdir('aac')
endif

if get_option('adplug')
  subdir('adplug')
endif

if get_option('amidiplug')
  subdir('amidiplug')
endif

if get_option('cdaudio')
  subdir('cdaudio')

  if have_cdaudio
    subdir('cd-menu-items')
  endif
endif

if get_option('console')
  subdir('console')
endif

if get_option('ffaudio')
  subdir('ffaudio')
endif

if get_option('flac')
  subdir('flac')
endif

if get_option('metronome')
  subdir('metronom')
endif

if get_option('modplug')
  subdir('modplug')
endif

if get_option('mpg123')
  subdir('mpg123')
endif

if get_option('openmpt')
  subdir('openmpt')
endif

if get_option('opus')
  subdir('opus')
endif

if get_option('psf')
  subdir('psf')
endif

if get_option('sid')
  subdir('sid')
endif

if get_option('sndfile')
  subdir('sndfile')
endif

if get_option('tonegen')
  subdir('tonegen')
endif

if get_option('vorbis')
  subdir('vorbis')
endif

if get_option('vtx')
  subdir('vtx')
endif

if get_option('wavpack')
  subdir('wavpack')
endif

if get_option('xsf')
  subdir('xsf')
endif


# output plugins
if get_option('alsa')
  subdir('alsa')
endif

if get_option('coreaudio') and have_darwin
  subdir('coreaudio')
endif

if get_option('filewriter')
  subdir('filewriter')
endif

if get_option('jack')
  subdir('jack')
endif

if get_option('oss')
  subdir('oss4')
endif

if get_option('pipewire')
  subdir('pipewire')
endif

if get_option('pulse')
  subdir('pulse')
endif

if get_option('sdlout')
  subdir('sdlout')
endif

if get_option('sndio')
  subdir('sndio')
endif

if have_windows or have_cygwin
  subdir('waveout')
endif


# config.h stuff
configure_file(input: '../config.h.meson',
  output: 'config.h',
  configuration: conf
)
