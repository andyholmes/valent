subdir('xml')

private_headers = ['config.h']
foreach source : libvalent_private_sources + libvalent_private_headers
  private_headers += ['@0@/@1@'.format(meson.source_root(), source)]
endforeach


# XRef Documentation
glib_prefix = gio_dep.get_pkgconfig_variable('prefix')
glib_doc_path = join_paths(glib_prefix, 'share', 'gtk-doc', 'html')

gtk_prefix = gtk_dep.get_pkgconfig_variable('prefix')
gtk_doc_path = join_paths(gtk_prefix, 'share', 'gtk-doc', 'html')

jsonglib_prefix = jsonglib_dep.get_pkgconfig_variable('prefix')
jsonglib_doc_path = join_paths(jsonglib_prefix, 'share', 'gtk-doc', 'html')

libpeas_prefix = libpeas_dep.get_pkgconfig_variable('prefix')
libpeas_doc_path = join_paths(libpeas_prefix, 'share', 'gtk-doc', 'html')

eds_prefix = libebook_contacts_dep.get_pkgconfig_variable('prefix')
eds_doc_path = join_paths(eds_prefix, 'share', 'gtk-doc', 'html')


# Valent Docs
docpath = join_paths(datadir, 'gtk-doc', 'html', meson.project_name())

gnome.gtkdoc(
  meson.project_name(),
           main_xml: meson.project_name() + '-docs.xml',
            src_dir: [
              join_paths(meson.source_root(), 'src', 'libvalent', 'core'),
              join_paths(meson.build_root(),  'src', 'libvalent', 'core'),
              join_paths(meson.source_root(), 'src', 'libvalent', 'clipboard'),
              join_paths(meson.build_root(),  'src', 'libvalent', 'clipboard'),
              join_paths(meson.source_root(), 'src', 'libvalent', 'contacts'),
              join_paths(meson.build_root(),  'src', 'libvalent', 'contacts'),
              join_paths(meson.source_root(), 'src', 'libvalent', 'input'),
              join_paths(meson.build_root(),  'src', 'libvalent', 'input'),
              join_paths(meson.source_root(), 'src', 'libvalent', 'media'),
              join_paths(meson.build_root(),  'src', 'libvalent', 'media'),
              join_paths(meson.source_root(), 'src', 'libvalent', 'mixer'),
              join_paths(meson.build_root(),  'src', 'libvalent', 'mixer'),
              join_paths(meson.source_root(), 'src', 'libvalent', 'notifications'),
              join_paths(meson.build_root(),  'src', 'libvalent', 'notifications'),
              join_paths(meson.source_root(), 'src', 'libvalent', 'power'),
              join_paths(meson.build_root(),  'src', 'libvalent', 'power'),
              join_paths(meson.source_root(), 'src', 'libvalent', 'session'),
              join_paths(meson.build_root(),  'src', 'libvalent', 'session'),
              join_paths(meson.source_root(), 'src', 'libvalent', 'ui'),
              join_paths(meson.build_root(),  'src', 'libvalent', 'ui'),
            ],
       dependencies: [libvalent_dep],
            install: true,
     ignore_headers: private_headers,
  gobject_typesfile: meson.project_name() + '.types',
          scan_args: [
    '--rebuild-types',
  ],
       fixxref_args: [
     '--html-dir=@0@'.format(docpath),
    '--extra-dir=@0@'.format(join_paths(glib_doc_path, 'glib')),
    '--extra-dir=@0@'.format(join_paths(glib_doc_path, 'gmodule')),
    '--extra-dir=@0@'.format(join_paths(glib_doc_path, 'gobject')),
    '--extra-dir=@0@'.format(join_paths(glib_doc_path, 'gio')),
    '--extra-dir=@0@'.format(join_paths(gtk_doc_path, 'gdk')),
    '--extra-dir=@0@'.format(join_paths(gtk_doc_path, 'gsk')),
    '--extra-dir=@0@'.format(join_paths(gtk_doc_path, 'gtk')),
    '--extra-dir=@0@'.format(join_paths(jsonglib_doc_path, 'json-glib')),
    '--extra-dir=@0@'.format(join_paths(libpeas_doc_path, 'libpeas')),
    '--extra-dir=@0@'.format(join_paths(eds_doc_path, 'evolution-data-server')),
  ],
)

