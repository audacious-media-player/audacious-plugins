opengl_dep = dependency('GL', required: false)
have_glspectrum = opengl_dep.found()


if have_glspectrum
  shared_module('gl-spectrum',
    'gl-spectrum.cc',
    dependencies: [audacious_dep, math_dep, gtk_dep, glib_dep, opengl_dep],
    name_prefix: '',
    install: true,
    install_dir: visualization_plugin_dir
  )
endif
