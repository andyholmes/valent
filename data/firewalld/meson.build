# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>


# We don't use `libdir` here, because firewalld has a specific path
# See: https://firewalld.org/documentation/man-pages/firewalld.service.html
if get_option('firewalld')
  install_data('ca.andyholmes.Valent.xml',
    install_dir: join_paths(prefix, 'lib', 'firewalld', 'services')
  )
endif

