#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

import dbus
import fcntl
import os
import subprocess
import sys
import unittest

try:
    import dbusmock
except ImportError:
    sys.stderr.write('You need python-dbusmock (http://pypi.python.org/pypi/python-dbusmock) for this test suite.\n')
    sys.exit(1)

# Add the shared directory to the search path
SOURCEDIR = os.environ.get('SOURCEDIR')
BUILDDIR = os.environ.get('BUILDDIR')

sys.path.append(os.path.join(os.path.dirname(__file__), '..', '..', 'fixtures'))

from gtest import GTest

class UPowerTestSuite(GTest, dbusmock.DBusTestCase):
    g_test_exe = os.path.join(BUILDDIR, 'test-upower')

    @classmethod
    def setUpClass(cls):
        cls.start_system_bus()
        cls.dbus_con = cls.get_dbus(True)

    def setUp(self):
        (self.p_mock, self.obj_upower) = self.spawn_server_template(
            'upower', {
                'OnBattery': True,
            },
            stdout=subprocess.PIPE)

        # set log to nonblocking
        flags = fcntl.fcntl(self.p_mock.stdout, fcntl.F_GETFL)
        fcntl.fcntl(self.p_mock.stdout, fcntl.F_SETFL, flags | os.O_NONBLOCK)
        self.dbusmock = dbus.Interface(self.obj_upower, dbusmock.MOCK_IFACE)

    def tearDown(self):
        self.p_mock.stdout.close()
        self.p_mock.terminate()
        self.p_mock.wait()


if __name__ == '__main__':
    # avoid writing to stderr
    unittest.main(testRunner=unittest.TextTestRunner(stream=sys.stdout, verbosity=2))
    
