# Notification Plugin

The **Notification** plugin sends and receives notifications between devices. The
fields generally correspond to libnotify notifications, but could represent
other notification systems such as GNotification.

## Sources

* [KDE Connect (Desktop)](https://invent.kde.org/network/kdeconnect-kde/tree/master/plugins/notification)
* [KDE Connect (Android)](https://invent.kde.org/network/kdeconnect-android/tree/master/src/org/kde/kdeconnect/Plugins/NotificationPlugin)

## Packets

### `kdeconnect.notification`

```js
// A packet indicating a new notification has been shown
{
    "id": 0,
    type: 'kdeconnect.notification',
    body: {
        "requestReplyId": "17499937-334b-4704-9c2c-24a0bcd4155a",
        "id": "gsconnect-notification",
        "appName": "GSConnect",
        "isClearable": true,
        "onlyOnce": false,
        "ticker": "GSConnect: Service Message",
        "title": "GSConnect",
        "text": "Service Message",
        "time": "<unix-timestamp>",
        "actions": [
            "Button 1",
            "Button 2",
            "Button 3"
        ],
        "payloadHash": "d97f60d052bf11d1e88821e04fff0007"
    }
}

// A packet indicating a notification has been withdrawn
{
    "id": 0,
    "type": "kdeconnect.notification",
    "body": {
        "id": "<notification-id>",
        "isCancel": true
    }
}
```

#### Fields

* `actions` *Array* of *String*

* `appName` *String*

  The name of the notifying application.

* `id` *String*

  The notification id, if `isClearable` is true it can be used to close the
  notification with a notification.request packet.
  
* `isCancel` *Boolean*

  If true, the notification indicated by `id` has been withdrawn by the device.
  
* `isClearable` *Boolean*

  If true, the notification can be closed by responding with a
   the `id` and `cancel` set to true.

* `onlyOnce` *Boolean*

  ...

* `requestReplyId` *String*

  A UUID for repliable notifications, used in `kdeconnect.notification.reply`
  packets.

* `text` *String*

  The body of the notification.

* `ticker` *String*

  A deprecated field combining the title and text of the notification in the
  form "`title`: `text`".

* `title` *String*

  The title of the notification.

* `time` *String*

  The time in milliseconds since the epoch (relative the device's local time)
  when the notification was posted, but as a *String*. For Android devices,
  this will reset if the device is reset.

### `kdeconnect.notification.reply`

**TODO**

### `kdeconnect.notification.request`

```js
// A request to close a notification by id.
{
    "id": 0,
    "type": "kdeconnect.notification.request",
    "body": {
        "cancel": "<notification-id>"
    }
}

// A request for the device's notifications.
{
    "id": 0,
    "type": "kdeconnect.notification.request",
    "body": {
        "request": true
    }
}
```

#### Fields

* `cancel` *String*

  The id of a notification being requested to be withdrawn.

* `request` *Boolean*

  Indicates this is a request for the device's notifications. Always true, if
  present.


