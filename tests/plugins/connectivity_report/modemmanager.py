# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2022 Andy Holmes <andrew.g.r.holmes@gmail.com>

'''ModeManager mock template

This creates the expected methods and properties of the main
org.freedesktop.ModemManager1 object, but nothing else. The only property
available to set in 'parameters' is 'Version'.
'''

# pylint: disable=import-error,invalid-name,missing-function-docstring,protected-access

# This program is free software; you can redistribute it and/or modify it under
# the terms of the GNU Lesser General Public License as published by the Free
# Software Foundation; either version 3 of the License, or (at your option) any
# later version.  See http://www.gnu.org/copyleft/lgpl.html for the full text
# of the license.

__author__ = 'Andy Holmes'
__copyright__ = '(c) 2022 Andy Holmes'

import dbus # type: ignore
import dbusmock # type: ignore
from dbusmock import MOCK_IFACE # type: ignore


BUS_NAME = 'org.freedesktop.ModemManager1'
MAIN_OBJ = '/org/freedesktop/ModemManager1'
# MAIN_IFACE = 'org.freedesktop.ModemManager1'
MANAGER_OBJ = '/org/freedesktop/ModemManager1'
MANAGER_IFACE = 'org.freedesktop.ModemManager1'
MODEM_IFACE = 'org.freedesktop.ModemManager1.Modem'
IS_OBJECT_MANAGER = True
SYSTEM_BUS = True


def load(mock, parameters):
    mock.AddMethods(MANAGER_IFACE,
                    [
                       ('ScanDevices', '', '', ''),
                       ('SetLogging', 's', '', ''),
                       ('ReportKernelEvent', 'a{sv}', '', ''),
                       ('InhibitDevice', 'sb', '', ''),
                   ])
    mock.AddProperties(MANAGER_IFACE,
                       {
                           'Version': parameters.get('Version', '1.18.8'),
                       })


@dbus.service.method(MOCK_IFACE, in_signature='u', out_signature='s')
def AddModem(self, index):
    '''Add a modem, in an offline state.

    You have to provide an index.

    Returns the new object path.
    '''

    object_path = f'/org/freedesktop/ModemManager1/Modem/{index}'
    self.AddObject(object_path,
                   MODEM_IFACE,
                   {
                       'Sim': dbus.ObjectPath('/'),
                       'SimSlots': dbus.Array([], signature='o'),
                       'PrimarySimSlot': dbus.UInt32(0),
                       'Bearers': dbus.Array([], signature='o'),
                       'SupportedCapabilities': dbus.Array([12], signature='u'),
                       'CurrentCapabilities': dbus.UInt32(12),
                       'MaxBearers': dbus.UInt32(1),
                       'MaxActiveBearers': dbus.UInt32(1),
                       'MaxActiveMultiplexedBearers': dbus.UInt32(254),
                       'Manufacturer': 'QUALCOMM INCORPORATED',
                       'Model': 'QUECTEL Mobile Broadband Module',
                       'Revision': 'EG25GGBR07A08M2G',
                       'CarrierConfiguration': 'Telus-Commercial_VoLTE',
                       'CarrierConfigurationRevision': '05800C43',
                       'HardwareRevision': '10000',
                       'DeviceIdentifier': '9583afc26b8e419967f79ff009872f02640071d2',
                       'Device': '/sys/devices/platform/soc/1c1b000.usb/usb2/2-1',
                       'Drivers': dbus.Array(['qmi_wwan', 'option'], signature='s'),
                       'Plugin': 'quectel',
                       'PrimaryPort': 'cdc-wdm0',
                       'Ports': dbus.Array([
                           ('cdc-wdm0', dbus.UInt32(6)),
                           ('ttyUSB0', dbus.UInt32(4)),
                           ('ttyUSB1', dbus.UInt32(5)),
                           ('ttyUSB2', dbus.UInt32(3)),
                           ('ttyUSB3', dbus.UInt32(3)),
                           ('wwan0', dbus.UInt32(2)),
                       ], signature='(su)'),
                       'EquipmentIdentifier': '867698044837725',
                       'UnlockRequired': dbus.UInt32(0),
                       'UnlockRetries': dbus.Dictionary({}, signature='uu'),
                       'State': dbus.Int32(-1),
                       'StateFailedReason': dbus.UInt32(2),
                       'AccessTechnologies': dbus.UInt32(0),
                       'SignalQuality': (dbus.UInt32(0), True),
                       'OwnNumbers': dbus.Array([], signature='s'),
                       'PowerState': dbus.UInt32(3),
                       'SupportedModes': dbus.Array([
                           (2, 0),
                           (4, 0),
                           (8, 0),
                           (6, 4),
                           (6, 2),
                           (10, 8),
                           (10, 2),
                           (14, 8),
                           (14, 4),
                           (14, 2),
                       ], signature='(uu)'),
                       'CurrentModes': (dbus.UInt32(4294967295), dbus.UInt32(0)),
                       'SupportedBands': dbus.Array([
                           1, 2, 3, 4, 5, 6, 7, 8, 9,
                           10, 12,
                           31, 32, 33, 34, 35, 37, 38,
                           42, 43, 48, 49,
                           50, 55, 56, 58,
                           68, 69,
                           70, 71,
                           219,
                        ], signature='u'),
                       'CurrentBands': dbus.Array([0], signature='u'),
                       'SupportedIpFamilies': dbus.UInt32(7),
                   }, [])

    self.object_manager_emit_added(object_path)

    return object_path


@dbus.service.method(MOCK_IFACE, in_signature='u', out_signature='s')
def RemoveModem(self, index):
    '''Remove a modem, regardless of its state.

    You have to provide an index.

    Returns the removed object path.
    '''

    object_path = f'/org/freedesktop/ModemManager1/Modem/{index}'

    self.object_manager_emit_removed(object_path)
    self.RemoveObject(object_path)

    return object_path


@dbus.service.method(MOCK_IFACE, in_signature='u', out_signature='')
def SetModemOnline(_self, index):
    '''Convenience method to change a modem's state from offline to online.
    index: the modem to update
    Calling this method will trigger the modem's 'PropertiesChanged' signal.
    '''

    modem = dbusmock.get_object(f'/org/freedesktop/ModemManager1/Modem/{index}')
    properties = {
        'Sim': dbus.ObjectPath(f'/org/freedesktop/ModemManager1/SIM/{index}'),
        'UnlockRequired': dbus.UInt32(3),
        'UnlockRetries': dbus.Dictionary({
            2: 3,
            4: 10,
            3: 3,
            5:10,
        }, signature='uu'),
        'State': dbus.Int32(8),
        'StateFailedReason': dbus.UInt32(0),
        'AccessTechnologies': dbus.UInt32(16384),
        'SignalQuality': (dbus.UInt32(62), False),
        'OwnNumbers': dbus.Array(['555-555-5555'], signature='s'),
        'CurrentModes': (dbus.UInt32(14), dbus.UInt32(8)),
        'CurrentBands': dbus.Array([
            1, 2, 3, 4, 5, 6, 7, 8, 9,
            10, 12,
            31, 32, 33, 34, 35, 37, 38,
            42, 43, 48, 49,
            50, 55, 56, 58,
            68, 69,
            70, 71,
            219,
         ], signature='u'),
    }

    for name, value in properties.items():
        modem._set_property(MODEM_IFACE, name, value)

    modem.EmitSignal(dbus.PROPERTIES_IFACE, 'PropertiesChanged', 'sa{sv}as', [
        MODEM_IFACE, properties, []])


@dbus.service.method(MOCK_IFACE, in_signature='u', out_signature='')
def SetModemOffline(_self, index):
    '''Convenience method to change a modem's state from online to offline.
    index: the modem to update
    Calling this methof will trigger the modem's PropertiesChanged signal.
    '''

    modem = dbusmock.get_object(f'/org/freedesktop/ModemManager1/Modem/{index}')
    properties = {
        'Sim': dbus.ObjectPath('/'),
        'UnlockRequired': dbus.UInt32(0),
        'UnlockRetries': dbus.Dictionary({}, signature='uu'),
        'State': dbus.Int32(-1),
        'StateFailedReason': dbus.UInt32(2),
        'AccessTechnologies': dbus.UInt32(0),
        'SignalQuality': (dbus.UInt32(0), True),
        'OwnNumbers': dbus.Array([], signature='s'),
        'CurrentModes': (dbus.UInt32(4294967295), dbus.UInt32(0)),
        'CurrentBands': dbus.Array([0], signature='u'),
    }

    for name, value in properties.items():
        modem._set_property(MODEM_IFACE, name, value)

    modem.EmitSignal(dbus.PROPERTIES_IFACE, 'PropertiesChanged', 'sa{sv}as', [
        MODEM_IFACE, properties, []])
