#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>


"""This module provides a test fixture for the ValentFdoNotifications plugin."""

# pylint: disable=import-error,invalid-name,missing-function-docstring

import fcntl
import os
import subprocess
import sys

import dbusmock # type: ignore


# Add the shared directory to the search path
G_TEST_SRCDIR = os.environ.get('G_TEST_SRCDIR', '')
sys.path.append(os.path.join(G_TEST_SRCDIR, 'fixtures'))

# pylint: disable-next=wrong-import-position
import glibtest # type: ignore


class NotificationTestFixture(glibtest.GLibTestCase, dbusmock.DBusTestCase):
    """A test fixture for the ValentFdoNotifications plugin."""

    @classmethod
    def setUpClass(cls) -> None:
        cls.start_session_bus()
        cls.dbus_con = cls.get_dbus(system_bus=False)

    def setUp(self) -> None:
        (self.p_mock, self.obj_notifications) = self.spawn_server_template(
            'notification_daemon', {}, stdout=subprocess.PIPE)

        # Set output to non-blocking
        flags = fcntl.fcntl(self.p_mock.stdout, fcntl.F_GETFL)
        fcntl.fcntl(self.p_mock.stdout, fcntl.F_SETFL, flags | os.O_NONBLOCK)

    def tearDown(self) -> None:
        self.p_mock.stdout.close()
        self.p_mock.terminate()
        self.p_mock.wait()


if __name__ == '__main__':
    glibtest.main()
