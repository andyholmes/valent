#!/usr/bin/python3
# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2022 Andy Holmes <andrew.g.r.holmes@gmail.com>


"""This script checks libvalent between two git references for ABI breaks.
"""


import argparse
import contextlib
import os
import shutil
import subprocess
import sys


def get_current_revision():
    """Get the current git revision.
    """

    revision = subprocess.check_output(['git', 'rev-parse', '--abbrev-ref', 'HEAD'],
                                       encoding='utf-8').strip()

    if revision == 'HEAD':
        # This is a detached HEAD, get the commit hash
        revision = subprocess.check_output(['git', 'rev-parse', 'HEAD'],
                                           encoding='utf-8').strip()

    return revision


@contextlib.contextmanager
def checkout_git_revision(revision):
    """Checkout `revision`.
    """

    current_revision = get_current_revision()
    subprocess.check_call(['git', 'checkout', '-q', revision])

    try:
        yield
    finally:
        subprocess.check_call(['git', 'checkout', '-q', current_revision])


def build_install(revision):
    """Build `revision`.
    """

    build_dir = '_build'
    dest_dir = os.path.abspath(revision.replace('/', '-'))

    with checkout_git_revision(revision):
        shutil.rmtree(build_dir, ignore_errors=True)
        shutil.rmtree(dest_dir, ignore_errors=True)

        subprocess.check_call(['meson', 'setup', build_dir,
                               '--prefix=/usr',
                               '--libdir=lib',
                               '-Db_coverage=false',
                               '-Ddocumentation=false',
                               '-Dintrospection=false',
                               '-Dtests=false',
                               '-Dplugin_battery=false',
                               '-Dplugin_bluez=false',
                               '-Dplugin_clipboard=false',
                               '-Dplugin_contacts=false',
                               '-Dplugin_eds=false',
                               '-Dplugin_fdo=false',
                               '-Dplugin_findmyphone=false',
                               '-Dplugin_gtk=false',
                               '-Dplugin_lan=false',
                               '-Dplugin_lock=false',
                               '-Dplugin_mousepad=false',
                               '-Dplugin_mpris=false',
                               '-Dplugin_notification=false',
                               '-Dplugin_photo=false',
                               '-Dplugin_ping=false',
                               '-Dplugin_pulseaudio=false',
                               '-Dplugin_runcommand=false',
                               '-Dplugin_sftp=false',
                               '-Dplugin_share=false',
                               '-Dplugin_sms=false',
                               '-Dplugin_systemvolume=false',
                               '-Dplugin_telephony=false',
                               '-Dplugin_xdp=false'])
        subprocess.check_call(['meson', 'compile', '-C', build_dir])
        subprocess.check_call(['meson', 'install', '-C', build_dir],
                              env={'DESTDIR': dest_dir})

    return dest_dir


def compare(old_dir, new_dir):
    """Compare the ABIs of the libraries installed in `old_dir` and `new_dir`.
    """

    old_headers = os.path.join(old_dir, 'usr', 'include')
    old_library = os.path.join(old_dir, 'usr', 'lib', 'libvalent.so')

    new_headers = os.path.join(new_dir, 'usr', 'include')
    new_library = os.path.join(new_dir, 'usr', 'lib', 'libvalent.so')

    subprocess.check_call(['abidiff', '--drop-private-types',
                                      '--fail-no-debug-info',
                                      '--no-added-syms',
                                      '--headers-dir1', old_headers,
                                      '--headers-dir2', new_headers,
                                      old_library, new_library])

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('stable', help='git reference of the stable ABI')
    parser.add_argument('commit', help='git reference of the new ABI')

    args = parser.parse_args()

    if args.stable == args.commit:
        sys.exit(0)

    # See: CVE-2022-24765
    subprocess.check_call(['git', 'config',
                                  '--global',
                                  '--add',
                                  'safe.directory',
                                  os.getcwd()])

    stable_dir = build_install(args.stable)
    commit_dir = build_install(args.commit)

    print('ABI Compliance',
          f'  Stable:  {args.stable}',
          f'  Current: {args.commit}',
          end='\n\n', flush=True, sep='\n')

    try:
        compare(stable_dir, commit_dir)
    except subprocess.CalledProcessError as e:
        sys.exit(1)
