# SMS Plugin

The **SMS** plugin allows receiving events such as incoming or missed
calls as well as sending and receive SMS messages.

## Sources

* [KDE Connect (Desktop)](https://invent.kde.org/network/kdeconnect-kde/tree/master/plugins/sms)
* [KDE Connect (Android)](https://invent.kde.org/network/kdeconnect-android/tree/master/src/org/kde/kdeconnect/Plugins/SMSPlugin)

## Packets

### `kdeconnect.sms.message`

```js
{
    "id": 0,
    "type": "kdeconnect.sms.message",
    "body": {
        {Array} A list of messages
        "messages": [
            {
                "address": "5556667777",
                "body": "an sms message",
                "date": 1526284870369,
                "type": 2,
                "read": 1,
                "thread_id": 5,
                "_id": 7,
                "event": "sms"
            },
            ...
        ]
    }
}
```

#### Fields

* TODO

### `kdeconnect.sms.request`

```js
{
    "id": 0,
    "type": "kdeconnect.sms.request",
    "body": {
        "sendSms": true,
        "phoneNumber": "555-555-5555",
        "messageBody": "An outgoing message"
    }
}
```

#### Fields

* `phoneNumber` *String*

  The phone number to send the SMS message to.

* `messageBody` *String*

  The SMS message body.

* `sendSms` *Boolean*

  If the packet body contains this field it indicates that this is and outgoing
  SMS message. Always true, if present.

