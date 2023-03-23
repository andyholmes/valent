#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>


"""This module provides a test fixture for the ValentGtkModemManager plugin."""

# pylint: disable=import-error,invalid-name,missing-function-docstring

import fcntl
import os
import subprocess
import sys
import unittest

import dbusmock # type: ignore


TEMPLATE_DIR = os.path.dirname(os.path.realpath(__file__))


class ModemManagerTestFixture(dbusmock.DBusTestCase):
    """A test fixture for the connectivity report plugin."""

    @classmethod
    def setUpClass(cls) -> None:
        cls.start_system_bus()
        cls.dbus_con = cls.get_dbus(system_bus=True)

    def setUp(self) -> None:
        (self.p_mock, self.obj_modemmanager) = self.spawn_server_template(
            f'{TEMPLATE_DIR}/modemmanager.py', {
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
        try:
            test = subprocess.run([os.environ.get('G_TEST_EXE', ''), '--tap'],
                                  capture_output=True,
                                  check=True,
                                  encoding='utf-8')

            sys.stdout.write(test.stdout)
            sys.stderr.write(test.stderr)
        except subprocess.SubprocessError as error:
            # pylint: disable-next=no-member
            self.fail(error.stdout) # type: ignore


if __name__ == '__main__':
    # Output to stderr; we're forwarding TAP output of the real program
    runner = unittest.TextTestRunner(stream=sys.stderr, verbosity=2)
    unittest.main(testRunner=runner)
