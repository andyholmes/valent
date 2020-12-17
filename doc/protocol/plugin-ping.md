# Ping Plugin

The **Ping** plugin allows sending and receiving simple "pings", with an
optional message. Usually these are displayed as notifications.

## Sources

* [KDE Connect (Desktop)](https://invent.kde.org/network/kdeconnect-kde/tree/master/plugins/ping)
* [KDE Connect (Android)](https://invent.kde.org/network/kdeconnect-android/tree/master/src/org/kde/kdeconnect/Plugins/PingPlugin)

## Packets

### `kdeconnect.ping`

```js
// A bi-directional ping packet
{
    "id": 0,
    "type": "kdeconnect.ping",
    "body": {
        "message": "A ping with a message"
    }
}
```

#### Fields
    
* `message` *String*
      
  An optional message to send with the ping.
