# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#
# AppStream
#
metainfo_conf = configuration_data()
metainfo_conf.set('application_id', application_id)
metainfo_conf.set('gettext_package', meson.project_name())
metainfo_file = i18n.merge_file(
        input: configure_file(
                  input: files('ca.andyholmes.Valent.metainfo.xml.in.in'),
                 output: 'ca.andyholmes.Valent.metainfo.xml.in',
          configuration: metainfo_conf,
        ),
       output: '@0@.metainfo.xml'.format(application_id),
       po_dir: po_dir,
  install_dir: join_paths(datadir, 'metainfo'),
      install: true,
)

appstreamcli = find_program('appstreamcli', required: false)
if appstreamcli.found()
  test('Validate AppStream file', appstreamcli,
       args: ['validate', '--nonet', metainfo_file],
    depends: [metainfo_file],
  )
endif

