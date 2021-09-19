#!/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# SPDX-FileCopyrightText: 2021 Carlos Garnacho <carlosg@gnome.org>
# SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>
# SPDX-FileComment: This file is from mutter <https://gitlab.gnome.org/GNOME/mutter>


"""CI script for running uncrustify."""

# pylint: disable=protected-access

import argparse
import os
import re
import subprocess
import sys
import tempfile
from typing import IO, Iterator


# Find the source root
# Change CWD to script location, necessary for always locating the configuration file
UNCRUSTIFY_CFG = 'uncrustify.cfg'

try:
    root = subprocess.run(['git', 'rev-parse', '--show-toplevel'],
                          capture_output=True,
                          check=True,
                          encoding='utf-8')
except subprocess.CalledProcessError as error:
    SOURCE_ROOT = os.getcwd()
else:
    SOURCE_ROOT = root.stdout.strip()
finally:
    os.chdir(os.path.dirname(os.path.abspath(sys.argv[0])))


def diff_chunks(sha: str) -> Iterator[dict]:
    """Collect the diff chunks."""

    file_entry_re = re.compile(r'^\+\+\+ b/(.*)$')
    diff_chunk_re = re.compile(r'^@@ -\d+,\d+ \+(\d+),(\d+)')
    file = ''

    diff = subprocess.run(['git', 'diff', '-U0', '--function-context', sha, 'HEAD'],
                          stdout=subprocess.PIPE,
                          stderr=subprocess.STDOUT,
                          check=True,
                          encoding='utf-8')

    for line in diff.stdout.split('\n'):
        match = file_entry_re.match(line)
        if match:
            file = match.group(1)

        match = diff_chunk_re.match(line)
        if match:
            start = int(match.group(1))
            length = int(match.group(2))
            end = start + length

            if length > 0 and (file.endswith('.c') or file.endswith('.h')):
                yield {
                    'file': os.path.join(SOURCE_ROOT, file),
                    'start': start,
                    'end': end
                }


def uncrustify_chunk(chunk: dict, output: IO[bytes]) -> None:
    "Run a chunk through uncrustify."

    # pylint: disable-next=consider-using-with
    check = subprocess.Popen(['uncrustify', '-c', UNCRUSTIFY_CFG, '-l', 'c'],
                             stdin=subprocess.PIPE,
                             stdout=subprocess.PIPE)

    if not check.stdin or not check.stdout:
        return

    # Open the source file
    with open(os.path.join(SOURCE_ROOT, chunk['file']), mode='rb') as fobj:
        # Inject INDENT-ON/INDENT-OFF helpers into the input of uncrustify
        for i, line in enumerate(fobj):
            if i == chunk['start'] - 2:
                check.stdin.write(b'/** *INDENT-ON* **/\n')

            check.stdin.write(line)

            if i == chunk['end'] - 2:
                check.stdin.write(b'/** *INDENT-OFF* **/\n')

    check.stdin.close()

    # Extract INDENT-ON/INDENT-OFF helpers from the output of uncrustify
    for line in check.stdout:
        if line not in (b'/** *INDENT-OFF* **/\n', b'/** *INDENT-ON* **/\n'):
            output.write(line)

    check.stdout.close()
    check.wait()


def uncrustify_diff(file: str, proposed: IO[bytes]) -> bool:
    """Show proposed changes to ``file``."""

    diff = subprocess.run(['diff', '-up', '--color=always', file, '-'],
                          capture_output=True,
                          check=False,
                          encoding='utf-8',
                          stdin=proposed)

    # 0 if inputs are the same, 1 if different, 2 if trouble
    if diff.returncode == 0:
        return False

    if diff.returncode == 1:
        print(re.sub('\t', '↦\t', diff.stdout))
        return True

    if diff.returncode == 2:
        diff.check_returncode()

    return False


def uncrustify_patch(file: str, proposed: IO[bytes]) -> bool:
    """Write proposed changes to ``file``."""

    # pylint: disable-next=consider-using-with
    diff = subprocess.Popen(['diff', '-up', file, '-'],
                            stdout=subprocess.PIPE,
                            stdin=proposed)
    # pylint: disable-next=consider-using-with
    patch = subprocess.Popen(['patch', file],
                             stdin=diff.stdout)
    diff.stdout.close() # type: ignore
    patch.communicate()

    return True


def uncrustify(sha: str, show: bool) -> bool:
    """Run uncrustify on each of ``chunks``."""

    changed = False

    for chunk in diff_chunks(sha):
        with tempfile.TemporaryFile() as changes:
            uncrustify_chunk(chunk, changes)
            changes.seek(0)

            if show:
                changed = uncrustify_diff(chunk['file'], changes)
            else:
                changed = uncrustify_patch(chunk['file'], changes)

    return changed


parser = argparse.ArgumentParser(description='Check code style.')
parser.add_argument('--sha', metavar='SHA', type=str,
                    help='SHA for the commit to compare HEAD with')
parser.add_argument('--dry-run', '-d', type=bool,
                    action=argparse.BooleanOptionalAction,
                    help='Only print changes to stdout, do not change code')
parser.add_argument('--rewrite', '-r', type=bool,
                    action=argparse.BooleanOptionalAction,
                    help='Whether to amend the result to the last commit \
                          (e.g. \'git rebase --exec "%(prog)s -r"\')')

# Change CWD to script location, necessary for always locating the configuration file
# os.chdir(os.path.dirname(os.path.abspath(sys.argv[0])))

args = parser.parse_args()
BASE_SHA = args.sha or 'HEAD^'
REWRITE = args.rewrite
DRY_RUN = args.dry_run
CHANGED = False


try:
    CHANGED = uncrustify(BASE_SHA, DRY_RUN)
except subprocess.CalledProcessError as error:
    print(f'STDOUT: {os.strerror(error.returncode)}: {error.stdout}')
    print(f'STDERR: {os.strerror(error.returncode)}: {error.stderr}')
    raise error


if DRY_RUN is not True and REWRITE is True:
    subprocess.run(['git', 'commit', '--all', '--amend', '-C', 'HEAD'],
                   check=True,
                   stdout=subprocess.DEVNULL)
    os._exit(0)
elif DRY_RUN is True and CHANGED is True:
    print ('\nIssue the following command in your local tree to apply the' +
           ' suggested changes (needs uncrustify installed):\n\n $ git rebase' +
           ' origin/master --exec "./check-style.py -r" \n')
    os._exit(-1)

os._exit(0)
