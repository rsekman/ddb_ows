gui_resources = gnome.compile_resources('ddb_ows_gui_resources',
  'resources.xml'
)

shared_module('ddb_ows_gtk3', 'ddb_ows_gui.cpp',
  'textbufferlogger.cpp',
  'progressmonitor.cpp',
  gui_resources,
  include_directories: incdir,
  install: true,
  install_dir: destdir,
  dependencies : [gtk_dep, fmt_dep, uuid_dep],
  link_with: lib,
  name_prefix: ''
)
