# Battery Plugin

The **Battery** plugin allows a device to expose it's battery status.

## Sources

* [KDE Connect (Desktop)](https://invent.kde.org/network/kdeconnect-kde/tree/master/plugins/battery)
* [KDE Connect (Android)](https://invent.kde.org/network/kdeconnect-android/tree/master/src/org/kde/kdeconnect/Plugins/BatteryPlugin)

## Packets

### `kdeconnect.battery`

```js
// A battery status update
{
    "id": 0,
    "type": "kdeconnect.battery",
    "body": {
        "currentCharge": 100,
        "isCharging": true,
        "thresholdEvent": 0
    }
}
```

#### Fields

* `currentCharge` *Number*

  The current battery level, typically between `0` and `100`. If the value is
  `-1`, the battery status may be unknown or the battery missing.

* `isCharging` *Boolean*

  A boolean value indicating whether the battery is currently charging.

* `thresholdEvent` *Number*

  Either `0` or `1` if the battery is below the threshold level; the level the
  when the device considers the battery low.

### `kdeconnect.battery.request`

```js
// A request for the battery status
{
    "id": 0,
    "type": "kdeconnect.battery.request",
    "body": {}
}
```

#### Fields

The `kdeconnect.battery.request` packet has no fields. This packet is sent to
request the remote device's battery status.

