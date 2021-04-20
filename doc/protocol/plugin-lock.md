# Lock Plugin

The **Lock** plugin allows requesting a device lock or unlock.

## Sources

* [KDE Connect (Desktop)](https://invent.kde.org/network/kdeconnect-kde/tree/master/plugins/lockdevice)

## Packets

### `kdeconnect.lock`

```js
// The current locked state
{
    "id": 0,
    "type": "kdeconnect.lock",
    "body": {
        "isLocked": true
    }
}
```

#### Fields

* `isLocked` *Boolean*

  Indicates the current locked status of the device. If `true` the device is
  locked and `false` if unlocked.

### `kdeconnect.lock.request`

```js
// A request for the current locked state
{
    "id": 0,
    "type": "kdeconnect.lock.request",
    "body": {
        "requestLocked": true
    }
}

// A request to change the locked state
{
    "id": 0,
    "type": "kdeconnect.lock.request",
    "body": {
        "setLocked": true
    }
}
```

#### Fields

* `requestLocked` *Boolean*

  Indicates this is a request for the current locked status. Always true, if
  present.

* `setLocked` *Boolean*

  A request to change the locked status. If `true` the device will be locked
  or if `false` unlocked.

