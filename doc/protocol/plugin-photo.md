# Photo Plugin

The **Photo** plugin allows requesting a device to open the camera and/or
take a photo, which is then transferred to the requesting device.

## Sources

* [KDE Connect (Desktop)](https://invent.kde.org/network/kdeconnect-kde/tree/master/plugins/photo)
* [KDE Connect (Android)](https://invent.kde.org/network/kdeconnect-android/tree/master/src/org/kde/kdeconnect/Plugins/PhotoPlugin)

## Packets

### `kdeconnect.photo`

```js
// A photo transfer packet
{
    "id": 0,
    "type": "kdeconnect.photo",
    "body": {
        "filename": "photo.jpg"
    },
    "payloadSize": 4096
}
```

#### Fields

* `filename` *String*

  The packet must contain this field and will be accompanied by payload transfer
  information for the file transfer.


### `kdeconnect.photo.request`

```js
// A photo request packet
{
    "id": 0,
    "type": "kdeconnect.photo.request",
    "body": {}
}
```

#### Fields

The `kdeconnect.photo` packet has no fields. Sending this packet requests the
remote device to take and/or transfer a photo.

