#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>


"""This script generates documentation for the KDE Connect protocol in Markdown
format from JSON Schemas, which is then passed to ``g-docgen``.
"""


import collections.abc
import json
import os
import re
import sys
from typing import TextIO


#
# Markdown Helpers
#
MD_HEADER = '#'
MD_INDENT = '    '
MD_LIST = '*'
MD_SEPARATOR = '---'

MD_JSON_SCHEMA_REQUIRED = '[🔒](#symbols)'
MD_JSON_SCHEMA_CONTENTS = '''
* [Appendix](#appendix)
    * [Symbols](#symbols)
    * [Data Types](#data-types)
'''
MD_JSON_SCHEMA_APPENDIX = f'''
{MD_HEADER * 2} Appendix

{MD_HEADER * 3} Data Types

These are the basic data types in the KDE Connect protocol, as described by the
JSON Schema specification. Note that the `integer` type defined in some versions
of JSON Schema is never used.

{MD_HEADER * 4} `Boolean`

A logical data type that can have only the values `true` or `false`.

{MD_HEADER * 4} `Number`

A numeric data type in the double-precision 64-bit floating point format
(IEEE 754).

{MD_HEADER * 4} `String`

A null-terminated sequence of UTF-8 encoded bytes.

{MD_HEADER * 4} `Array`

An ordered collection of values.

{MD_HEADER * 4} `Object`

A mapping collection of string keys to values.

{MD_HEADER * 3} Symbols

* 🔒 **Required**

    Packets missing these fields may be ignored or result in undefined
    behaviour.

{MD_HEADER * 3} References

* <https://json-schema.org>
* <https://datatracker.ietf.org/doc/html/draft-fge-json-schema-validation-00>
'''


def md_slug(text: str) -> str:
    """Return a slug for ``text``."""

    value = re.sub(r'[^\w\s-]', '', text).strip().lower()
    return re.sub(r'[\s\-]+', '-', value)

def md_slug_link(text: str) -> str:
    """Return a slug wrapped in a link for ``text``."""

    slug = md_slug(text)
    return f'[{text}](#{slug})'

def md_enum(schema: dict) -> str:
    """Return a JSON Schema ``enum`` formatted for markdown."""

    enum = schema.get('enum', [])

    return '|'.join([f'`{repr(e)}`' for e in enum])

def md_range(schema: dict) -> str:
    """Return a JSON Schema number, accounting for ``minimum``, ``maximum``,
    ``exclusiveMinimum`` and ``exclusiveMaximum``, formatted for markdown."""

    lower = schema.get('minimum', None)

    if lower is None:
        lower = schema.get('exclusiveMinimum', None)
        lower = lower if lower is None else lower + 1

    upper = schema.get('maximum', None)

    if upper is None:
        upper = schema.get('exclusiveMaximum', None)
        upper = upper if upper is None else upper - 1

    if lower is not None and upper is not None:
        return f'`{repr(lower)}–{repr(upper)}`'

    return 'Unrestricted'

def md_pattern(schema: dict) -> str:
    """Return a JSON Schema ``pattern`` formatted for markdown."""

    pattern = schema.get('pattern', '.*')

    return f'`/{pattern}/`'

def md_type(schema: dict) -> str:
    """Return a JSON Schema ``type`` formatted for markdown."""

    type_names = {
        'boolean': 'Boolean',
        'number': 'Number',
        'string': 'String',
        'array': 'Array',
        'object': 'Object',
    }

    type_name = type_names.get(schema['type'], schema['type'])
    type_slug = md_slug(type_name)
    type_link = f'[**`{type_name}`**](#{type_slug})'

    if 'items' in schema:
        if 'title' in schema['items']:
            item_name = schema['items']['title']
            item_link = f'**`{item_name}`**'
            type_link = f'{type_link} of {item_link}'
        elif 'type' in schema['items']:
            item_link = md_type(schema['items'])
            type_link = f'{type_link} of {item_link}'

    return type_link


#
# Output
#

# pylint: disable-next=redefined-outer-name
def write_subschema(fobj: TextIO, schema: dict, indent: str = '') -> None:
    """Document a schema as a type."""

    title = schema.get('title')
    description = schema.get('description')
    type_str = md_type(schema)

    fobj.write(f'{indent}* **`{title}`** ({type_str})\n\n')
    indent += MD_INDENT
    fobj.write(f'{indent}{description}\n\n')

    for name, subschema in schema.get('properties', {}).items():
        write_schema(fobj, schema, name, subschema, indent)


# pylint: disable-next=redefined-outer-name
def write_schema(fobj: TextIO, parent: dict, name: str, schema: dict, indent: str = '') -> None:
    """Document a schema as a type field with its name, type and description.

    Child schemas will be treated as opaque values (eg. Array of Object) unless
    they contain ``title`` or ``description`` members.
    """

    title = schema.get('title', name)
    description = schema.get('description')
    type_link = md_type(schema)

    is_required = name in parent.get('required', [])
    required = f' {MD_JSON_SCHEMA_REQUIRED}' if is_required else ''

    fobj.write(f'{indent}* `{title}`: {type_link}{required}\n\n')
    indent += MD_INDENT

    if 'enum' in schema:
        fobj.write(f'{indent}**`enum`**: {md_enum(schema)}\n\n')

    if 'minimum' in schema or 'maximum' in schema or \
       'exclusiveMinimum' in schema or 'exclusiveMaximum' in schema:
        fobj.write(f'{indent}**`range`**: {md_range(schema)}\n\n')

    if 'pattern' in schema:
        fobj.write(f'{indent}**`pattern`**: {md_pattern(schema)}\n\n')

    fobj.write(f'{indent}{description}\n\n')

    # Search for subschemas with "title" or "description" and recurse into those
    if 'items' in schema:
        items = schema.get('items', [])

        if isinstance(items, collections.abc.Mapping):
            items = [items]

        for subschema in items:
            if 'title' in subschema or 'description' in subschema:
                write_subschema(fobj, subschema, indent)

    for subschema in schema.get('properties', {}).values():
        if 'title' in subschema or 'description' in subschema:
            write_subschema(fobj, subschema, indent)

    for subschema in schema.get('patternProperties', {}).values():
        if 'title' in subschema or 'description' in subschema:
            write_subschema(fobj, subschema, indent)


# pylint: disable-next=redefined-outer-name
def write_packet(fobj: TextIO, packet: dict, depth: int = 1) -> None:
    """Document a packet, with examples and a bullet-point for each field."""

    # Header
    header = MD_HEADER * depth
    title = packet.get('title', 'Untitled')
    description = packet.get('description', 'No description')

    fobj.write(f'{header} `{title}`\n\n')
    fobj.write(f'{description}\n\n')

    # Examples
    if 'examples' in packet:
        for example in packet.get('examples', {}):
            text = json.dumps(example, indent=4)
            fobj.write(f'```js\n{text}\n```\n\n')

    # Fields
    for name, schema in packet.get('properties', {}).items():
        # Root fields
        if 'title' in schema or 'description' in schema:
            write_schema(fobj, packet, name, schema)

        # Recurse into `body` fields
        if name == 'body':
            properties = schema.get('properties', {})
            pattern_properties = schema.get('patternProperties', {})

            if len(properties) > 0 or len(pattern_properties) > 0:
                for subname, subschema in properties.items():
                    write_schema(fobj, schema, subname, subschema)

                for subname, subschema in pattern_properties.items():
                    write_schema(fobj, schema, subname, subschema)
            else:
                fobj.write('This packet has no body fields.\n\n')


# pylint: disable-next=redefined-outer-name
def write_section(fobj: TextIO, section: dict, depth: int = 1) -> None:
    """Document a section with references."""

    # Header
    header = MD_HEADER * depth
    title = section.get('title', 'Untitled')
    description = section.get('description', 'No description')

    fobj.write(f'{header} {title}\n\n')
    fobj.write(f'{description}\n\n')

    # References
    if 'references' in section:
        header = '#' * (depth + 1)
        fobj.write(f'{header} References\n\n')

        for uri in section.get('references', []):
            fobj.write(f'* <{uri}>\n')

        fobj.write('\n')

    # Packets
    if 'packets' in section:
        header = '#' * (depth + 1)
        fobj.write(f'{header} Packets\n\n')

        for packet in section.get('packets', {}):
            write_packet(fobj, packet, depth + 2)


# pylint: disable-next=redefined-outer-name
def write_toc(fobj: TextIO, section: dict, packet: bool = False, depth: int = 0) -> None:
    """Write a table of contents for ``section``."""

    indent = MD_INDENT * depth
    title = section.get('title', 'Untitled')
    link = md_slug_link(f'`{title}`' if packet else title)
    fobj.write(f'{indent}* {link}\n')

    for subsection in section.get('sections', []):
        write_toc(fobj, subsection, False, depth + 1)

    for subsection in section.get('packets', []):
        write_toc(fobj, subsection, True, depth + 1)


def load_json(path: str) -> dict:
    """A simple helper for loading a JSON file from disk."""

    try:
        with open(path, 'rt', encoding='utf-8') as json_file:
            return json.load(json_file)
    except json.decoder.JSONDecodeError as error:
        raise RuntimeError(f'{path}: {error}') from error


# pylint: disable-next=redefined-outer-name
def write_index(fobj: TextIO, path: str) -> None:
    """Read the files described by ``index.json`` in the input directory and
    output a formatted Markdown document."""

    # Load the index file and any JSON Schemas it references
    index = load_json(path)
    index_dir = os.path.dirname(path)

    for section in index.get('sections', []):
        if 'packets' in section:
            packets = [os.path.join(index_dir, p) for p in section['packets']]
            section['packets'] = [load_json(f'{p}.json') for p in packets]

    # Header
    title = index.get('title', 'Untitled')
    description = index.get('description', 'No description')

    fobj.write(f'Title: {title}\n\n')
    fobj.write(f'# {title}\n\n')
    fobj.write(f'{description}\n\n')

    # Table of Contents
    fobj.write('## Table of Contents\n\n')

    for section in index.get('sections', []):
        write_toc(fobj, section)

    fobj.write(MD_JSON_SCHEMA_CONTENTS)
    fobj.write('\n\n')

    # Contents
    for section in index.get('sections', []):
        write_section(fobj, section, 2)

    fobj.write(MD_JSON_SCHEMA_APPENDIX)
    fobj.write('\n\n')


if __name__ == '__main__':
    if len(sys.argv) < 2:
        sys.stderr.write(f'{sys.argv[0]}: missing path to "index.json"\n')
        sys.exit(1)

    # If given the second argument is not given output is written to STDOUT
    INDEX_PATH = sys.argv[1]
    OUTPUT_PATH = sys.argv[2] if len(sys.argv) > 2 else ''

    if OUTPUT_PATH:
        with open(OUTPUT_PATH, 'wt', encoding='utf-8') as fobj:
            write_index(fobj, INDEX_PATH)
    else:
        write_index(sys.stdout, INDEX_PATH)
