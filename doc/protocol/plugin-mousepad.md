# Mousepad Plugin

The **Mousepad** plugin allows requesting a device to announce it's location,
usually by playing a sound like a traditional cordless phone.

## Sources

* [KDE Connect (Desktop)](https://invent.kde.org/network/kdeconnect-kde/tree/master/plugins/mousepad)
* [KDE Connect (Android)](https://invent.kde.org/network/kdeconnect-android/tree/master/src/org/kde/kdeconnect/Plugins/MousepadPlugin)

## Packets

### `kdeconnect.mousepad.request`

```js
// A key press
{
    "id": 0,
    "type": "kdeconnect.mousepad.request",
    "body": {}
}

// A special key press (non-printable)
{
    "id": 0,
    "type": "kdeconnect.mousepad.request",
    "body": {}
}

// A pointer delta
{
    "id": 0,
    "type": "kdeconnect.mousepad.request",
    "body": {
        "dx": 0.1,
        "dy": 0.1
    }
}

// A scroll wheel delta
{
    "id": 0,
    "type": "kdeconnect.mousepad.request",
    "body": {
        "dx": 0.1,
        "dy": 0.1,
        "scroll": true
    }
}
```

#### Fields

The `kdeconnect.mousepad` packet has no fields. By convention,
sending a second request causes the device to stop announcing its location.

