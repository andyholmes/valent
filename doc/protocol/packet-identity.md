# Identity Packet

### `kdeconnect.identity`

```js
{
  "id": 1602828714372,
  "type": "kdeconnect.identity",
  "body": {
    "deviceId": "ee5f31fbfcc1efbe",
    "deviceName": "Asus Nexus 7",
    "protocolVersion": 7,
    "deviceType": "tablet",
    "incomingCapabilities": [
      "kdeconnect.battery.request",
      "kdeconnect.clipboard",
      "kdeconnect.clipboard.connect",
      "kdeconnect.contacts.request_all_uids_timestamps",
      "kdeconnect.contacts.request_vcards_by_uid",
      "kdeconnect.findmyphone.request",
      "kdeconnect.mousepad.keyboardstate",
      "kdeconnect.mousepad.request",
      "kdeconnect.mpris",
      "kdeconnect.mpris.request",
      "kdeconnect.notification",
      "kdeconnect.notification.action"
      "kdeconnect.notification.reply",
      "kdeconnect.notification.request",
      "kdeconnect.photo.request",
      "kdeconnect.ping",
      "kdeconnect.runcommand",
      "kdeconnect.sftp.request",
      "kdeconnect.share.request",
      "kdeconnect.share.request.update",
      "kdeconnect.sms.request",
      "kdeconnect.sms.request_conversation",
      "kdeconnect.sms.request_conversations",
      "kdeconnect.systemvolume",
      "kdeconnect.telephony.request",
      "kdeconnect.telephony.request_mute",
    ],
    "outgoingCapabilities": [
      "kdeconnect.battery",
      "kdeconnect.clipboard",
      "kdeconnect.clipboard.connect",
      "kdeconnect.contacts.response_uids_timestamps",
      "kdeconnect.contacts.response_vcards",
      "kdeconnect.findmyphone.request"
      "kdeconnect.mousepad.echo",
      "kdeconnect.mousepad.keyboardstate",
      "kdeconnect.mousepad.request",
      "kdeconnect.mpris",
      "kdeconnect.mpris.request",
      "kdeconnect.notification",
      "kdeconnect.notification.request",
      "kdeconnect.photo",
      "kdeconnect.ping",
      "kdeconnect.presenter",
      "kdeconnect.runcommand.request",
      "kdeconnect.sftp",
      "kdeconnect.share.request",
      "kdeconnect.sms.messages",
      "kdeconnect.systemvolume.request",
      "kdeconnect.telephony",
    ],
    "tcpPort": 1716
  }
}
```

#### Fields

* `deviceId` *String*

  A unique ID for the device, typically a hostname or UUID.

* `deviceName` *String*

  A human-readable label for the device.

* `deviceType` *String*

  A case-sensitive device type string. Known values include

* `protocolVersion` *Number*

  Currently always `7`.

