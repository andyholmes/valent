# Clipboard Plugin

The **Clipboard** plugin allows syncing clipboard text content between devices.

## Sources

* [KDE Connect (Desktop)](https://invent.kde.org/network/kdeconnect-kde/tree/master/plugins/clipboard)
* [KDE Connect (Android)](https://invent.kde.org/network/kdeconnect-android/tree/master/src/org/kde/kdeconnect/Plugins/ClipboardPlugin)

## Packets

### `kdeconnect.clipboard`

```js
// A clipboard update
{
    "id": 0,
    "type": "kdeconnect.clipboard",
    "body": {
        "content": "lorem ipsum"
    }
}
```

#### Fields

* `content` *String*

  Text content of the remote clipboard.

### `kdeconnect.clipboard.connect`

```js
// A connect-time clipboard update
{
    "id": 0,
    "type": "kdeconnect.clipboard.connect",
    "body": {
        "content": "lorem ipsum",
        "timestamp": 0
    }
}
```

#### Fields

The `kdeconnect.clipboard.connect` packet is only sent at connection.

* `content` *String*

  Text content of the remote clipboard.

* `timestamp` *Number*

  UNIX epoch timestamp (ms) for the clipboard content.

