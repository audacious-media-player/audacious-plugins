opengl_dep = dependency('GL', required: false)


if opengl_dep.found()
  shared_module('gl-spectrum',
    'gl-spectrum.cc',
    dependencies: [audacious_dep, math_dep, gtk_dep, glib_dep, opengl_dep],
    install: true,
    install_dir: visualization_plugin_dir
  )
endif
