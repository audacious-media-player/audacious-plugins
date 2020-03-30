sidplayfp_dep = dependency('libsidplayfp', version: '>= 2.0', required: false)


if sidplayfp_dep.found()
  siddatadir = join_paths(get_option('prefix'), get_option('datadir'))

  shared_module('sid',
    'xmms-sid.cc',
    'xs_config.cc',
    'xs_sidplay2.cc',
    cpp_args: ['-DSIDDATADIR="@0@"'.format(siddatadir)],
    dependencies: [audacious_dep, sidplayfp_dep],
    install: true,
    install_dir: input_plugin_dir
  )
endif
