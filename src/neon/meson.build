neon_dep = dependency('neon', version: '>= 0.27', required: false)
have_neon = neon_dep.found()

if have_neon
  shared_module('neon',
    'neon.cc',
    'cert_verification.cc',
    dependencies: [audacious_dep, neon_dep, glib_dep],
    name_prefix: '',
    install: true,
    install_dir: transport_plugin_dir
  )
endif
