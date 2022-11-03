# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

# Dependencies
plugin_sms_test_deps = [
  libvalent_test_dep,
  plugin_sms_deps,
]

plugin_sms_tests = [
  'test-contact-row',
  'test-date-label',
  'test-message',
  'test-message-row',
  'test-message-thread',
  'test-sms-conversation',
  'test-sms-conversation-row',
  'test-sms-plugin',
  'test-sms-store',
  'test-sms-utils',
  'test-sms-window',
]

foreach test : plugin_sms_tests
  test_program = executable(test, '@0@.c'.format(test),
                 c_args: test_c_args,
           dependencies: plugin_sms_test_deps,
    include_directories: plugin_sms_include_directories,
              link_args: test_link_args,
             link_whole: [libvalent_test, plugin_sms],
  )

  test(test, test_program,
            env: tests_env,
    is_parallel: false,
          suite: ['plugins', 'sms'],
        timeout: 240,
  )
endforeach

