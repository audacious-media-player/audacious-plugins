config_h = configure_file(input: 'config.h.meson',
  output: 'config.h',
  configuration: conf)


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


# playlist (container) plugins
subdir('audpl')
subdir('asx')
subdir('m3u')
subdir('pls')
if xml_dep.found()
  subdir('asx3')
  subdir('xspf')
endif
