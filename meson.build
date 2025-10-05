project('ddb_ows', 'cpp', 'c',
  default_options : ['warning_level=1', 'cpp_std=c++17'],
  version : '0.6.0',
  meson_version : '>=1.1'
)

destdir = get_option('libdir') / 'deadbeef'

add_global_arguments('-DDDB_OWS_LOGLEVEL=' + get_option('log-level'), language : 'cpp')
version_parts = meson.project_version().split('.')
add_global_arguments('-DDDB_OWS_VERSION="' + meson.project_version() +'"', language : 'cpp')
add_global_arguments('-DDDB_OWS_VERSION_MAJOR=' + version_parts[0], language : 'cpp')
add_global_arguments('-DDDB_OWS_VERSION_MINOR=' + version_parts[1], language : 'cpp')
add_global_arguments('-DDDB_OWS_VERSION_PATCH=' + version_parts[2], language : 'cpp')

subdir('src')
