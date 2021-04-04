# Mousepad Plugin

The **Mousepad** plugin allows requesting a device to announce it's location,
usually by playing a sound like a traditional cordless phone.

## Sources

* [KDE Connect (Desktop)](https://invent.kde.org/network/kdeconnect-kde/tree/master/plugins/mousepad)
* [KDE Connect (Android)](https://invent.kde.org/network/kdeconnect-android/tree/master/src/org/kde/kdeconnect/Plugins/MousePadPlugin)

## Packets

### `kdeconnect.mousepad.request`

```js
// A key press
{
    "id": 0,
    "type": "kdeconnect.mousepad.request",
    "body": {
        "key": "a"
    }
}

// A key press with modifiers
{
    "id": 0,
    "type": "kdeconnect.mousepad.request",
    "body": {
        "key": "a",
        "alt": false,
        "ctrl": true,
        "shift": false,
        "super": false
    }
}

// A special key press (non-printable)
{
    "id": 0,
    "type": "kdeconnect.mousepad.request",
    "body": {
        "specialKey": 12
    }
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

* `key` *String*

  A request to press-release a single readable character, which may be a
  unicode character and thus more than one byte.

* `specialKey` *Number*

  A request to press-release a single non-printable character, usually a control
  character or function key such as Backspace or F10.

* `sendAck` *Boolean*

  Indicates that the devices wants a `kdeconnect.mousepad.echo` packet as
  confirmation of a keyboard event. Always `true` if present.

* `alt` *Boolean*

  Indicates the ALT modifier should be applied to the associated `key` or
  `specialKey`. Always `true` if present.

* `ctrl` *Boolean*

  Indicates the CTRL modifier should be applied to the associated `key` or
  `specialKey`. Always `true` if present.

* `shift` *Boolean*

  Indicates the SHIFT modifier should be applied to the associated `key` or
  `specialKey`. Always `true` if present.

* `super` *Boolean*

  Indicates the SUPER modifier should be applied to the associated `key` or
  `specialKey`. Always `true` if present.

* `dx` *Number*

  A double precision integer indicating a relative position delta for the
  pointer on the X-axis.

* `dy` *Number*

  A double precision integer indicating a relative position delta for the
  pointer on the Y-axis.

* `scroll` *Boolean*

  Indicates that the pointer motion of the association `dx` or `dy` is for the
  mouse wheel. Always `true` if present.


### `kdeconnect.mousepad.echo`

```js
// A key press echo
{
    "id": 0,
    "type": "kdeconnect.mousepad.echo",
    "body": {
        "key": "a",
        "isAck": true
    }
}
```

#### Fields

The `kdeconnect.mousepad.echo` packet share the same keyboard fields as the
`kdeconnect.mousepad.request` packet and is used to send confirmation of a
successful event.

* `isAck` *Boolean*

  Indicates the packet is a confirmation of a request keyboard event. Always
  `true` and always present.


### `kdeconnect.mousepad.keyboardstate`

```js
// A keyboard status update
{
    "id": 0,
    "type": "kdeconnect.mousepad.keyboardstate",
    "body": {
        "state": true
    }
}
```

#### Fields

* `state` *Boolean*

  Indicates the keyboard is read to receive keypress events.

