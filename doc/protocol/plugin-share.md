# Share Plugin

The **Share** plugin allows sharing files, text content and URLs.

## Sources

* [KDE Connect (Desktop)](https://invent.kde.org/network/kdeconnect-kde/tree/master/plugins/share)
* [KDE Connect (Android)](https://invent.kde.org/network/kdeconnect-android/tree/master/src/org/kde/kdeconnect/Plugins/SharePlugin)

## Packets

### `kdeconnect.share.request`

```js
// A file transfer (TCP)
{
    "id": 0,
    "type": "kdeconnect.share.request",
    "body": {
        "filename": "song.mp3"
    },
    "payloadSize": 1024000,
    "payloadTransferInfo": {
        "port": 1739
    }
}

// Shared text content
{
    "id": 0,
    "type": "kdeconnect.share.request",
    "body": {
        "text": "lorem ipsum"
    }
}

// Shared URL
{
    "id": 0,
    "type": "kdeconnect.share.request",
    "body": {
        "url": "https://www.google.com"
    }
}
```

#### Fields

* `filename` *String*

  If the packet contains this field, it will be accompanied by payload transfer
  information for a file transfer.

* `text` *String*

  If the packet contains this field, it will contain text content. The receiving
  device determines how to present it, such as a dialog or writing it to a
  temporary file and opening it in the default text editor.

* `url` *String*

  If the packet contains this field, it will contain a URL. The receiving device
  will typically open the URL with the default handler for the URI scheme.

