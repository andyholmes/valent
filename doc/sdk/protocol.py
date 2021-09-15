#!/usr/bin/env python3

# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

import collections.abc
import json
import os
import re
import sys


# Arguments
INPUT_FILE = sys.argv[1]
INPUT_DIR = os.path.dirname(INPUT_FILE)
OUTPUT_FILE = sys.stdout


def load_json(filename):
    path = os.path.join(INPUT_DIR, filename)

    try:
        with open(path, 'rt') as fobj:
            return json.load(fobj)
    except json.decoder.JSONDecodeError as e:
        raise RuntimeError('{0}: {1}'.format(path, e))


#
# Markdown Helpers
#
MD_HEADER = '#'
MD_INDENT = '    '
MD_LIST = '*'
MD_SEPARATOR = '---'

MD_JSON_SCHEMA_REQUIRED = '[🔒](#symbols)'
MD_JSON_SCHEMA_CONTENTS = f'''
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


def md_slug(text):
    value = re.sub(r'[^\w\s-]', '', text).strip().lower()
    return re.sub(r'[\s\-]+', '-', value)

def md_slug_link(text):
    slug = md_slug(text)
    return f'[{text}](#{slug})'

def md_enum(schema):
    return '|'.join([f'`{repr(e)}`' for e in schema.get('enum', [])])

def md_pattern(schema):
    return '`/{0}/`'.format(schema.get('pattern', '.*'))

def md_type(schema):
    JSON_SCHEMA_TYPE = {
        'boolean': 'Boolean',
        'number': 'Number',
        'string': 'String',
        'array': 'Array',
        'object': 'Object',
    }

    type_text = JSON_SCHEMA_TYPE.get(schema['type'], schema['type'])
    type_slug = md_slug(type_text)
    type_link = f'[**`{type_text}`**](#{type_slug})'

    if 'items' in schema:
        if 'title' in schema['items']:
            item_text = schema['items']['title']
            item_link = f'**`{item_text}`**'
            type_link = f'{type_link} of {item_link}'
        elif 'type' in schema['items']:
            item_link = md_type(schema['items'])
            type_link = f'{type_link} of {item_link}'

    return type_link


#
# Output
#
def write_subschema(fobj, schema, indent = ''):
    """Document a schema as a type."""

    title = schema.get('title')
    description = schema.get('description')
    type_str = md_type(schema)

    fobj.write(f'{indent}* **`{title}`** ({type_str})\n\n')
    indent += MD_INDENT
    fobj.write(f'{indent}{description}\n\n')

    for name, subschema in schema.get('properties', {}).items():
        write_schema(fobj, schema, name, subschema, indent)


def write_schema(fobj, parent, name, schema, indent = ''):
    """Document a schema as a type field with its name, type and description.

    Child schemas will be treated as opaque values (eg. Array of Object) unless
    they contain ``title`` or ``description`` members.
    """

    title = schema.get('title', name)
    description = schema.get('description')
    type_link = md_type(schema)

    required = name in parent.get('required', [])
    required = f' {MD_JSON_SCHEMA_REQUIRED}' if required else ''

    fobj.write(f'{indent}* `{title}`: {type_link}{required}\n\n')
    indent += MD_INDENT

    if 'enum' in schema:
        fobj.write(f'{indent}**`enum`**: {md_enum(schema)}\n\n')

    if 'pattern' in schema:
        fobj.write(f'{indent}**`pattern`**: {md_pattern(schema)}\n\n')

    fobj.write(f'{indent}{description}\n\n')

    # Search for subschemas with "title" or "description" and recurse into those
    if 'items' in schema:
        items = schema.get('items')

        if isinstance(items, collections.abc.Mapping):
            items = [items]

        for subschema in items:
            if 'title' in subschema or 'description' in subschema:
                write_subschema(fobj, subschema, indent)

    for subschema in schema.get('properties', {}).values():
        if 'title' in subschema or 'description' in subschema:
            write_subschema(fobj, subschema, indent)


def write_packet(fobj, packet, header_depth = 1):
    """Document a packet, with examples and a bullet-point for each field."""

    # Header
    header = '#' * header_depth
    title = packet.get('title')
    description = packet.get('description')

    fobj.write(f'{header} `{title}`\n\n')
    fobj.write(f'{description}\n\n')

    # Examples
    if 'examples' in packet:
        for example in packet.get('examples'):
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

            if len(properties) > 0:
                for name, subschema in properties.items():
                    write_schema(fobj, schema, name, subschema)
            else:
                fobj.write(f'This packet has no body fields.\n\n')


def write_section(fobj, section, header_depth = 1):
    # Header
    header = '#' * header_depth
    title = section.get('title')
    description = section.get('description')

    fobj.write(f'{header} {title}\n\n')
    fobj.write(f'{description}\n\n')

    # References
    if 'references' in section:
        header = '#' * (header_depth + 1)
        fobj.write(f'{header} References\n\n')

        for uri in section.get('references'):
            fobj.write(f'* <{uri}>\n')

        fobj.write('\n')

    # Packets
    if 'packets' in section:
        header = '#' * (header_depth + 1)
        fobj.write(f'{header} Packets\n\n')

        for packet in section.get('packets'):
            write_packet(fobj, packet, header_depth + 2)


def write_toc(fobj, section, list_depth = 0):
    indent = MD_INDENT * list_depth
    link = md_slug_link(section.get('title', 'Untitled'))
    fobj.write(f'{indent}* {link}\n')

    for subsection in section.get('sections', []):
        write_toc(fobj, subsection, list_depth + 1)

    for packet in section.get('packets', []):
        indent = MD_INDENT * (list_depth + 1)
        link = md_slug_link('`{0}`'.format(packet.get('title', 'Untitled')))
        fobj.write(f'{indent}* {link}\n')


def write_index(fobj):
    index = load_json('index.json')

    # Load packet schemas
    for section in index.get('sections', []):
        if 'packets' in section:
            packets = section['packets']
            section['packets'] = [load_json(f'{p}.json') for p in packets]

    # Header
    fobj.write('Title: {0}\n\n'.format(index['title']))
    fobj.write('# {0}\n\n'.format(index['title']))
    fobj.write('{0}\n\n'.format(index['description']))

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


# Usage: protocol.py INPUT [OUTPUT]
if __name__ == '__main__':
    if len(sys.argv) > 2:
        OUTPUT_FILE = sys.argv[2]

        with open(OUTPUT_FILE, 'wt') as fobj:
            write_index(fobj)
    else:
        write_index(sys.stdout)
            
