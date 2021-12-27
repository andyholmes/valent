# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

libvalent_build = join_paths(meson.project_build_root(), 'src', 'libvalent')

# Get gi-docgen
gidocgen_dep = dependency('gi-docgen',
   version: '>= 2021.1',
  fallback: ['gi-docgen', 'dummy_dep'],
  required: true,
)

gidocgen = find_program('gi-docgen', required: true)

# Configuration
toml_conf = configuration_data()
toml_conf.set('version', meson.project_version())

valent_toml = configure_file(
          input: 'valent.toml.in',
         output: 'valent.toml',
  configuration: toml_conf
)

# Protocol Docs
protocol_py = find_program('protocol.py')
protocol_doc = custom_target('protocol-doc',
  input: join_paths(meson.project_source_root(), 'doc', 'schemas', 'index.json'),
  output: 'protocol.md',
  command: [
    protocol_py,
    '@INPUT@',
    '@OUTPUT@',
  ],
  build_by_default: true,
)

content_files = [
  # 'protocol.md',
]

custom_target('valent-doc',
  input: [valent_toml, libvalent_gir[0]],
  output: meson.project_name(),
  command: [
    gidocgen,
    'generate',
    '--quiet',
    '--fatal-warnings',
    '--add-include-path=@0@'.format(libvalent_build),
    '--config=@INPUT0@',
    '--output-dir=@OUTPUT@',
    '--no-namespace-dir',
    '--content-dir=@0@'.format(meson.current_build_dir()),
    '--content-dir=@0@'.format(meson.current_source_dir()),
    '@INPUT1@',
  ],
  depends: [libvalent_gir[0]],
  depend_files: [content_files],
  build_by_default: true,
  install: true,
  install_dir: docdir,
)

