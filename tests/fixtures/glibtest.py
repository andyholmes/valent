#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# SPDX-FileCopyrightText: 2018 Red Hat, Inc
# SPDX-FileContributor: Benjamin Berg <bberg@redhat.com>
# SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>


"""This module provides helpers for running GLib-based test programs with
:mod:`~unittest`, especially :mod:`~dbusmock`.
"""


import os
import subprocess
import sys
import unittest
from typing import ClassVar


class GLibTestCase:
    """A :class:`~unittest.TestCase` mixin for wrapping GLib-based test
    executables, especially with python-dbusmock.

    The test executable path can be set by defining the environment variable
    ``G_TEST_EXE``. It can also be set in :meth:`~unittest.TestCase.setUpClass`
    with the class attribute :attr:`~glibtest.GLibTestCase.executable`.
    """

    # pylint: disable=too-few-public-methods

    executable: ClassVar[str] = os.environ.get('G_TEST_EXE', '')

    def test_run(self) -> None:
        """Run the executable at :attr:`~glibtest.GLibTestCase.executable` and
        propagate the TAP output.
        """

        try:
            test = subprocess.run([self.executable, '--tap'],
                                  capture_output=True,
                                  check=True,
                                  encoding='utf-8')

            sys.stdout.write(test.stdout)
            sys.stderr.write(test.stderr)
        except subprocess.SubprocessError as error:
            # pylint: disable-next=no-member
            self.fail(error.stdout) # type: ignore


def main() -> None:
    """A convenience method for running GLib-based tests. Put this in your
    ``__main__`` function instead of :func:`~unittest.main`.
    """

    # Output to stderr; we're forwarding TAP output of the real program
    runner = unittest.TextTestRunner(stream=sys.stderr, verbosity=2)
    unittest.main(testRunner=runner)
