#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>


"""This module provides a test fixture for the ValentGtkNotifications plugin."""

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


class NotificationsTestFixture(glibtest.GLibTestCase, dbusmock.DBusTestCase):
    """A test fixture for the ValentGtkNotifications plugin."""

    @classmethod
    def setUpClass(cls) -> None:
        cls.start_session_bus()
        cls.dbus_con = cls.get_dbus(system_bus=False)

    def setUp(self) -> None:
        self.p_mock = self.spawn_server('org.gtk.Notifications',
                                        '/org/gtk/Notifications',
                                        'org.gtk.Notifications',
                                        system_bus=False,
                                        stdout=subprocess.PIPE)

        # Get a proxy for the notification object's Mock interface
        mock = dbus.Interface(self.dbus_con.get_object('org.gtk.Notifications',
                                                       '/org/gtk/Notifications'),
                              dbusmock.MOCK_IFACE)

        mock.AddMethods('org.gtk.Notifications', [
            ('AddNotification', 'ssa{sv}', '', ''),
            ('RemoveNotification', 'ss', '', ''),
        ])

        # Set output to non-blocking
        flags = fcntl.fcntl(self.p_mock.stdout, fcntl.F_GETFL)
        fcntl.fcntl(self.p_mock.stdout, fcntl.F_SETFL, flags | os.O_NONBLOCK)

    def tearDown(self) -> None:
        self.p_mock.stdout.close()
        self.p_mock.terminate()
        self.p_mock.wait()


if __name__ == '__main__':
    glibtest.main()
