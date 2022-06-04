# Contributing to Valent

Thanks for thinking about contributing to Valent!

Valent is an implementation of the KDE Connect protocol, built on GNOME platform
libraries. It is written entirely in C, with support for plugins written in
C/C++, Python3 or Vala.

The project primarily targets Linux systems running GNOME Shell or Phosh, but
will accept contributions for other UNIX-based systems like BSD and other
GTK-based environments like elementaryOS or XFCE.


## Reporting an Issue

Valent is currently in an early stage of development and not accepting bug
reports. Along with missing features, there are a large number of known issues
that will be resolved as a result of planned architecture changes.

Until the project is ready to start accepting reports, you are invited to
discuss issues, features and get help in the [Discussions][discussions].


## Submitting a Translation

Valent does not yet use a translation service like Weblate or Crowdin. You
should expect translatable strings to change frequently until the project is
more mature.

To contribute a translation, open a pull request which adds your locale to the
[`LINGUAS`][linguas] file and your translated `.po` file to the [`po/`][po_dir]
directory.


## Contributing Code

Valent follows most of the conventions of a typical GNOME project including
coding style, documentation and introspection, with an emphasis on automated
testing.


### Coding Style

Valent generally follows the [GNOME Coding Style][gnome-coding-style], with
GNU-style indentation.

* Don't use GLib typedefs like `gchar` or `guint`, unless they have some benefit
  like `gint64`.

* Function-like macros have a single space between the symbol name and the first
  parenthesis, just like regular functions.

* Use `g_return_if_fail ()` or `g_return_val_if_fail ()` in public functions and
  `g_assert ()` in private functions.

* Use `g_autoptr ()`, `g_autofree` and other cleanup macros to automatically
  free scoped memory.

* Use `g_steal_pointer ()` for explicit ownership transfer.

* Use `g_clear_pointer ()` and `g_clear_object ()` to free struct fields.

* Use `g_clear_handle_id ()` and `g_clear_signal_handler ()` to disconnect
  closures by ID.


### Documentation & Introspection

Valent exposes a public API with GObject-Introspection support, allowing for
plugins written in C/C++, Python3 and Vala. The introspection data is then used
to generate API documentation, meaning that all public API must have proper
[GObject-Introspection annotations][annotations].

Private functions and classes should also be documented following the same
rules, whenever it makes sense.


### Testing

Valent has a thorough test suite that includes sanitizers, ABI compliance and
linters for licensing, spelling and more. All relevant tests must pass before
your contribution will be reviewed for merging.

Contributions are required to include tests that result in `>= 75%` line
coverage. Note that the coverage check will also fail if your contribution
lowers the project average too much.


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


[annotations]: https://gi.readthedocs.io/en/latest/annotations/giannotations.html
[discussions]: https://github.com/andyholmes/valent/discussions
[linguas]: https://github.com/andyholmes/valent/blob/main/po/LINGUAS
[po_dir]: https://github.com/andyholmes/valent/tree/main/po
[gnome-coding-style]: https://developer.gnome.org/documentation/guidelines/programming/coding-style.html

