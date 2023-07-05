# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>


'''RemoteDesktop mock template

This creates the expected methods and properties of Mutter's implementation of
the remote desktop D-Bus service.

The service itself is exported on org.gnome.Shell, because that's the well-known
name Valent's plugins watch.
'''

# pylint: disable=line-too-long,protected-access


import io
import os

import dbus # type: ignore
import dbusmock # type: ignore
from dbusmock import MOCK_IFACE # type: ignore
from gi.repository import GLib # type: ignore


BUS_NAME = 'org.gnome.Mutter.RemoteDesktop'
MAIN_IFACE = 'org.gnome.Mutter.RemoteDesktop'
MAIN_OBJ = '/org/gnome/Mutter/RemoteDesktop'
SESSION_IFACE = 'org.gnome.Mutter.RemoteDesktop.Session'
SESSION_BASE_PATH = '/org/gnome/Mutter/RemoteDesktop/Session'
IS_OBJECT_MANAGER = False
SYSTEM_BUS = False


def load(mock, _parameters):
    mock.n_sessions = 0
    mock.AddProperties(MAIN_IFACE, {
        'Version': dbus.Int32(1),
        'SupportedDeviceTypes': dbus.UInt32(7),
    })


@dbus.service.method(MAIN_IFACE, in_signature='', out_signature='o')
def CreateSession(self):
    '''Add a session, in a quiescent state.

    Returns the new object path.
    '''

    self.n_sessions += 1
    index = self.n_sessions

    object_path = f'/org/gnome/Mutter/RemoteDesktop/Session/u{index}'
    self.AddObject(object_path,
                   SESSION_IFACE,
                   {
                       'CapsLockState': dbus.Boolean(False),
                       'NumLockState': dbus.Boolean(False),
                       'SessionId': dbus.String(f'{index}'),
                   },
                   [
                       ('NotifyKeyboardKeycode',       '(ub)',   '',  ''),
                       ('NotifyKeyboardKeysym' ,       '(ub)',   '',  ''),
                       ('NotifyPointerButton',         '(ib)',   '',  ''),
                       ('NotifyPointerAxis',           '(ddu)',  '',  ''),
                       ('NotifyPointerAxisDiscrete',   '(ui)',   '',  ''),
                       ('NotifyPointerMotionAbsolute', '(ddu)',  '',  ''),
                       ('NotifyPointerMotionRelative', '(dd)',   '',  ''),
                       ('NotifyPointerTouchDown',      '(sudd)', '',  ''),
                       ('NotifyPointerTouchMotion',    '(sudd)', '',  ''),
                       ('NotifyPointerTouchUp',        '(u)',    '',  ''),

                       # ('DisableClipboard',            '',       '',  'self.enabled = True'),
                       # ('EnableClipboard',             'a{sv}',  '',  'self.enabled = False'),
                       # ('SelectionRead',               's',      'h', 'ret = 0'),
                       # ('SelectionWrite',              'u',      'h', 'ret = 0'),
                       # ('SelectionWriteDone',          'ub',     '',  ''),
                       # ('SetSelection',                'a{sv}',  '',  'ret = dbus.Dictionary({}, signature="sv")'),

                       ('Start',                       '',       '',  ''),
                       ('Stop',                        '',       '',  f'self.RemoveSession({index})'),
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

    if session is not None:
        session.EmitSignal(SESSION_IFACE, 'Closed', '', [])
        self.RemoveObject(object_path)

    return object_path


@dbus.service.method(SESSION_IFACE, in_signature='', out_signature='')
def DisableClipboard(self):
    '''Disable the clipboard for a session.
    '''

    if getattr(self, 'enabled', False):
        raise dbus.exceptions.DBusException('Clipboard not enabled',
                                            name=SESSION_IFACE)

    self.enabled = False


@dbus.service.method(SESSION_IFACE, in_signature='a{sv}', out_signature='')
def EnableClipboard(self, options):
    '''Enable the clipboard for a session.
    '''

    if getattr(self, 'enabled', False):
        raise dbus.exceptions.DBusException('Clipboard already enabled',
                                            name=SESSION_IFACE)

    self.enabled = True
    self.serial = 0

    if 'mime-types' in options:
        self.SetSelection(options)


@dbus.service.method(SESSION_IFACE, in_signature='s', out_signature='h')
def SelectionRead(self, _mime_type):
    '''Opens a file descriptor for the client to read from.

    Returns a file descriptor.
    '''

    fd_read, fd_write = os.pipe2(os.O_NONBLOCK | os.O_CLOEXEC)

    def write_content(fd, _cond):
        target = io.FileIO(fd, 'wb', closefd=True)
        target.write(self.content.read())
        return GLib.SOURCE_REMOVE

    GLib.unix_fd_add_full(GLib.PRIORITY_HIGH,
                          fd_write,
                          GLib.IO_OUT,
                          write_content)
    GLib.idle_add(os.close, fd_read)

    # Emit SelectionTransfer to simulate a content request
    if getattr(self, 'is_owner', True):
        self.serial += 1
        self.EmitSignal(SESSION_IFACE, 'SelectionTransfer', '', [
            dbus.String(self.mimetypes[0]),
            dbus.UInt32(self.serial),
        ])

    return fd_read

@dbus.service.method(SESSION_IFACE, in_signature='u', out_signature='h')
def SelectionWrite(self, serial):
    '''Opens a file descriptor for the client to write to.

    Returns a file descriptor.
    '''

    if getattr(self, 'serial', 0) < serial:
        raise dbus.exceptions.DBusException(f'Invalid serial number ({serial})',
                                            name='org.freedesktop.DBus.Error.InvalidArgs')

    self.content = io.BytesIO()

    fd_read, fd_write = os.pipe2(os.O_NONBLOCK | os.O_CLOEXEC)

    def read_content(fd, _cond):
        source = io.FileIO(fd, 'rb', closefd=True)
        self.content.write(source.read())
        return GLib.SOURCE_REMOVE

    GLib.unix_fd_add_full(GLib.PRIORITY_HIGH,
                          fd_read,
                          GLib.IO_IN,
                          read_content)
    GLib.idle_add(os.close, fd_write)

    return fd_write


@dbus.service.method(SESSION_IFACE, in_signature='ub', out_signature='')
def SelectionWriteDone(self, serial, success):
    '''Mark a write operation as complete.'''

    if getattr(self, 'serial', 0) == serial:
        self.is_owner = success


@dbus.service.method(SESSION_IFACE, in_signature='a{sv}', out_signature='')
def SetSelection(self, options):
    '''Remove a session, after emitting Closed.'''

    self.content = io.BytesIO()
    self.mimetypes = options.mimetypes
    self.is_owner = True

    self.EmitSignal('', 'SelectionOwnerChanged', 'a{sv}', [{
        'mime-types': (dbus.Array(self.mimetypes, signature='s'),),
        'session-is-owner': dbus.Boolean(self.is_owner),
    }])
