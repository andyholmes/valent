#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>


"""This module provides a test fixture for the ValentFdoNotifications plugin."""

# pylint: disable=import-error,invalid-name,missing-function-docstring

import fcntl
import os
import subprocess
import sys

import dbusmock # type: ignore
import dbus # type: ignore


# Add the shared directory to the search path
G_TEST_SRCDIR = os.environ.get('G_TEST_SRCDIR', '')
sys.path.append(os.path.join(G_TEST_SRCDIR, 'fixtures'))

# pylint: disable-next=wrong-import-position
import glibtest # type: ignore


class BluezTestFixture(glibtest.GLibTestCase, dbusmock.DBusTestCase):
    """A test fixture for the ValentBluezChannelService plugin."""

    @classmethod
    def setUpClass(cls) -> None:
        cls.start_system_bus()
        cls.dbus_con = cls.get_dbus(system_bus=True)

    def setUp(self) -> None:
        (self.p_mock, self.obj_bluez) = self.spawn_server_template(
            'bluez5', {}, stdout=subprocess.PIPE)

        self.dbusmock = dbus.Interface(self.obj_bluez, dbusmock.MOCK_IFACE)
        self.dbusmock_bluez = dbus.Interface(self.obj_bluez, 'org.bluez.Mock')

        # Add an adapter (i.e. the local device)
        self.dbusmock_bluez.AddAdapter('hci0', 'test-device')

        # Add a device (i.e. the remote device)
        self.dbusmock_bluez.AddDevice('hci0', 'AA:BB:CC:DD:EE:FF', 'MockDevice')

        # Set output to non-blocking
        flags = fcntl.fcntl(self.p_mock.stdout, fcntl.F_GETFL)
        fcntl.fcntl(self.p_mock.stdout, fcntl.F_SETFL, flags | os.O_NONBLOCK)

    def tearDown(self) -> None:
        self.p_mock.stdout.close()
        self.p_mock.terminate()
        self.p_mock.wait()


if __name__ == '__main__':
    glibtest.main()
