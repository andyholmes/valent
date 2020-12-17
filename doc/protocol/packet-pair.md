# Pair Packet

...

### `kdeconnect.pair`

```js
{
    "id": 0,
    "type": "kdeconnect.pair",
    "body": {
        "pair": true
    }
}
```

#### Fields

* `pair` *Boolean*

  A boolean value, indicating whether a device is requesting pairing or
  unpairing. The receiving device responds with the same packet type indicating
  whether it is accepting or rejecting the request.

