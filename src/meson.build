src_inc = include_directories('.')


config_h_full_path = join_paths(meson.current_build_dir(), 'config.h')


if cc.get_id() == 'gcc' or cc.get_id() == 'clang'
  add_project_arguments('-include', config_h_full_path, language: 'c')
  add_project_arguments('-include', config_h_full_path, language: 'cpp')
else
  error('Please implement -include handling for your chosen compiler.')
endif


# input plugins
subdir('metronom')
subdir('psf')
subdir('tonegen')
subdir('vtx')
subdir('xsf')
subdir('console')


# effect plugins
subdir('compressor')
subdir('crossfade')
subdir('crystalizer')
subdir('mixer')
subdir('silence-removal')
subdir('stereo_plugin')
subdir('voice_removal')
subdir('echo_plugin')


# transport plugins
subdir('gio')
if get_option('neon')
  subdir('neon')
endif
if get_option('mms')
  subdir('mms')
endif


# playlist (container) plugins
subdir('audpl')
subdir('asx')
subdir('m3u')
subdir('pls')
if xml_dep.found()
  subdir('asx3')
  subdir('xspf')
endif


# Qt-specific plugins
if get_option('qt')
  subdir('blur_scope-qt')
  subdir('qt-spectrum')

  subdir('qtui')
  subdir('skins-qt')

  subdir('albumart-qt')
  subdir('lyricwiki-qt')
  subdir('playlist-manager-qt')
  subdir('search-tool-qt')
  subdir('song-info-qt')
  subdir('statusicon-qt')

  subdir('delete-files')
  subdir('skins-data')

  subdir('qtaudio')
endif


# output plugins
if get_option('pulse')
  subdir('pulse')
endif
if host_machine.system() == 'linux' and get_option('alsa')
  subdir('alsa')
endif

# input plugins
if get_option('mpg123')
  subdir('mpg123')
endif

if get_option('ffaudio') in ['libav', 'ffmpeg']
  subdir('ffaudio')
endif

if get_option('adplug')
  subdir('adplug')
endif

if get_option('flac')
  subdir('flac')
endif

if get_option('vorbis')
  subdir('vorbis')
endif

if get_option('faad')
  subdir('aac')
endif

if get_option('modplug')
  subdir('modplug')
endif

if get_option('openmpt')
  subdir('openmpt')
endif

if get_option('wavpack')
  subdir('wavpack')
endif

if get_option('sndfile')
  subdir('sndfile')
endif


# config.h stuff
configure_file(input: 'config.h.meson',
  output: 'config.h',
  configuration: conf)
