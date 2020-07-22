neon_dep = dependency('neon', version: '>= 0.27', required: false)


if neon_dep.found()
  shared_module('neon',
    'neon.cc',
    'cert_verification.cc',
    dependencies: [audacious_dep, neon_dep, glib_dep],
    install: true,
    install_dir: transport_plugin_dir
  )
endif
