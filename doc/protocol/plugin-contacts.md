# Contacts Plugin

The **Contacts** plugin allows requesting a device to announce it's location,
usually by playing a sound like a traditional cordless phone.

## Sources

* [KDE Connect (Desktop)](https://invent.kde.org/network/kdeconnect-kde/tree/master/plugins/contacts)
* [KDE Connect (Android)](https://invent.kde.org/network/kdeconnect-android/tree/master/src/org/kde/kdeconnect/Plugins/ContactsPlugin)

## Packets

### `kdeconnect.contacts.request_all_uids_timstamps`

```js
// A request for all contact UIDs with timestamps
{
    "id": 0,
    "type": "kdeconnect.contacts.request_all_uids_timstamps",
    "body": {}
}
```

#### Fields

The `kdeconnect.contacts.request_all_uids_timestamps` packet has no fields.

### `kdeconnect.contacts.request_vcards_by_uid`

```js
// A request for contact vCards by UID
{
    "id": 0,
    "type": "kdeconnect.contacts",
    "body": {
        "uids": [
          "test-contact1",
          "test-contact2"
        ]
    }
}
```

#### Fields

* `uids` *Array* of *String*

The `kdeconnect.contacts` packet has no fields. By convention,
sending a second request causes the device to stop announcing its location.

### `kdeconnect.contacts.response_uids_timestamps`

```js
// A response to a request for all contact UIDs with timestamps
{
    "id": 0,
    "type": "kdeconnect.contacts.response_uids_timestamps",
    "body": {
        "test-contact1": 1608700784336,
        "test-contact2": 1608700782848,
        "uids": [
          "test-contact1",
          "test-contact2"
        ]
    }
}
```

#### Fields

The `kdeconnect.contacts` packet has no fields. By convention,
sending a second request causes the device to stop announcing its location.

### `kdeconnect.contacts.response_vcards`

```js
// A bi-directional request packet
{
    "id": 0,
    "type": "kdeconnect.contacts.response_vcards",
    "body": {
        "test-contact1": "BEGIN:VCARD\nVERSION:2.1\nFN:Contact One\nTEL;CELL:123-456-7890\nX-KDECONNECT-ID-DEV-test-device:test-contact1\nX-KDECONNECT-TIMESTAMP:1608700784336\nEND:VCARD",
        "test-contact2": "BEGIN:VCARD\nVERSION:2.1\nFN:Contact Two\nTEL;CELL:123-456-7890\nX-KDECONNECT-ID-DEV-test-device:test-contact2\nX-KDECONNECT-TIMESTAMP:1608700782848\nEND:VCARD",
        "uids": [
            "test-contact1",
            "test-contact2"
        ]
    }
}
```

#### Fields

The `kdeconnect.contacts` packet has no fields. By convention,
sending a second request causes the device to stop announcing its location.

