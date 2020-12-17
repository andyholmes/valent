# RunCommand Plugin

The **RunCommand** plugin allows exposing and executing commands on devices.

## Sources

* [KDE Connect (Desktop)](https://invent.kde.org/network/kdeconnect-kde/tree/master/plugins/remotecommands)
* [KDE Connect (Android)](https://invent.kde.org/network/kdeconnect-android/tree/master/src/org/kde/kdeconnect/Plugins/RunCommandPlugin)

## Packets

### `kdeconnect.runcommand`

```js
// A list of commands
{
    "id": 0,
    "type": "kdeconnect.runcommand",
    "body": {
        "commandList": {
            "1e55114f-590c-487b-8af1-7ec6f194d671": {
                "name": "Kodi",
                "command": "/home/andrew/scripts/kodi"
            },
            "bea9fb3e-0c80-4d05-afdc-6a8f4156bc03": {
                "name": "Transmission",
                "command": "transmission-gtk -m"
            }
        }
    }
}
```

#### Fields
    
* `commandList` *Object* of *Object*
      
  A dictionary of commands that the device offers. By convention each entry's
  key is a UUIDv4, but can be any unique string. The key can be sent in a
  `kdeconnect.runcommand.request` packet to execute its corresponding command.
  Each command object has the following fields:
  
  * `name` *String*
    
     The display name of the command.
    
  * `command` *String*
  
     The command line arguments, usually passed to `sh -c`.

### `kdeconnect.runcommand.request`

```js
// A request to execute a command
{
    "id": 0,
    "type": "kdeconnect.runcommand.request",
    "body": {
        "key": "bea9fb3e-0c80-4d05-afdc-6a8f4156bc03"
    }
}

// A request for a device's command list
{
    "id": 0,
    "type": "kdeconnect.runcommand.request",
    "body": {
        "requestCommandList": true
    }
}
```

#### Fields
    
* `key` *String*
      
  If the packet body contains this field it is a request to execute the
  command with the id `key`.
    
* `requestCommandList` *Boolean*
      
  If the packet body contains this field it is a request for a list of commands
  the device offers. Always `true` if present.
