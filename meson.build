project('audacious-plugins', 'c', 'cpp',
        version: '4.3',
        meson_version: '>= 0.51',
        default_options: [
          'c_std=gnu99',
          'cpp_std=gnu++17',
          'warning_level=1'
        ])


copyright = 'Copyright (C) 2001-2023 Audacious developers and others'


have_darwin = host_machine.system() == 'darwin'
have_windows = host_machine.system() == 'windows'


cc = meson.get_compiler('c')
cxx = meson.get_compiler('cpp')

if have_darwin
  add_languages('objc', 'objcpp')

  objc = meson.get_compiler('objc')
  objcpp = meson.get_compiler('objcpp')
endif


audacious_req = '>= 4.3'
audacious_dep = dependency('audacious', version: audacious_req, required: true)


# XXX - make this its own .pc file
audtag_dep = declare_dependency(link_args: [
  '-L@0@'.format(audacious_dep.get_variable(pkgconfig: 'lib_dir')),
  '-laudtag'
])


# XXX - make this its own .pc file
audqt_dep = declare_dependency(link_args: [
  '-L@0@'.format(audacious_dep.get_variable(pkgconfig: 'lib_dir')),
  '-laudqt'
])


# XXX - make this its own .pc file
audgui_dep = declare_dependency(link_args: [
  '-L@0@'.format(audacious_dep.get_variable(pkgconfig: 'lib_dir')),
  '-laudgui'
])


glib_req = '>= 2.32'
glib_dep = dependency('glib-2.0', version: glib_req, required: true)
gmodule_dep = dependency('gmodule-2.0', version: glib_req, required: true)
zlib_dep = dependency('zlib', required: true)

math_dep = cxx.find_library('m', required: false)
samplerate_dep = dependency('samplerate', required: false)
xml_dep = dependency('libxml-2.0', required: false)


if get_option('qt')
  if get_option('qt6')
    qt_req = '>= 6.0'
    qt_dep = dependency('qt6', version: qt_req, required: true, modules: ['Core', 'Widgets', 'Gui'])
  else
    qt_req = '>= 5.2'
    qt_dep = dependency('qt5', version: qt_req, required: true, modules: ['Core', 'Widgets', 'Gui'])
  endif
endif


if get_option('gtk')
  if get_option('gtk3')
    gtk_req = '>= 3.22'
    gtk_dep = dependency('gtk+-3.0', version: gtk_req, required: true)
  else
    gtk_req = '>= 2.24'
    gtk_dep = dependency('gtk+-2.0', version: gtk_req, required: true)
  endif
endif


if cc.get_id() in ['gcc', 'clang']
  common_flags = [
    '-ffast-math',
    '-Wtype-limits',
    '-Wno-stringop-truncation',
    '-fvisibility=hidden'
  ]

  cxx_flags = [
    '-Wno-non-virtual-dtor',
    '-Woverloaded-virtual'
  ]

  check_cflags = common_flags
  check_cxxflags = common_flags + cxx_flags

  add_project_arguments(cc.get_supported_arguments(check_cflags), language: 'c')
  add_project_arguments(cxx.get_supported_arguments(check_cxxflags), language: 'cpp')

  if have_darwin
    add_project_arguments(objc.get_supported_arguments(check_cflags), language: 'objc')
    add_project_arguments(objcpp.get_supported_arguments(check_cxxflags), language: 'objcpp')
  endif
endif


conf = configuration_data()
conf.set_quoted('COPYRIGHT', copyright)
conf.set_quoted('PACKAGE', meson.project_name())
conf.set_quoted('VERSION', meson.project_version())
conf.set('PACKAGE_VERSION', meson.project_version())
if host_machine.endian() == 'big'
  conf.set10('WORDS_BIGENDIAN', true)
endif


# XXX - investigate to see if we can do better
if have_windows
  conf.set_quoted('PLUGIN_SUFFIX', '.dll')
elif have_darwin
  conf.set_quoted('PLUGIN_SUFFIX', '.dylib')
else
  conf.set_quoted('PLUGIN_SUFFIX', '.so')
endif


if have_windows
  conf.set('EXPORT', '__declspec(dllexport)')
elif cxx.has_argument('-fvisibility=default')
  conf.set('EXPORT', '__attribute__((visibility("default")))')
else
  error('Could not define EXPORT keyword for public symbols.')
endif


# XXX - why do we have to define this manually?
if (cxx.has_header('libintl.h'))
  add_project_arguments('-DHAVE_GETTEXT', language: ['c', 'cpp'])

  if have_darwin or have_windows
    add_project_link_arguments('-lintl', language: ['c', 'cpp'])
  endif
endif


install_plugindir = audacious_dep.get_variable(pkgconfig: 'plugin_dir')


conf.set_quoted('INSTALL_PLUGINDIR', install_plugindir)
conf.set('plugindir', install_plugindir)


container_plugin_dir = join_paths(install_plugindir, 'Container')
effect_plugin_dir = join_paths(install_plugindir, 'Effect')
general_plugin_dir = join_paths(install_plugindir, 'General')
input_plugin_dir = join_paths(install_plugindir, 'Input')
output_plugin_dir = join_paths(install_plugindir, 'Output')
transport_plugin_dir = join_paths(install_plugindir, 'Transport')
visualization_plugin_dir = join_paths(install_plugindir, 'Visualization')


if get_option('qt')
  conf.set10('USE_QT', true)
endif


if get_option('gtk')
  conf.set10('USE_GTK', true)
  if get_option('gtk3')
    conf.set10('USE_GTK3', true)
  endif
endif


if get_option('qt') or get_option('gtk')
  conf.set10('USE_GTK_OR_QT', true)
endif


subdir('src')
subdir('po')


if meson.version().version_compare('>= 0.53')
  summary({
    'Prefix': get_option('prefix'),
    'Bin dir': get_option('bindir'),
    'Lib dir': get_option('libdir'),
    'Data dir': get_option('datadir'),
  }, section: 'Directories')

  summary({
    'Qt 5 support': get_option('qt') and not get_option('qt6'),
    'Qt 6 support': get_option('qt6'),
    'GTK2 support': get_option('gtk') and not get_option('gtk3'),
    'GTK3 support': get_option('gtk3'),
  }, section: 'GUI Toolkits')

  summary({
    'Audio CD': get_variable('have_cdaudio', false),
    'Free Lossless Audio Codec': get_variable('have_flac', false),
    'Ogg Vorbis': get_variable('have_vorbis', false),
    'Opus': get_variable('have_opus', false),
    'MIDI (via FluidSynth)': get_variable('have_amidiplug', false),
    'MPEG-1 Layer I/II/III (via mpg123)': get_variable('have_mpg123', false),
    'MPEG-2/4 AAC': get_variable('have_aac', false),
    'WavPack': get_variable('have_wavpack', false),
  }, section: 'Audio Formats')

  summary({
    'FFmpeg': get_variable('have_ffaudio', false),
    'Libsndfile': get_variable('have_sndfile', false),
  }, section: 'External Decoders')

  summary({
    'AdLib synthesizer (adplug)': get_variable('have_adplug', false),
    'Commodore 64 audio (sid)': get_variable('have_sid', false),
    'Game Music Emu (spc, nsf, gbs, etc.)': get_option('console'),
    'ModPlug': get_variable('have_modplug', false),
    'OpenMPT': get_variable('have_openmpt', false),
    'Nintendo DS audio (xsf)': true,
    'PlayStation audio (psf/psf2)': true,
    'Vortex Tracker (vtx)': true,
  }, section: 'Chiptunes')

  summary({
    'Metronome': true,
    'Tone Generator': true,
  }, section: 'Other Inputs')

  summary({
    'Bauer stereophonic-to-binaural (bs2b)': get_variable('have_bs2b', false),
    'Bitcrusher': true,
    'Channel Mixer': true,
    'Crystalizer': true,
    'Dynamic Range Compressor': true,
    'Echo/Surround': true,
    'Extra Stereo': true,
    'LADSPA Host (requires GTK)': conf.has('USE_GTK'),
    'Sample Rate Converter': get_variable('have_resample', false),
    'Silence Removal': true,
    'SoX Resampler': get_variable('have_soxr', false),
    'Speed and Pitch': get_variable('have_speedpitch', false),
    'Voice Removal': true,
  }, section: 'Effects')

  summary({
    'Advanced Linux Sound Architecture': get_variable('have_alsa', false),
    'Jack Audio Connection Kit': get_variable('have_jack', false),
    'Open Sound System': get_variable('have_oss4', false),
    'PipeWire': get_variable('have_pipewire', false),
    'PulseAudio': get_variable('have_pulse', false),
    'Simple DirectMedia Layer': get_variable('have_sdlout', false),
    'Sndio': get_variable('have_sndio', false),
    'Win32 waveOut': have_windows,
    'FileWriter': get_option('filewriter'),
    '  -> MP3 encoding': conf.has('FILEWRITER_MP3'),
    '  -> Vorbis encoding': conf.has('FILEWRITER_VORBIS'),
    '  -> FLAC encoding': conf.has('FILEWRITER_FLAC'),
  }, section: 'Outputs')

  summary({
    'Cue sheets': get_variable('have_cue', false),
    'M3U playlists': true,
    'Microsoft ASX (legacy)': true,
    'Microsoft ASX 3.0': get_variable('have_asx3', false),
    'PLS playlists': true,
    'XML Sharable Playlist Format (XSPF)': get_variable('have_xspf', false),
  }, section: 'Playlists')

  summary({
    'FTP, SFTP, SMB (via GIO)': true,
    'HTTP/HTTPS (via neon)': get_variable('have_neon', false),
    'MMS (via libmms)': get_variable('have_mms', false),
  }, section: 'Transports')

  summary({
    'Ampache browser (requires Qt)': get_variable('have_ampache', false),
    'Delete Files': conf.has('USE_GTK_OR_QT'),
    'Libnotify OSD': get_variable('have_notify', false),
    'Linux Infrared Remote Control (LIRC)': get_variable('have_lirc', false),
    'MPRIS 2 Server': get_variable('have_mpris2', false),
    'Scrobbler 2.0': get_variable('have_scrobbler2', false),
    'Song Change': get_option('songchange'),
  }, section: 'General')

  if conf.has('USE_QT')
    summary({
      'Qt Multimedia output': get_variable('have_qtaudio', false),
      'Qt Interface': true,
      'Winamp Classic Interface': true,
      'Album Art': true,
      'Blur Scope': true,
      'Lyrics Viewer': get_variable('have_lyrics', false),
      'OpenGL Spectrum Analyzer': get_variable('have_qtglspectrum', false),
      'Playlist Manager': true,
      'Search Tool': true,
      'Song Info': true,
      'Spectrum Analyzer (2D)': true,
      'Status Icon': true,
      'Stream Tuner (experimental)': get_variable('have_streamtuner', false),
      'VU Meter': get_option('vumeter'),
      'X11 Global Hotkeys': get_variable('have_qthotkey', false),
    }, section: 'Qt Support')
  endif

  if conf.has('USE_GTK')
    summary({
      'GTK Interface': true,
      'Winamp Classic Interface': true,
      'Album Art': true,
      'Blur Scope': true,
      'OpenGL Spectrum Analyzer': get_variable('have_glspectrum', false),
      'Playlist Manager': true,
      'Search Tool': true,
      'Spectrum Analyzer (2D)': true,
      'Status Icon': true,
      'X11 Global Hotkeys': get_variable('have_hotkey', false),
      'X11 On-Screen Display (aosd)': get_variable('have_aosd', false),
    }, section: 'GTK Support')
  endif

  if have_darwin
    summary({
      'CoreAudio Output': get_option('coreaudio'),
      'Media Keys': get_option('mac-media-keys'),
    }, section: 'Mac OS X Support')
  endif
endif
