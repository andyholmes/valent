#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>


"""This module provides a test environment for the battery plugin."""

# pylint: disable=import-error,invalid-name,missing-function-docstring

import fcntl
import os
import subprocess
import sys

import dbus # type: ignore
import dbusmock # type: ignore


# Add the shared directory to the search path
G_TEST_SRCDIR = os.environ.get('G_TEST_SRCDIR', '')
sys.path.append(os.path.join(G_TEST_SRCDIR, 'fixtures'))

# pylint: disable-next=wrong-import-position
import glibtest # type: ignore


class UPowerTestFixture(glibtest.GLibTestCase, dbusmock.DBusTestCase):
    """A test environment for the battery plugin."""

    @classmethod
    def setUpClass(cls) -> None:
        cls.start_system_bus()
        cls.dbus_con = cls.get_dbus(system_bus=True)

    def setUp(self) -> None:
        (self.p_mock, self.obj_upower) = self.spawn_server_template(
            'upower', {
                'OnBattery': True,
            },
            stdout=subprocess.PIPE)

        # Set output to non-blocking
        flags = fcntl.fcntl(self.p_mock.stdout, fcntl.F_GETFL)
        fcntl.fcntl(self.p_mock.stdout, fcntl.F_SETFL, flags | os.O_NONBLOCK)

        # Export a battery
        self.dbusmock = dbus.Interface(self.obj_upower, dbusmock.MOCK_IFACE)
        self.dbusmock.SetDeviceProperties(
            '/org/freedesktop/UPower/devices/DisplayDevice',
            {
                'IsPresent': dbus.Boolean(True, variant_level=1),
                'Type': dbus.UInt32(2, variant_level=1)
            })

    def tearDown(self) -> None:
        self.p_mock.stdout.close()
        self.p_mock.terminate()
        self.p_mock.wait()


if __name__ == '__main__':
    glibtest.main()
