glspectrum_sources = []
glspectrum_deps = [audacious_dep, gtk_dep]

if get_option('gtk2')
  glspectrum_sources += 'gl-spectrum-gtk2.cc'
  opengl_dep = dependency('GL', required: false)
  glspectrum_deps += [opengl_dep, x11_dep]
  have_glspectrum = opengl_dep.found() and not have_darwin and (have_windows or x11_dep.found())
else
  glspectrum_sources += 'gl-spectrum.cc'
  epoxy_dep = dependency('epoxy', required: false)
  glspectrum_deps += [epoxy_dep, math_dep]
  have_glspectrum = epoxy_dep.found()
endif

if have_glspectrum
  shared_module('gl-spectrum',
    glspectrum_sources,
    dependencies: glspectrum_deps,
    name_prefix: '',
    install: true,
    install_dir: visualization_plugin_dir
  )
endif
