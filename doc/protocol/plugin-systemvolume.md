# SystemVolume Plugin

The `systemvolume` plugin allows controlling volume controls between devices.
The fields generally correspond to PulseAudio output sink properties, but could
represent other volume systems.

## Sources

* [KDE Connect (Desktop)](https://invent.kde.org/network/kdeconnect-kde/tree/master/plugins/systemvolume)
* [KDE Connect (Android)](https://invent.kde.org/network/kdeconnect-android/tree/master/src/org/kde/kdeconnect/Plugins/SystemvolumePlugin)

## Packets

### `kdeconnect.systemvolume`

```js
// A packet listing the device's audio sinks (outputs)
{
    "id": 0,
    type: 'kdeconnect.systemvolume',
    body: {
        "sinkList": [
            {
                "name": "alsa_output.pci-0000_00_1b.0.analog-stereo",
                "description": "Built-in Audio Analog Stereo",
                "muted": false,
                "volume": 32768,
                "maxVolume": 65536
            }
        ]
    }
}

// A packet updating the status of an audio sink
{
    "id": 0,
    "type": "kdeconnect.systemvolume",
    "body": {
        "name": "Speakers",
        "volume": 49,
        "muted": false
    }
}
```

#### Fields
    
* `name` *String*
      
  The name of the sink, used as an id and not displayed in the interface. If the
  packet body contains this field it is an update for a single sink.

* `muted` *Boolean*

  Whether the stream is muted.

* `volume` *Number*

  The volume level of the sink. Usually a 16-bit integer (0-65536), but only
  required to be relative to `maxVolume`.

* `sinkList` *Array* of *Object*

    If the packet body contains this field it is a list of audio sinks, each an
    object with the following fields:
    
    * `name` *String*
      
      The name of the sink, used as an id and not displayed in the interface.
      
    * `description` *String*
      
      The description of the sink, displayed in the interface.
      
    * `maxVolume` *Number*
      
      The maximum volume level of the sink. Usually a 16-bit integer (0-65536),
      but only required to be relative to `volume`.
      
    * `muted` *Boolean*
    
      Whether the sink is muted.
      
    * `volume` *Number*
    
      The volume level of the sink. Usually a 16-bit integer (0-65536), but only
      required to be relative to `maxVolume`.

### `kdeconnect.systemvolume.request`

```js
// A request for the list of pulseaudio sinks (outputs)
{
    "id": 0,
    "type": "kdeconnect.systemvolume.request",
    "body": {
        "requestSinks": true
    }
}

// A request to adjust an audio sink (output)
{
    "id": 0,
    "type": "kdeconnect.systemvolume.request",
    "body": {
        "name": "Speakers",
        "volume": 32768,
        "muted": false
    }
}
```

#### Fields

* `muted` *Boolean*

  Whether the sink should be muted.

* `name` *String*

  The name of the sink to adjust.

* `requestSinks` *Boolean*

  If the packet body contains this field it is a request for a list of audio
  sinks. Always `true` if present.

* `volume` *Number*

  The level the volume of the sink should be set to. Usually a 16-bit integer
  (0-65536), but only required to be relative to `maxVolume`.
