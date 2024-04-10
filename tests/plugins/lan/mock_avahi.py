#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>


"""This module provides a test fixture for Avahi."""


import fcntl
import os
import subprocess
import sys
import unittest

import dbusmock


TEMPLATE_DIR = os.path.dirname(os.path.realpath(__file__))


class AvahiTestFixture(dbusmock.DBusTestCase):
    """A DNS-SD test fixture for the LAN plugin."""

    @classmethod
    def setUpClass(cls) -> None:
        cls.start_system_bus()
        cls.dbus_con = cls.get_dbus(system_bus=True)

    def setUp(self) -> None:
        (self.p_mock, self.obj_avahi) = self.spawn_server_template(
            f'{TEMPLATE_DIR}/avahi.py', {
            },
            stdout=subprocess.PIPE)

        # Set output to non-blocking
        flags = fcntl.fcntl(self.p_mock.stdout, fcntl.F_GETFL)
        fcntl.fcntl(self.p_mock.stdout, fcntl.F_SETFL, flags | os.O_NONBLOCK)

    def tearDown(self) -> None:
        self.p_mock.stdout.close()
        self.p_mock.terminate()
        self.p_mock.wait()

    def test_run(self) -> None:
        subprocess.run([os.environ.get('G_TEST_EXE', ''), '--tap'],
                       check=True,
                       encoding='utf-8',
                       stderr=sys.stderr,
                       stdout=sys.stdout)


if __name__ == '__main__':
    # Output to stderr; we're forwarding TAP output of the real program
    runner = unittest.TextTestRunner(stream=sys.stderr, verbosity=2)
    unittest.main(testRunner=runner)
