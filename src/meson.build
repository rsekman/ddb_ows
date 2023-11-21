add_global_arguments('-DLIBDIR="'  + get_option('libdir') + '"', language : 'cpp')
add_global_arguments('-DDDB_OWS_LOGLEVEL=3', language : 'cpp')

gtk_dep = dependency('gtkmm-3.0')
uuid_dep = dependency('uuid')
fmt_dep = dependency('fmt')

conf = static_library('default_config',
  'default_config.S',
  c_args: '-DFNAME="default_config.json"'
)

shared_module('ddb_ows',
  'ddb_ows.cpp',
  'config.cpp',
  'database.cpp',
  'job.cpp',
  'jobsqueue.cpp',
  'logger.cpp',
  'playlist_uuid.cpp',
  install: true,
  install_dir: destdir,
  dependencies : [fmt_dep, uuid_dep],
  link_whole: conf,
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
  install: true,
  install_dir: destdir,
  dependencies : [gtk_dep, fmt_dep, uuid_dep],
  link_whole: conf,
  name_prefix: ''
)

install_data('ddb_ows.ui',
  install_dir: destdir
)
