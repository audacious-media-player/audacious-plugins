mpg123_dep = dependency('libmpg123', version: '>= 1.12', required: false)
have_mpg123 = mpg123_dep.found()


if have_mpg123
  shared_module('madplug',
    'mpg123.cc',
    dependencies: [audacious_dep, mpg123_dep, audtag_dep],
    name_prefix: '',
    include_directories: [src_inc],
    install: true,
    install_dir: input_plugin_dir,
  )
endif
