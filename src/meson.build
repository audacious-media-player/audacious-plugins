src_inc = include_directories('.')


config_h_full_path = join_paths(meson.current_build_dir(), 'config.h')


if cc.get_id() in ['gcc', 'clang']
  add_project_arguments('-include', config_h_full_path, language: ['c', 'cpp'])

  if have_darwin
    add_project_arguments('-include', config_h_full_path, language: ['objc', 'objcpp'])
  endif
else
  error('Please implement -include handling for your chosen compiler.')
endif


# input plugins
subdir('metronom')
subdir('psf')
subdir('tonegen')
subdir('vtx')
subdir('xsf')

if get_option('console')
  subdir('console')
endif


# effect plugins
subdir('bitcrusher')
subdir('compressor')
subdir('crossfade')
subdir('crystalizer')
subdir('echo_plugin')
subdir('mixer')
subdir('silence-removal')
subdir('stereo_plugin')
subdir('voice_removal')

if get_option('bs2b')
  subdir('bs2b')
endif

if get_option('resample')
  subdir('resample')
endif

if get_option('soxr')
  subdir('soxr')
endif

if get_option('speedpitch')
  subdir('speedpitch')
endif


# transport plugins
subdir('gio')

if get_option('mms')
  subdir('mms')
endif

if get_option('neon')
  subdir('neon')
endif


# playlist (container) plugins
subdir('asx')
subdir('asx3')
subdir('audpl')
subdir('m3u')
subdir('pls')
subdir('xspf')

if get_option('cue')
  subdir('cue')
endif


# Qt/GTK-specific plugins
if conf.has('USE_GTK_OR_QT')
  subdir('delete-files')
  subdir('skins-data')

  if get_option('notify')
    subdir('notify')
  endif
endif


# Qt-specific plugins
if conf.has('USE_QT')
  subdir('albumart-qt')
  subdir('blur_scope-qt')
  subdir('lyricwiki-qt')
  subdir('playlist-manager-qt')
  subdir('qt-spectrum')
  subdir('qtui')
  subdir('search-tool-qt')
  subdir('skins-qt')
  subdir('song-info-qt')
  subdir('statusicon-qt')

  if get_option('ampache')
    subdir('ampache')
  endif

  if get_option('gl-spectrum')
    subdir('qtglspectrum')
  endif

  if get_option('hotkey')
    subdir('qthotkey')
  endif

  if get_option('moonstone')
    subdir('moonstone')
  endif

  if get_option('qtaudio')
    subdir('qtaudio')
  endif

  if get_option('streamtuner')
    subdir('streamtuner')
  endif

  if get_option('vumeter')
    subdir('vumeter-qt')
  endif
endif


# GTK-specific plugins
if conf.has('USE_GTK')
  subdir('albumart')
  subdir('blur_scope')
  subdir('cairo-spectrum')
  subdir('gtkui')
  subdir('ladspa')
  subdir('playlist-manager')
  subdir('search-tool')
  subdir('skins')
  subdir('statusicon')

  if get_option('aosd')
    subdir('aosd')
  endif

  if get_option('gl-spectrum') and not have_darwin
    subdir('glspectrum')
  endif

  if get_option('hotkey')
    subdir('hotkey')
  endif
endif


# general plugins
if get_option('lirc')
  subdir('lirc')
endif

if get_option('mac-media-keys') and have_darwin
  subdir('mac-media-keys')
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

if get_option('ffaudio')
  subdir('ffaudio')
endif

if get_option('flac')
  subdir('flac')
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

if get_option('sid')
  subdir('sid')
endif

if get_option('sndfile')
  subdir('sndfile')
endif

if get_option('vorbis')
  subdir('vorbis')
endif

if get_option('wavpack')
  subdir('wavpack')
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

if have_windows
  subdir('waveout')
endif


# config.h stuff
configure_file(input: 'config.h.meson',
  output: 'config.h',
  configuration: conf)
