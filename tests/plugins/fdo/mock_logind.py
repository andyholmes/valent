#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>


"""This module provides a test fixture for the ValentFdoSession plugin."""

# pylint: disable=import-error,invalid-name,missing-function-docstring

import fcntl
import os
import subprocess
import sys
import unittest

import dbusmock # type: ignore
import dbus # type: ignore


class SessionTestFixture(dbusmock.DBusTestCase):
    """A test fixture for the ValentFdoSession plugin."""

    @classmethod
    def setUpClass(cls) -> None:
        cls.start_system_bus()
        cls.dbus_con = cls.get_dbus(system_bus=True)

    def setUp(self) -> None:
        self.p_mock = self.spawn_server('org.freedesktop.login1',
                                        '/org/freedesktop/login1',
                                        'org.freedesktop.login1.Manager',
                                        system_bus=True,
                                        stdout=subprocess.PIPE)

        # Get a proxy for the logind object's Mock interface
        mock = dbus.Interface(self.dbus_con.get_object('org.freedesktop.login1',
                                                       '/org/freedesktop/login1'),
                              dbusmock.MOCK_IFACE)

        # We call this in the plugin, so we need to match that here
        uid = os.geteuid()
        user_path = dbus.ObjectPath(f'/org/freedesktop/login1/user/{uid}')

        session_id = '1'
        session_path = dbus.ObjectPath(f'/org/freedesktop/login1/session/{session_id}')

        mock.AddMethods('org.freedesktop.login1.Manager', [
            ('GetUser', 'u', 'o', 'ret = f"/org/freedesktop/login1/user/{args[0]}"'),
        ])

        mock.AddObject(user_path,
                       'org.freedesktop.login1.User',
                       {
                           'Sessions': dbus.Array([(session_id, session_path)],
                                                  signature='(so)'),
                           'Display': ('1', session_path),
                       },
                       [
                       ])

        mock.AddObject(session_path,
                       'org.freedesktop.login1.Session',
                       {
                           'Active': True,
                           'LockedHint': False,
                       },
                       [
                           ('Activate', '', '', ''),
                           ('Lock', '', '', 'self.EmitSignal("", "Lock", "", [])'),
                           ('Unlock', '', '', 'self.EmitSignal("", "Unlock", "", [])'),
                       ])

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
