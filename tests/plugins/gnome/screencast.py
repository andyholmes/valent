# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

'''ScreenCast mock template

This creates the expected methods and properties of Mutter's implementation of
the screencast D-Bus service.
'''

# pylint: disable=import-error,invalid-name,line-too-long,missing-function-docstring,protected-access

import dbus # type: ignore
import dbusmock # type: ignore
from dbusmock import MOCK_IFACE # type: ignore


BUS_NAME = 'org.gnome.Mutter.ScreenCast'
MAIN_IFACE = 'org.gnome.Mutter.ScreenCast'
MAIN_OBJ = '/org/gnome/Mutter/ScreenCast'
SESSION_IFACE = 'org.gnome.Mutter.ScreenCast.Session'
STREAM_IFACE = 'org.gnome.Mutter.ScreenCast.Stream'
IS_OBJECT_MANAGER = False
SYSTEM_BUS = False


def load(mock, _parameters):
    mock.AddMethods(MAIN_IFACE, [
        ('CreateSession', 'a{sv}', 'o', 'ret = self.AddSession(args[0])'),
    ])
    mock.AddProperties(MAIN_IFACE, {
        'Version': dbus.Int32(1),
    })


@dbus.service.method(MOCK_IFACE, in_signature='a{sv}', out_signature='o')
def AddSession(self, _options):
    '''Add a screencast session.

    You have to provide an index.

    Returns the new object path.
    '''

    index = 1 # be dynamic
    object_path = f'/org/gnome/Mutter/ScreenCast/Session/u{index}'
    self.AddObject(object_path,
                   SESSION_IFACE,
                   {
                       'SessionId': dbus.String(f'{index}'),
                       'CapsLockState': dbus.Boolean(False),
                       'NumLockState': dbus.Boolean(False),
                   },
                   [
                       ('RecordMonitor', 'sa{sv}',    'o', 'ret = self.AddMonitorStream(args[0], args[1])'),
                       ('RecordWindow',  'a{sv}',     'o', 'ret = self.AddWindowStream(args[0])'),
                       ('RecordArea',    'iiiia{sv}', 'o', 'ret = self.AddAreaStream(args[0], args[1], args[2], args[3], args[4])'),
                       ('RecordVirtual', 'a{sv}',     'o', 'ret = self.AddVirtualStream(args[0])'),

                       ('Start',                       '',       '', ''),
                       ('Stop',                        '',       '', ''),
                   ])

    return object_path


@dbus.service.method(MOCK_IFACE, in_signature='u', out_signature='s')
def RemoveSession(self, index):
    '''Remove a session, after emitting Closed.

    You have to provide an index.

    Returns the removed object path.
    '''

    object_path = f'/org/gnome/Mutter/ScreenCast/Session/u{index}'

    session = dbusmock.get_object(object_path)
    session.EmitSignal('', 'Closed', '', [])
    self.RemoveObject(object_path)

    return object_path

@dbus.service.method(MOCK_IFACE, in_signature='u', out_signature='o')
def AddStream(self, index):
    '''Add a screencast stream.

    You have to provide an index.

    Returns the new object path.
    '''

    object_path = f'/org/gnome/Mutter/ScreenCast/Stream/u{index}'
    self.AddObject(object_path,
                   STREAM_IFACE,
                   {
                       'Parameters': dbus.Dictionary({
                            'position':    (dbus.Int32(0), dbus.Int32(0)),
                            'size':        (dbus.Int32(0), dbus.Int32(0)),
                            'output-name': dbus.String(f'TEST-{index}'),
                        }, signature='sv'),
                   },
                   [
                       ('Start', '', '', ''),
                   ])

    return object_path


@dbus.service.method(MOCK_IFACE, in_signature='sa{sv}', out_signature='o')
def AddMonitorStream(self, _connector, _properties):
    '''Add a modem, in an offline state.

    You have to provide an index.

    Returns the new object path.
    '''

    return self.AddStream(0)


@dbus.service.method(MOCK_IFACE, in_signature='a{sv}', out_signature='o')
def AddWindowStream(self, _properties):
    '''Add a modem, in an offline state.

    You have to provide an index.

    Returns the new object path.
    '''

    return self.AddStream(0)


@dbus.service.method(MOCK_IFACE, in_signature='iiiia{sv}', out_signature='o')
def AddAreaStream(self, _x, _y, _width, _height, _properties): # pylint: disable=too-many-arguments
    '''Add a modem, in an offline state.

    You have to provide an index.

    Returns the new object path.
    '''

    return self.AddStream(0)


@dbus.service.method(MOCK_IFACE, in_signature='a{sv}', out_signature='o')
def AddVirtualStream(self, _properties):
    '''Add a modem, in an offline state.

    You have to provide an index.

    Returns the new object path.
    '''

    return self.AddStream(0)


@dbus.service.method(MOCK_IFACE, in_signature='u', out_signature='s')
def RemoveStream(self, index):
    '''Remove a stream, after emitting Closed.

    You have to provide an index.

    Returns the removed object path.
    '''

    object_path = f'/org/gnome/Mutter/ScreenCast/Stream/u{index}'

    stream = dbusmock.get_object(object_path)
    stream.EmitSignal('', 'Closed', '', [])
    self.RemoveObject(object_path)

    return object_path
