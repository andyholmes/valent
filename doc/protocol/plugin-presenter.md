# Presenter Plugin

The **Presenter** plugin allows requesting a device to announce it's location,
usually by playing a sound like a traditional cordless phone.

## Sources

* [KDE Connect (Desktop)](https://invent.kde.org/network/kdeconnect-kde/tree/master/plugins/presenter)
* [KDE Connect (Android)](https://invent.kde.org/network/kdeconnect-android/tree/master/src/org/kde/kdeconnect/Plugins/PresenterPlugin)

## Packets

### `kdeconnect.presenter.request`

```js
// A key press
{
    "id": 0,
    "type": "kdeconnect.presenter.request",
    "body": {}
}

// A special key press (non-printable)
{
    "id": 0,
    "type": "kdeconnect.presenter.request",
    "body": {}
}

// A pointer delta
{
    "id": 0,
    "type": "kdeconnect.presenter.request",
    "body": {
        "dx": 0.1,
        "dy": 0.1
    }
}

// A scroll wheel delta
{
    "id": 0,
    "type": "kdeconnect.presenter.request",
    "body": {
        "dx": 0.1,
        "dy": 0.1,
        "scroll": true
    }
}
```

#### Fields

The `kdeconnect.presenter` packet has no fields. By convention,
sending a second request causes the device to stop announcing its location.

