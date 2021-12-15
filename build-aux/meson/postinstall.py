#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

# pylint: disable=missing-module-docstring

import os
import subprocess

prefix = os.environ.get('MESON_INSTALL_PREFIX', '/usr/local')
datadir = os.path.join(prefix, 'share')

# If DESTDIR is defined, a package manager will be handling this
if 'DESTDIR' not in os.environ:
    print('Compiling GSettings schemas...')
    subprocess.call(['glib-compile-schemas',
                     os.path.join(datadir, 'glib-2.0', 'schemas')])

    print('Updating icon cache...')
    subprocess.call(['gtk-update-icon-cache', '-qtf',
                     os.path.join(datadir, 'icons', 'hicolor')])

    print('Updating desktop database...')
    subprocess.call(['update-desktop-database', '-q',
                     os.path.join(datadir, 'applications')])
