# Appdata file.
appdata_conf = configuration_data()
appdata_conf.set('application_id', application_id)
appdata_file = i18n.merge_file(
  'appdata',
  input: configure_file(
    input: files('ca.andyholmes.Valent.appdata.xml.in.in'),
    output: 'ca.andyholmes.Valent.appdata.xml.in',
    configuration: appdata_conf
  ),
  output: '@0@.appdata.xml'.format(application_id),
  install: true,
  install_dir: join_paths(datadir, 'metainfo'),
  po_dir: po_dir
)

appstream_util = find_program('appstream-util', required: false)
if appstream_util.found()
  validate_args = ['validate-relax', appdata_file]

  if not get_option('network_tests')
    validate_args += '--nonet'
  endif

  test('Validate appstream file', appstream_util, args: validate_args)
endif

