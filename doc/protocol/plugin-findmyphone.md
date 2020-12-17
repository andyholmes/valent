# FindMyPhone Plugin

The **FindMyPhone** plugin allows requesting a device to announce it's location,
usually by playing a sound like a traditional cordless phone.

## Sources

* [KDE Connect (Desktop)](https://invent.kde.org/network/kdeconnect-kde/tree/master/plugins/findmyphone)
* [KDE Connect (Android)](https://invent.kde.org/network/kdeconnect-android/tree/master/src/org/kde/kdeconnect/Plugins/FindMyPhonePlugin)

## Packets

### `kdeconnect.findmyphone.request`

```js
// A bi-directional request packet
{
    "id": 0,
    "type": "kdeconnect.findmyphone.request",
    "body": {}
}
```

#### Fields
    
The `kdeconnect.findmyphone.request` packet has no fields. By convention,
sending a second request causes the device to stop announcing its location.

