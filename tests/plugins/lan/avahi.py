# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

'''Avahi mock template

This creates the expected methods and properties of Avahi's EntryGroup and
ServiceBrowser interfaces, and the main interface methods they require.
'''

# pylint: disable=line-too-long,protected-access

import dbus # type: ignore
import dbusmock # type: ignore
from dbusmock import MOCK_IFACE


BUS_NAME = 'org.freedesktop.Avahi'
MAIN_IFACE = 'org.freedesktop.Avahi.Server2'
MAIN_OBJ = '/'
SERVICE_BROWSER_IFACE = 'org.freedesktop.Avahi.ServiceBrowser'
ENTRY_GROUP_IFACE = 'org.freedesktop.Avahi.EntryGroup'
IS_OBJECT_MANAGER = False
SYSTEM_BUS = True


def load(mock, _parameters):
    mock.AddMethods(MAIN_IFACE, [
        ('GetAPIVersion',                  '',        'u',             'ret = 516'),
        ('GetDomainName',                  '',        's',             'ret = self.domain'),
        ('GetHostName',                    '',        's',             'ret = self.hostname'),
        ('SetHostName',                    's',       '',              'self.hostname = args[0]'),
        ('GetHostNameFqdn',                '',        's',             'ret = f"{self.hostname}.{self.domain}"'),
        ('GetLocalServiceCookie',          '',        'i',             'ret = 0'),
        ('GetState',                       '',        'i',             'ret = self.avahi_state'),
        ('GetVersionString',               '',        's',             'ret = "avahi 0.8"'),
        ('IsNSSSupportAvailable',          '',        'b',             'ret = True'),

        ('GetAlternativeHostName',         's',       's',             'ret = f"{args[0]}-alternate"'),
        ('GetAlternativeServiceName',      's',       's',             'ret = f"{args[0]}-alternate"'),
        ('GetNetworkInterfaceIndexByName', 's',       'i',             'ret = -1'), # AVAHI_IF_UNSPEC
        ('GetNetworkInterfaceNameByIndex', 'i',       's',             'ret = "wlp1s0"'),
        ('ResolveAddress',                 'iisiu',   'iiissu',        'ret = [-1, -1, -1, "", "", 0]'),
        ('ResolveHostName',                'iisu',    'iisisu',        'ret = [-1, -1, "", -1 "", 0]'),
        # ('ResolveService',                 'iisssiu', 'iissssisqaayu', 'ret = [-1, -1, "", "", "", "", -1, "", 0, [], 0]'),

        # ('EntryGroupNew',                  '',        'o',             'ret = "/Client1/EntryGroup1"'),

        ('DomainBrowserPrepare',           'iisiu',   'o',             'ret = "/"'),
        ('RecordBrowserPrepare',           'iisqqu',  'o',             'ret = "/"'),
        # ('ServiceBrowserPrepare',          'iissu',   'o',             'ret = "/"'),
        ('ServiceTypeBrowserPrepare',      'iisu',    'o',             'ret = "/"'),

        ('AddressResolverPrepare',         'iisu',    'o',             'ret = "/"'),
        ('HostNameResolverPrepare',        'iisiu',   'o',             'ret = "/"'),
        ('ServiceResolverPrepare',         'iisssiu', 'o',             'ret = "/"'),
    ])
    mock.avahi_state = 2 # AVAHI_SERVER_RUNNING
    mock.avahi_services = {}
    mock.hostname = 'localhost'
    mock.domain = 'local'
    mock.n_sb = 0
    mock.n_eg = 0

@dbus.service.method(MAIN_IFACE, in_signature='iisssiu', out_signature='iissssisqaayu')
def ResolveService(self, _interface, _protocol, name, _type, _domain, aprotocol, flags):
    '''Resolve a service.

    Returns the resolved service.
    '''

    if name not in self.avahi_services:
        self.avahi_state = 4 # SERVER_FAILURE
        self.EmitSignal(ENTRY_GROUP_IFACE, 'StateChanged', 'is', [
            dbus.Int32(self.avahi_state),
            dbus.String('org.freedesktop.Avahi.InvalidServiceNameError'),
        ])
        raise dbus.exceptions.DBusException(f'DNS-SD service name "{name}" unknown',
                                            name='org.freedesktop.Avahi.InvalidServiceNameError')

    service = self.avahi_services.get(name)

    return (
        dbus.Int32(service['interface']),
        dbus.Int32(service['protocol']),
        dbus.String(service['name']),
        dbus.String(service['type']),
        dbus.String(service['domain']),
        dbus.String(service['host']),
        dbus.Int32(aprotocol),
        dbus.String('127.0.0.1'),                   # address
        dbus.UInt16(service['port']),               # port
        dbus.Array(service['txt'], signature='ay'), # txt
        dbus.UInt32(flags),                         # result flags
    )

@dbus.service.method(MAIN_IFACE, in_signature='', out_signature='o')
def EntryGroupNew(self):
    '''Add an entry group

    Returns the new object path.
    '''

    self.n_eg += 1
    object_path = f'/org/freedesktop/Avahi/Client1/EntryGroup{self.n_eg}'

    self.AddObject(object_path,
                   ENTRY_GROUP_IFACE,
                   {
                   },
                   [
                       ('GetState',          '',            'i', 'ret = self.avahi_state'),
                       ('IsEmpty',           '',            'b', 'ret = True'),

                       # ('Commit',            '',            '',  ''),
                       ('Free',              '',            '',  'self.EntryGroupFree()'),
                       # ('Reset',             '',            '',  ''),

                       # ('AddService',        'iiussssqaay', '',  ''),
                       ('AddServiceSubtype', 'iiussss',     '',  ''),
                       # ('UpdateServiceTxt',  'iiusssaay',   '',  ''),
                       ('AddAddress',        'iiuss',       '',  ''),
                       ('AddRecord',         'iiusqquay',   '',  ''),
                   ])

    entry_group = dbusmock.get_object(object_path)
    entry_group.avahi_state = 0 # AVAHI_ENTRY_GROUP_UNCOMMITTED
    entry_group.avahi_services = {}
    entry_group.avahi_uncommitted = []

    return object_path

@dbus.service.method(ENTRY_GROUP_IFACE, in_signature='iiussssqaay', out_signature='')
def AddService(self, interface, protocol, flags, name, type_, domain, host, port, txt):
    '''Add a new service.
    '''

    # pylint: disable=too-many-arguments

    self.avahi_uncommitted.append({
        'interface': interface,
        'protocol': protocol,
        'flags': flags,
        'name': name,
        'type': type_,
        'domain': domain or 'local',
        'host': host or 'localhost',
        'port': port or 1717,
        'txt': txt,
    })

    self.avahi_state = 0 # AVAHI_ENTRY_GROUP_UNCOMMITTED
    self.EmitSignal(ENTRY_GROUP_IFACE, 'StateChanged', 'is', [
        dbus.Int32(self.avahi_state),
        dbus.String(''),
    ])

@dbus.service.method(ENTRY_GROUP_IFACE, in_signature='iiusssaay', out_signature='')
def UpdateServiceTxt(self, interface, protocol, flags, name, type_, domain, txt):
    '''Update a service TXT record.
    '''

    # pylint: disable=too-many-arguments

    if name not in self.avahi_services:
        self.avahi_state = 4 # AVAHI_ENTRY_GROUP_FAILURE
        self.EmitSignal(ENTRY_GROUP_IFACE, 'StateChanged', 'is', [
            dbus.Int32(self.avahi_state),
            dbus.String('org.freedesktop.Avahi.InvalidServiceNameError'),
        ])
        raise dbus.exceptions.DBusException(f'DNS-SD service name "{name}" unknown',
                                            name='org.freedesktop.Avahi.InvalidServiceNameError')

    service = self.avahi_services.get(name)
    service.update({
        'interface': interface,
        'protocol': protocol,
        'flags': flags,
        'name': name,
        'type': type_,
        'domain': domain or 'local',
        'txt': txt,
    })

@dbus.service.method(ENTRY_GROUP_IFACE, in_signature='', out_signature='')
def Commit(self):
    '''Commit new services.
    '''

    avahi_server2 = dbusmock.get_object('/')

    object_path = f'/org/freedesktop/Avahi/Client{1}/ServiceBrowser{1}'
    service_browser = dbusmock.get_object(object_path)

    self.avahi_state = 1 # STATE_REGISTERING
    self.EmitSignal(ENTRY_GROUP_IFACE, 'StateChanged', 'is', [
        dbus.Int32(self.avahi_state),
        dbus.String(''),
    ])

    for service in self.avahi_uncommitted:
        name = service['name']

        # Services are not being unregistered between tests
        if name in avahi_server2.avahi_services:
            self.avahi_state = 3 # STATE_COLLISION
            self.EmitSignal(ENTRY_GROUP_IFACE, 'StateChanged', 'is', [
                dbus.Int32(self.avahi_state),
                dbus.String('org.freedesktop.Avahi.CollisionError'),
            ])
            raise dbus.exceptions.DBusException(f'DNS-SD service name "{name}" already registered',
                                                name='org.freedesktop.Avahi.CollisionError')

        self.avahi_services[name] = service
        avahi_server2.avahi_services[name] = service

        service_browser.EmitSignal(SERVICE_BROWSER_IFACE, 'ItemNew', 'iisssu', [
            dbus.Int32(service['interface']),
            dbus.Int32(service['protocol']),
            dbus.String(service['name']),
            dbus.String(service['type']),
            dbus.String(service['domain']),
            dbus.UInt32(service['flags']),
        ])

    self.avahi_uncommitted.clear()
    self.avahi_state = 2 # AVAHI_ENTRY_GROUP_ESTABLISHED
    self.EmitSignal(ENTRY_GROUP_IFACE, 'StateChanged', 'is', [
        dbus.Int32(self.avahi_state),
        dbus.String('org.freedesktop.Avahi.Success'),
    ])

@dbus.service.method(MOCK_IFACE, in_signature='', out_signature='')
def EntryGroupFree(self):
    '''Free the entry group.
    '''

    registered_services = dbusmock.get_object('/').avahi_services

    for name in self.avahi_services:
        registered_services.pop(name)

    avahi_server2 = dbusmock.get_object('/')
    avahi_server2.RemoveObject(self.path)

@dbus.service.method(ENTRY_GROUP_IFACE, in_signature='', out_signature='')
def Reset(self):
    '''Reset the entry group.
    '''

    registered_services = dbusmock.get_object('/').avahi_services

    object_path = f'/org/freedesktop/Avahi/Client{1}/ServiceBrowser{1}'
    service_browser = dbusmock.get_object(object_path)

    for name, service in self.avahi_services.items():
        registered_services.pop(name)
        service_browser.EmitSignal(SERVICE_BROWSER_IFACE, 'ItemRemove', 'iisssu', [
            dbus.Int32(service['interface']),
            dbus.Int32(service['protocol']),
            dbus.String(service['name']),
            dbus.String(service['type']),
            dbus.String(service['domain']),
            dbus.UInt32(service['flags']),
        ])

    self.avahi_services.clear()

@dbus.service.method(MAIN_IFACE, in_signature='iissu', out_signature='o')
def ServiceBrowserPrepare(self, _interface, _protocol, _domain, _type, _flags):
    '''Create a new service browser.

    Returns the new object path.
    '''

    self.n_sb += 1
    object_path = f'/org/freedesktop/Avahi/Client1/ServiceBrowser{self.n_sb}'

    self.AddObject(object_path,
                   SERVICE_BROWSER_IFACE,
                   {
                   },
                   [
                       ('Free', '', '', 'avahi_server2 = dbusmock.get_object("/");'
                                        'avahi_server2.RemoveObject(self.path);   '),
                       # ('Start', '', '', ''),
                   ])

    return object_path

@dbus.service.method(SERVICE_BROWSER_IFACE, in_signature='', out_signature='')
def Start(self):
    '''Start the service browser.
    '''

    registered_services = dbusmock.get_object('/').avahi_services.values()

    for service in registered_services:
        self.EmitSignal(SERVICE_BROWSER_IFACE, 'ItemNew', 'iisssu', [
            dbus.Int32(service['interface']),
            dbus.Int32(service['protocol']),
            dbus.String(service['name']),
            dbus.String(service['type']),
            dbus.String(service['domain']),
            dbus.UInt32(service['flags']),
        ])

    self.EmitSignal(SERVICE_BROWSER_IFACE, 'CacheExhausted', '', [])
    self.EmitSignal(SERVICE_BROWSER_IFACE, 'AllForNow', '', [])
