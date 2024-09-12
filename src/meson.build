add_global_arguments('-DLIBDIR="'  + get_option('libdir') + '"', language : 'cpp')
add_global_arguments('-DDDB_OWS_LOGLEVEL=3', language : 'cpp')

gio_dep = dependency('giomm-2.4')
gtk_dep = dependency('gtkmm-3.0')
uuid_dep = dependency('uuid')
fmt_dep = dependency('fmt')

incdir = include_directories('../include')

gnome = import('gnome')
resources = gnome.compile_resources('ddb_ows_resources',
  'resources.xml'
)
gui_resources = gnome.compile_resources('ddb_ows_gui_resources',
  'gui_resources.xml'
)

shared_module('ddb_ows',
  'ddb_ows.cpp',
  'config.cpp',
  'database.cpp',
  'job.cpp',
  'jobsqueue.cpp',
  'logger.cpp',
  'playlist_uuid.cpp',
  resources,
  include_directories: incdir,
  install: true,
  install_dir: destdir,
  dependencies : [gio_dep, fmt_dep, uuid_dep],
  name_prefix: ''
)

shared_module('ddb_ows_gtk3', 'ddb_ows_gui.cpp',
  'config.cpp',
  'database.cpp',
  'job.cpp',
  'jobsqueue.cpp',
  'logger.cpp',
  'playlist_uuid.cpp',
  'textbufferlogger.cpp',
  'progressmonitor.cpp',
  gui_resources,
  include_directories: incdir,
  install: true,
  install_dir: destdir,
  dependencies : [gtk_dep, fmt_dep, uuid_dep],
  name_prefix: ''
)
