# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

valent_c_args = ['-DVALENT_PLUGINSDIR="@0@"'.format(pkgpluginsdir)]
valent_link_args = []


# The base API is defined by libvalent and functionality is delegated to plugins
subdir('libvalent')
subdir('plugins')


# Application
valent = executable('valent', 'main.c',
                 c_args: valent_c_args + release_args,
              link_args: valent_link_args,
             link_whole: plugins_static,
  gnu_symbol_visibility: 'hidden',
           dependencies: libvalent_dep,
                install: true,
                    pie: true,
)

