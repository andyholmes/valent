# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [1.0.0.alpha.49] - 2025-11-17

### Added

- Support bluetooth connections
- Support notification IDs for XDG notifications
- Add Estonian (et) translation

### Changed

- Port from GdkPixbuf to Glycin
- Drop device GSettings
- Various protocol updates

### Fixed

- Fixes for VAPI and Vala plugins
- Protocol and security fixes

## [1.0.0.alpha.48] - 2025-09-14

### Added

- Support connecting to devices by IP address
- Support compose mode and grab release for input remote
- Support mounting specific SFTP volumes

### Changed

- Improve async operations for device connections, file transfers and mDNS
- Improve cross-platform support for launching URIs
- Various protocol updates
- Translation updates (Dutch @Vistaus)

### Fixed

- Fixes for certificates and devices IDs
- Fixes for modemmanager and signal strengths

## New Contributors

- @coreyoconnor made their first contribution in https://github.com/andyholmes/valent/pull/846

## [1.0.0.alpha.47] - 2025-03-04

### Changed

- Support v8 of the KDE Connect protocol
- The Contacts plugin has been rewritten with TinySPARQL
- Drop the Mutter input adapter in favour of the XDG portal
- Translation updates

## New Contributors

- @SpaciousCoder78 made their first contribution in https://github.com/andyholmes/valent/pull/733
- @rrrrrrmb made their first contribution in https://github.com/andyholmes/valent/pull/780
- @teohhanhui made their first contribution in https://github.com/andyholmes/valent/pull/783

## [1.0.0.alpha.46] - 2024-08-17

### Changed

- PipeWire support added
- The user interface has been absorbed the `gnome` plugin
- The SMS plugin has been rewritten with TinySPARQL
- The share dialog has been redesigned
- The media dialog has been refactored, with some UI improvements
- Port to libawaita-1.5
- Updates to protocol implementation; certificates, device metadata, security
- Translation updates

## New Contributors

- @glerroo made their first contribution in https://github.com/andyholmes/valent/pull/632
- @sungsphinx made their first contribution in https://github.com/andyholmes/valent/pull/628
- @vixalien made their first contribution in https://github.com/andyholmes/valent/pull/640

## [1.0.0.alpha.45] - 2024-03-10

### Changed

- Initial release.

[unreleased]: https://github.com/andyholmes/valent/compare/v1.0.0.alpha.48...HEAD
[1.0.0.alpha.47]: https://github.com/andyholmes/valent/compare/v1.0.0.alpha.47...v1.0.0.alpha.48
[1.0.0.alpha.47]: https://github.com/andyholmes/valent/compare/v1.0.0.alpha.46...v1.0.0.alpha.47
[1.0.0.alpha.46]: https://github.com/andyholmes/valent/compare/v1.0.0.alpha.45...v1.0.0.alpha.46
[1.0.0.alpha.45]: https://github.com/andyholmes/valent/releases/tag/v1.0.0.alpha.45

