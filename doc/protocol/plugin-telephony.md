# Telephony Plugin

The **Telephony** plugin allows receiving events such as incoming or missed
calls as well as sending and receive SMS messages.

## Sources

* [KDE Connect (Desktop)](https://invent.kde.org/network/kdeconnect-kde/tree/master/plugins/telephony)
* [KDE Connect (Android)](https://invent.kde.org/network/kdeconnect-android/tree/master/src/org/kde/kdeconnect/Plugins/TelephonyPlugin)

## Packets

### `kdeconnect.telephony`

```js
// A packet indicating an SMS message has been received
{
    "id": 0,
    type: 'kdeconnect.telephony',
    body: {
        "event": "sms"
        "contactName": "John Smith",
        "messageBody": "A text message",
        "phoneNumber": "555-555-5555",
        "phoneThumbnail": "<base64 encoded JPEG>"
    }
}

// A packet indicating the end of a phone call
{
    "id": 0,
    "type": "kdeconnect.telephony",
    "body": {
        "event": "talking",
        "contactName": "John Smith",
        "phoneNumber": "555-555-5555",
        "isCancel": true
    }
}
```

#### Fields
    
* `event` *String*
      
  The type of event, which is one of the following:
    * `missedCall` - The phone rang and was not answered
    * `ringing` - The phone is ringing
    * `sms` - An SMS message was received
    * `talking` - A call is in progress (incoming or outgoing)

* `isCancel` *Boolean*

  If the packet body contains this field the event indicated by the `event`
  field (either `ringing` or `talking`) has ended. Always `true` if present.

* `contactName` *String*

  The contact name associated with the event. May be absent if the call is from
  a contact not in the phone's address book.
  
* `messageBody` *String*

  If the `event` field is `sms` this will contain the body of the SMS message.

* `phoneNumber` *String*

  The phone number associated with the event. May be absent if the call is from
  an blocked or unknown number.

* `phoneThumbnail` *String*

    If the contact has a photo in the phone's address book, it may be sent as a
    base64 encoded string of bytes representing a JPEG image.

### `kdeconnect.telephony.message`

```js
{
    "id": 0,
    "type": "kdeconnect.telephony.message",
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

