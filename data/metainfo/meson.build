# AppStream
metainfo_conf = configuration_data()
metainfo_conf.set('application_id', application_id)
metainfo_conf.set('gettext_package', meson.project_name())
metainfo_file = i18n.merge_file('metainfo',
  input: configure_file(
    input: files('ca.andyholmes.Valent.metainfo.xml.in.in'),
    output: 'ca.andyholmes.Valent.metainfo.xml.in',
    configuration: metainfo_conf
  ),
  output: '@0@.metainfo.xml'.format(application_id),
  install: true,
  install_dir: join_paths(datadir, 'metainfo'),
  po_dir: po_dir
)

appstream_util = find_program('appstream-util', required: false)
if appstream_util.found()
  validate_args = ['validate-relax', metainfo_file]

  if not get_option('network_tests')
    validate_args += '--nonet'
  endif

  test('Validate AppStream file', appstream_util, args: validate_args)
endif

