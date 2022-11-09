#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# SPDX-FileCopyrightText: 2018 Red Hat, Inc
# SPDX-FileContributor: Benjamin Berg <bberg@redhat.com>
# SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>


"""This module provides helpers for running GLib-based test programs with
:mod:`~unittest`, especially :mod:`~dbusmock`.
"""


import os
import subprocess
import sys
import functools
import unittest
from typing import Any, Callable, ClassVar, Optional, Type, TypeVar


GLibTestCaseAttr = TypeVar('GLibTestCaseAttr', bound='_GLibTestCaseAttr')

class _GLibTestCaseAttr:
    """An attribute which creates a bound method for running the test at the
    specified path.
    """

    # pylint: disable=bad-staticmethod-argument,missing-function-docstring
    # pylint: disable=too-few-public-methods

    def __init__(self, path: str) -> None:
        self.path = path

    @staticmethod
    def __func(self, path: str) -> None:
        self._test_path(path)

    # pylint: disable-next=line-too-long
    def __get__(self, instance: Type[GLibTestCaseAttr], owner: Optional[Any]) -> Callable[[unittest.TestCase], None]:
        # pylint: disable-next=no-value-for-parameter
        bound_method = self.__func.__get__(instance, owner)
        partial_method = functools.partial(bound_method, self.path)
        partial_method.__doc__ = bound_method.__doc__
        partial_method.__qualname__ = f'{owner.__qualname__}.{self.path}' # type: ignore

        return partial_method


GLibTestCaseMeta = TypeVar('GLibTestCaseMeta', bound='_GLibTestCaseMeta')

class _GLibTestCaseMeta(type):
    """Metaclass for :class:`~glibtest.GLibTestCase`."""

    # pylint: disable-next=line-too-long,undefined-variable
    def __new__(cls: Type[GLibTestCaseMeta], name: str, bases: tuple, namespace: dict) -> GLibTestCaseMeta:
        test_case = type.__new__(cls, name, bases, namespace)

        if test_case.executable: # type: ignore
            _GLibTestCaseMeta.query_tests(test_case)

        return test_case

    @staticmethod
    def query_tests(test_case: GLibTestCaseMeta) -> None:
        """Load the test cases from :attr:`~glibtest.GLibTestCase.executable`.

        A function attribute is set on ``test_case`` for each result so the
        runner executes them in the proper order. In other words, each GLib
        test case becomes a single test of a :class:`~unittest.TestCase`.
        """

        env = os.environ.copy()
        env['G_MESSAGES_DEBUG'] = ''

        try:
            tests = subprocess.run([test_case.executable, '-l'], # type: ignore
                                   capture_output=True,
                                   check=True,
                                   encoding='utf-8',
                                   env=env,
                                   timeout=15)
        except subprocess.TimeoutExpired as error:
            sys.stderr.write(error.stdout.decode('utf-8')) # type: ignore
            raise RuntimeError(f'Querying tests: {error}') from error
        except subprocess.CalledProcessError as error:
            sys.stderr.write(error.stdout)
            raise RuntimeError(f'Querying tests: {error}') from error
        else:
            n_tests = 0

            for line in tests.stdout.split('\n'):
                # Filter empty lines, diagnostics and plan
                if not line or line.startswith(('#', '1..',)):
                    continue

                n_tests += 1
                setattr(test_case,
                        f'test_{n_tests:03d}_{line}',
                        _GLibTestCaseAttr(line))


class GLibTestCase(metaclass = _GLibTestCaseMeta):
    """A :class:`~unittest.TestCase` mixin for wrapping GLib-based test
    executables, especially with python-dbusmock.

    The test executable path can be set by defining the environment variable
    ``G_TEST_EXE``. It can also be set in :meth:`~unittest.TestCase.setUpClass`
    with the class attribute :attr:`~glibtest.GLibTestCase.executable`.
    """

    # pylint: disable=too-few-public-methods

    executable: ClassVar[str] = os.environ.get('G_TEST_EXE', '')

    def _test_path(self, path: str) -> None:
        """Run the test at ``path`` in the GLib-based executable at
        :attr:`~glibtest.GLibTestCase.executable`.
        """

        try:
            test = subprocess.run([self.executable, '-p', path],
                                  capture_output=True,
                                  check=True,
                                  encoding='utf-8')

        # NOTE: If the meson timeout is reached, *this* process will be killed
        #       and output from the test executable will not be propagated.
        except subprocess.TimeoutExpired as error:
            stdout = '' if not error.stdout else error.stdout.decode('utf-8')
            stderr = '' if not error.stderr else error.stderr.decode('utf-8')

            # pylint: disable-next=no-member
            self.fail(f'\nstdout:\n{stdout}\nstderr:\n{stderr}') # type: ignore

        # On failure, preserve the output and pipe
        except subprocess.CalledProcessError as error:
            # pylint: disable-next=no-member
            self.fail(f'\nstdout:\n{error.stdout}\nstderr:\n{error.stderr}') # type: ignore

        # On success, propagate the output verbatim
        else:
            sys.stdout.write(test.stdout)
            sys.stderr.write(test.stderr)


def main() -> None:
    """A convenience method for running GLib-based tests. Put this in your
    ``__main__`` function instead of :func:`~unittest.main`.
    """

    # avoid writing to stderr
    runner = unittest.TextTestRunner(stream=sys.stdout, verbosity=2)
    unittest.main(testRunner=runner)
