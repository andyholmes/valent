# Contributing

Thanks for thinking about contributing to Valent!

Valent is an implementation of the KDE Connect protocol, built on GNOME platform
libraries. It is written entirely in C, with support for plugins written in
C/C++, Python3 or Vala.

The project primarily targets Linux systems running GNOME Shell or Phosh, but
will accept contributions for other UNIX-based systems like BSD and other
GTK-based environments like elementaryOS or XFCE.


## Reporting an Issue

This project is currently in alpha. It generally works as intended, but is still
missing some features and may contain bugs. Before reporting an issue, please
verify it has not been fixed in the `main` branch.

* If you confirm a problem exists, open a [new issue]
* To request a feature, start a [new discussion]
* To report a security vulnerability, report a [new vulnerability]


## Workflow

This project uses a simple feature branch workflow, with commit messages
following the [Conventional Commits] standard.

Simply create a new branch off of `main` to do your work, separate your changes
into commits as appropriate and then open a pull request for review. Don't worry
if any of this is unfamiliar, since this can be fixed up before merging.


## Submitting a Translation

The easiest way to contribute a translation is through [Weblate], which
provides a web interface for translators.

To contribute a translation directly, open a pull request that adds your locale
to the [`LINGUAS`] file and your translated `.po` file to the [`po/`] directory.


## Contributing Code

This projects follows most of the conventions of a typical GNOME project
including coding style, documentation and introspection, with an emphasis on
automated testing.

If developing with GNOME Builder, select the `ca.andyholmes.Valent.Devel.json`
build configuration.


### Coding Style

This project generally follows the [GNOME C Coding Style], with GNU-style
indentation.

* Avoid GLib `typedef`s like `gchar` and `gint64`. Exceptions can be seen in the
  existing code, where the existing API can not be broken. For compatibility
  guarantees, see the static assertions in [`glib-init.c`].

* Function-like macros have a single space between the symbol name and the first
  parenthesis, just like regular functions.

* Use `g_return_if_fail ()` or `g_return_val_if_fail ()` in public functions and
  `g_assert ()` in private functions.

* Use `g_clear_pointer ()` and `g_clear_object ()` to free `struct` fields,
  `g_clear_handle_id ()` and `g_clear_signal_handler ()` to disconnect closures,
  and other macros that reset pointers to freed memory.

* Use `g_autoptr ()`, `g_autofree` and other cleanup macros to automatically
  free scoped memory.

* Use `g_steal_pointer ()` for explicit ownership transfer.


### Documentation & Introspection

Valent exposes a public API with GObject-Introspection support, allowing for
plugins written in C/C++, Python3 and Vala. The introspection data is then used
to generate API documentation, meaning that all public API must have proper
[GObject-Introspection annotations].

Private functions and classes should also be documented following the same
rules, whenever it makes sense.


### Testing

Valent has a thorough test suite that includes sanitizers, ABI compliance and
linters for licensing, spelling and more. All relevant tests must pass before
your contribution will be reviewed for merging.


## Licensing

The following table describes the preferred licensing for various types of
contributions:

| Type                                   | License            |
|----------------------------------------|--------------------|
| Code                                   | `GPL-3.0-or-later` |
| Documentation                          | `CC-BY-SA-4.0`     |
| Translations                           | `GPL-3.0-or-later` |
| Other (icons, metadata, configuration) | `CC0-1.0`          |

Contributions may be accepted under other licensing, such as code or icons
already distributed under an acceptable open source license.


[`glib-init.c`]: https://gitlab.gnome.org/GNOME/glib/blob/main/glib/glib-init.c
[GObject-Introspection annotations]: https://gi.readthedocs.io/en/latest/annotations/giannotations.html
[Conventional Commits]: https://www.conventionalcommits.org
[new discussion]: https://github.com/andyholmes/valent/discussions/new?category=feature-request
[new issue]: https://github.com/andyholmes/valent/issues/new
[new vulnerability]: https://github.com/andyholmes/valent/security/advisories/new
[Weblate]: https://hosted.weblate.org/engage/valent/
[`LINGUAS`]: https://github.com/andyholmes/valent/blob/main/po/LINGUAS
[`po/`]: https://github.com/andyholmes/valent/tree/main/po
[GNOME C Coding Style]: https://developer.gnome.org/documentation/guidelines/programming/coding-style.html

