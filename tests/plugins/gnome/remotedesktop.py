# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

'''RemoteDesktop mock template

This creates the expected methods and properties of Mutter's implementation of
the remote desktop D-Bus service.

The service itself is exported on org.gnome.Shell, because that's the well-known
name Valent's plugins watch.
'''

# pylint: disable=import-error,invalid-name,line-too-long,missing-function-docstring,protected-access

import dbus # type: ignore
import dbusmock # type: ignore
from dbusmock import MOCK_IFACE # type: ignore


BUS_NAME = 'org.gnome.Shell'
MAIN_IFACE = 'org.gnome.Mutter.RemoteDesktop'
MAIN_OBJ = '/org/gnome/Mutter/RemoteDesktop'
SESSION_IFACE = 'org.gnome.Mutter.RemoteDesktop.Session'
SESSION_BASE_PATH = '/org/gnome/Mutter/RemoteDesktop/Session'
IS_OBJECT_MANAGER = False
SYSTEM_BUS = False


def load(mock, _parameters):
    mock.AddMethods(MAIN_IFACE, [
        ('CreateSession', '', 'o', 'ret = self.AddSession(1)'),
    ])
    mock.AddProperties(MAIN_IFACE, {
        'Version': dbus.Int32(1),
        'SupportedDeviceTypes': dbus.UInt32(7),
    })


@dbus.service.method(MOCK_IFACE, in_signature='u', out_signature='o')
def AddSession(self, index):
    '''Add a modem, in an offline state.

    You have to provide an index.

    Returns the new object path.
    '''

    object_path = f'/org/gnome/Mutter/RemoteDesktop/Session/u{index}'
    self.AddObject(object_path,
                   SESSION_IFACE,
                   {
                       'CapsLockState': dbus.Boolean(False),
                       'NumLockState': dbus.Boolean(False),
                       'SessionId': dbus.String(f'{index}'),
                   },
                   [
                       ('NotifyKeyboardKeycode',       '(ub)',   '', ''),
                       ('NotifyKeyboardKeysym' ,       '(ub)',   '', ''),
                       ('NotifyPointerButton',         '(ib)',   '', ''),
                       ('NotifyPointerAxis',           '(ddu)',  '', ''),
                       ('NotifyPointerAxisDiscrete',   '(ui)',   '', ''),
                       ('NotifyPointerMotionAbsolute', '(ddu)',  '', ''),
                       ('NotifyPointerMotionRelative', '(dd)',   '', ''),
                       ('NotifyPointerTouchDown',      '(sudd)', '', ''),
                       ('NotifyPointerTouchMotion',    '(sudd)', '', ''),
                       ('NotifyPointerTouchUp',        '(u)',    '', ''),

                       # Clipboard

                       ('Start',                       '',       '', ''),
                       ('Stop',                        '',       '', 'self.EmitSignal("", "Closed", "", [])'),
                   ])

    return object_path


@dbus.service.method(MOCK_IFACE, in_signature='u', out_signature='s')
def RemoveSession(self, index):
    '''Remove a session, after emitting Closed.

    You have to provide an index.

    Returns the removed object path.
    '''

    object_path = f'/org/gnome/Mutter/RemoteDesktop/Session/u{index}'

    session = dbusmock.get_object(object_path)
    session.EmitSignal('', 'Closed', '', [])
    self.RemoveObject(object_path)

    return object_path
