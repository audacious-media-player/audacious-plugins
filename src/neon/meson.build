neon_dep = dependency('neon', version: '>= 0.27', required: get_option('neon'))
have_neon = neon_dep.found()

if have_neon
  shared_module('neon',
    'neon.cc',
    'cert_verification.cc',
    dependencies: [audacious_dep, neon_dep, glib_dep],
    name_prefix: '',
    link_args: have_windows ? ['-lcrypt32'] : [],
    install: true,
    install_dir: transport_plugin_dir
  )
endif
