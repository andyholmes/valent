libvalent_power_header_dir = join_paths(libvalent_header_dir, 'power')
libvalent_power_header_subdir = join_paths(libvalent_header_subdir, 'power')
libvalent_include_directories += include_directories('.')
libvalent_power_generated_headers = []
libvalent_power_generated_sources = []


# Headers
libvalent_power_public_headers = [
  'libvalent-power.h',
  'valent-power.h',
  'valent-power-device.h',
  'valent-power-device-provider.h',
]

libvalent_power_private_headers = [
]

libvalent_power_enum_headers = [
  'valent-power-device.h',
]

install_headers(libvalent_power_public_headers,
  subdir: libvalent_power_header_subdir
)


# Sources
libvalent_power_public_sources = [
  'valent-power.c',
  'valent-power-device.c',
  'valent-power-device-provider.c',
]


# Enumerations
libvalent_power_enums = gnome.mkenums_simple('valent-power-enums',
     body_prefix: '#include "config.h"',
   header_prefix: '#include <libvalent-core.h>',
         sources: libvalent_power_enum_headers,
  install_header: true,
     install_dir: libvalent_power_header_dir,
)
libvalent_power_generated_sources += [libvalent_power_enums[0]]
libvalent_power_generated_headers += [libvalent_power_enums[1]]


# Dependencies
libvalent_power_deps = [
  libvalent_core_dep,
]


# Library Definitions
libvalent_power = static_library('valent-power-' + valent_api_version,
                                 libvalent_power_public_sources,
                                 libvalent_power_generated_sources,
                                 libvalent_power_generated_headers,
  dependencies: libvalent_power_deps,
        c_args: libvalent_c_args + release_args + ['-DVALENT_POWER_COMPILATION'],
)

libvalent_power_dep = declare_dependency(
              sources: libvalent_power_private_headers,
         dependencies: libvalent_power_deps,
            link_with: libvalent_power,
  include_directories: include_directories('.'),
)


libvalent_static += [libvalent_power]
libvalent_deps += [libvalent_power_dep]
libvalent_public_sources += files(libvalent_power_public_sources)
libvalent_public_headers += files(libvalent_power_public_headers)
libvalent_private_headers += files(libvalent_power_private_headers)
libvalent_generated_headers += libvalent_power_generated_headers
libvalent_generated_sources += libvalent_power_generated_sources
libvalent_include_subdirs += libvalent_power_header_subdir
libvalent_gir_extra_args += ['--c-include=libvalent-power.h', '-DVALENT_POWER_COMPILATION']

