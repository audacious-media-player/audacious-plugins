sidplayfp_dep = dependency('libsidplayfp', version: '>= 2.0', required: false)
have_sid = sidplayfp_dep.found()


if have_sid
  sid_datadir = join_paths(get_option('prefix'), get_option('datadir'))

  # libsidplayfp 2.0 is not c++17 clean yet, fixed with 2.1
  # https://sourceforge.net/p/sidplay-residfp/bugs/38/
  sid_override_options = []
  if sidplayfp_dep.version().version_compare('< 2.1')
    sid_override_options += 'cpp_std=gnu++11'
  endif

  shared_module('sid',
    'xmms-sid.cc',
    'xs_config.cc',
    'xs_sidplay2.cc',
    cpp_args: ['-DSIDDATADIR="@0@"'.format(sid_datadir)],
    override_options: sid_override_options,
    dependencies: [audacious_dep, sidplayfp_dep],
    name_prefix: '',
    install: true,
    install_dir: input_plugin_dir
  )
endif
