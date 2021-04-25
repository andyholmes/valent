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
BUILDDIR = os.environ.get('BUILDDIR')
TEST_SRCDIR = os.environ.get('G_TEST_SRCDIR')
TEST_PROGRAM = os.environ.get('TEST_PROGRAM')

sys.path.append(os.path.join(TEST_SRCDIR, 'fixtures'))

from gtest import GTest

class UPowerTestFixture(GTest, dbusmock.DBusTestCase):
    g_test_exe = os.path.join(BUILDDIR, TEST_PROGRAM)

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
        self.dbusmock.SetDeviceProperties(
            '/org/freedesktop/UPower/devices/DisplayDevice',
            {
                'Type': dbus.UInt32(2, variant_level=1)
            })

    def tearDown(self):
        self.p_mock.stdout.close()
        self.p_mock.terminate()
        self.p_mock.wait()


if __name__ == '__main__':
    # avoid writing to stderr
    unittest.main(testRunner=unittest.TextTestRunner(stream=sys.stdout, verbosity=2))

