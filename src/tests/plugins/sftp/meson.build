# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

# Dependencies
plugin_sftp_test_deps = [
  libvalent_test_dep,
  plugin_sftp_deps,
]

plugin_sftp_tests = [
  'test-sftp-plugin',
  'test-sftp-preferences',
]

foreach test : plugin_sftp_tests
  source = ['@0@.c'.format(test)]

  test_program = executable(test, source,
          c_args: test_c_args,
    dependencies: plugin_sftp_test_deps,
       link_args: test_link_args,
      link_whole: [libvalent_test, plugin_sftp],
  )

  test(test, test_program,
            env: tests_env,
    is_parallel: false,
          suite: ['plugins', 'sftp'],
  )
endforeach

