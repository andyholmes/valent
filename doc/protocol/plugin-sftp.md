# SFTP Plugin

The **SFTP** plugin allows requesting a device start an Sftp server and
exchanging information necessary to connect to it. Generally, this is
implemented using `sshfs`.

Although this could be implemented by IO libraries like Gio or KIO, it's usually
not possible since the Android app requires use of DSA (ssh-dss) keys which have
been deprecated in OpenSSH since openssh-7.0p1.

See also:

  * [Bug #351725](https://bugs.kde.org/show_bug.cgi?id=351725)
  * [openssh-7.0p1 release announcement](http://lists.mindrot.org/pipermail/openssh-unix-announce/2015-August/000122.html)

## Sources

* [KDE Connect (Desktop)](https://invent.kde.org/network/kdeconnect-kde/tree/master/plugins/sftp)
* [KDE Connect (Android)](https://invent.kde.org/network/kdeconnect-android/tree/master/src/org/kde/kdeconnect/Plugins/SftpPlugin)

## Packets

### `kdeconnect.sftp`

```js
// A response to a `startBrowsing` request
{
    "id": 0,
    "type": "kdeconnect.sftp",
    "body": {
        "ip": "192.168.1.71",
        "port": 1743,
        "user": "kdeconnect",
        "password": "UzcNCrI7T668JyxUFjOxQncBPNcO",
        "path": "/storage/emulated/0",
        "multiPaths": [
            "/storage/0000-0000",
            "/storage/0000-0000/DCIM/Camera",
            "/storage/emulated/0",
            "/storage/emulated/0/DCIM/Camera"
        ],
        "pathNames": [
            "SD Card",
            "Camera Pictures (SD Card)",
            "All files",
            "Camera pictures"
        ]
    }
}
```

#### Fields
    
* `ip` *String*
      
  The remote host address. Since there are situations when the returned address
  may be for the wrong interface (eg. cell radio), it's often more reliable to
  take the address from the channel sending this packet.
  
* `port` *Number*

  The remote host port, between 1739-1764.
  
* `user` *String*

  The user name to pass to SSH. Currently always "kdeconnect".
  
* `password` *String*

  A password to pass to SSH. The password is randomly generated each time the
  remote server is started, usually per connection session.
  
* `multiPaths` *Array* of *String*

  An ordered list of paths for the remote server. Usually contains at least a
  "root" directory and a path the the camera folder, but may contain additional
  paths to external storage devices.
  
* `pathNames` *Array* of *String*

  An ordered list of names associated with the paths in `multiPaths`, in the
  same order.
  
* `path` *String*

  The base path of the remote server. This should generally only be used as a
  fallback if the `multiPaths` field is missing.

### `kdeconnect.sftp.request`

```js
// A request to start the mounting process
{
    "id": 0,
    "type": "kdeconnect.sftp.request",
    "body": {
        "startBrowsing": true
    }
}
```

#### Fields
    
* `startBrowsing` *Boolean*
      
  A request for the remote device to start the Sftp server and respond with a
  `kdeconnect.sftp.request` packet with the necessary connection information.
  Always true, if present.
