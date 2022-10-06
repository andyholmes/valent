#!/bin/sh -e

# SPDX-License-Identifier: CC0-1.0
# SPDX-FileCopyrightText: No rights reserved

#
# Top-Level Environment Variables
#
WORKSPACE="${GITHUB_WORKSPACE:=$(git rev-parse --show-toplevel)}"
BUILDDIR="${BUILDDIR:=$WORKSPACE/_build}"


#
# General Variables
#
# | Variable                   | meson setup                     |
# |----------------------------|---------------------------------|
# | CX_COMPILER                | Compiler Collection (gcc, llvm) |
# | CX_SUITE                   | Test Profile                    |
CX_COMPILER=${CX_COMPILER:=default}
CX_SUITE=${CX_SUITE:=test}


#
# Profiles
#
cx_suite_pretest() {
    if command -v pylint > /dev/null 2>&1; then
        # shellcheck disable=SC2046
        pylint --rcfile tests/extra/setup.cfg \
               $(git ls-files '*.py')
    fi

    if command -v mypy > /dev/null 2>&1; then
        # shellcheck disable=SC2046
        mypy $(git ls-files '*.py')
    fi

    if command -v shellcheck > /dev/null 2>&1; then
        # shellcheck disable=SC2046
        shellcheck $(git ls-files '*.sh')
    fi
}

cx_suite_abidiff_build() {
    TARGET_REF="${1}"
    TARGET_DIR="${2}"

    git fetch origin "${TARGET_REF}" && \
    git checkout "${TARGET_REF}"

    BUILDSUBDIR="${BUILDDIR}/$(git rev-parse "${TARGET_REF}")"

    if [ ! -d "${BUILDSUBDIR}" ]; then
        # shellcheck disable=SC2086
        meson setup --buildtype="${CX_SETUP_BUILDTYPE:=release}" \
                    --prefix=/usr \
                    --libdir=lib \
                    ${CX_SETUP_ARGS} \
                    "${BUILDSUBDIR}"
    fi

    meson compile -C "${BUILDSUBDIR}" && \
    DESTDIR="${TARGET_DIR}" meson install -C "${BUILDSUBDIR}"
}

#
# PROFILE: abidiff
#
# Run the compiler's static analysis tool.
#
# The setup phase respects the following environment variables:
#
# | Variable                           | meson setup                         |
# |------------------------------------|-------------------------------------|
# | CX_SETUP_BUILDTYPE                 | --buildtype                         |
# | CX_SETUP_ARGS                      | additional command-line arguments   |
#
cx_suite_abidiff() {
    # Ensure a log directory exists where it is expected
    mkdir -p "${BUILDDIR}/meson-logs"

    if [ "${GITHUB_ACTIONS}" = "true" ]; then
        BASE_REF="${GITHUB_BASE_REF}"
        HEAD_REF="${GITHUB_HEAD_REF}"
    else
        BASE_REF="main"
        HEAD_REF=$(git rev-parse HEAD)
    fi

    # See: CVE-2022-24765
    git config --global --add safe.directory "${WORKSPACE}"

    BASE_DIR="${BUILDDIR}/_base"
    HEAD_DIR="${BUILDDIR}/_head"

    cx_suite_abidiff_build "${BASE_REF}" "${BASE_DIR}" > /dev/null 2>&1
    cx_suite_abidiff_build "${HEAD_REF}" "${HEAD_DIR}" > /dev/null 2>&1

    # Run `abidiff`
    abidiff --drop-private-types \
            --fail-no-debug-info \
            --no-added-syms \
            --headers-dir1 "${BASE_DIR}/usr/include" \
            --headers-dir2 "${HEAD_DIR}/usr/include" \
            "${BASE_DIR}/usr/lib/libvalent.so" \
            "${HEAD_DIR}/usr/lib/libvalent.so" > \
            "${BUILDDIR}/meson-logs/abidiff.log" || \
    (cat "${BUILDDIR}/meson-logs/abidiff.log" && exit 1);
}

#
# PROFILE: cppcheck
#
# Run the `cppcheck` tool on the source code.
#
# The profile respects the following environment variables:
#
# | Variable                           | CLI flag                            |
# |------------------------------------|-------------------------------------|
# | CX_CPPCHECK_LIBRARY                | --library                           |
# | CX_CPPCHECK_SUPPRESSIONS           | --timeout-multiplier                |
# | CX_CPPCHECK_ARGS                   | additional command-line arguments   |
#
cx_suite_cppcheck() {
    # Ensure a log directory exists where it is expected
    mkdir -p "${BUILDDIR}/meson-logs"

    if [ "${CX_CPPCHECK_LIBRARY}" != "" ]; then
        CX_CPPCHECK_ARGS="${CX_CPPCHECK_ARGS} --library=${CX_CPPCHECK_LIBRARY}"
    fi

    if [ "${CX_CPPCHECK_SUPPRESSIONS}" != "" ]; then
        CX_CPPCHECK_ARGS="${CX_CPPCHECK_ARGS} --suppressions-list=${CX_CPPCHECK_SUPPRESSIONS}"
    fi

    # shellcheck disable=SC2086
    cppcheck --error-exitcode=1 \
             --library=gtk \
             --quiet \
             --xml \
             ${CX_CPPCHECK_ARGS} \
             src 2> "${BUILDDIR}/meson-logs/cppcheck.xml" || \
    (cppcheck-htmlreport --file "${BUILDDIR}/meson-logs/cppcheck.xml" \
                         --report-dir "${BUILDDIR}/meson-logs/cppcheck-html" \
                         --source-dir "${WORKSPACE}" && exit 1);
}

#
# PROFILE: coverage
#
# A custom lcov generator.
#
# If $BUILDDIR does not exist, `cx_suite_test()` will be called and relayed
# any arguments passed to the function.
#
# The coverage phase respects the following environment variables:
#
# | Variable                           | lcov                                |
# |------------------------------------|-------------------------------------|
# | CX_LCOV_INCLUDE_PATH               | path glob to include                |
# | CX_LCOV_EXCLUDE_PATH               | path glob to exclude                |
#
cx_suite_coverage() {
    if [ ! -d "${BUILDDIR}" ]; then
        CX_SETUP_COVERAGE=true
        cx_suite_test "${@}"
    fi

    lcov --directory "${BUILDDIR}" \
         --capture \
         --initial \
         --output-file "${BUILDDIR}/meson-logs/coverage.p1" && \
    lcov --directory "${BUILDDIR}" \
         --capture \
         --no-checksum \
         --rc lcov_branch_coverage=1 \
         --output-file "${BUILDDIR}/meson-logs/coverage.p2" && \
    lcov --add-tracefile "${BUILDDIR}/meson-logs/coverage.p1" \
         --add-tracefile "${BUILDDIR}/meson-logs/coverage.p2" \
         --rc lcov_branch_coverage=1 \
         --output-file "${BUILDDIR}/meson-logs/coverage.info"

    # shellcheck disable=SC2086
    lcov --extract "${BUILDDIR}/meson-logs/coverage.info" \
         "${CX_LCOV_INCLUDE_PATH}" \
         --rc lcov_branch_coverage=1 \
         --output-file "${BUILDDIR}/meson-logs/coverage.info" && \
    lcov --remove "${BUILDDIR}/meson-logs/coverage.info" \
         "${CX_LCOV_EXCLUDE_PATH}" \
         --rc lcov_branch_coverage=1 \
         --output-file "${BUILDDIR}/meson-logs/coverage.info"

    genhtml --prefix "${WORKSPACE}" \
            --output-directory "${BUILDDIR}/meson-logs/coverage-html" \
            --title 'Code Coverage' \
            --legend \
            --show-details \
            --branch-coverage \
            "${BUILDDIR}/meson-logs/coverage.info"
}

#
# PROFILE: test
#
# Setup a `meson` build directory at $BUILDDIR, compile the project and run the
# test suite. Each phase can be configured with environment variables.
#
# The setup phase respects the following environment variables:
#
# | Variable                           | meson setup                         |
# |------------------------------------|-------------------------------------|
# | CX_SETUP_BUILDTYPE                 | --buildtype                         |
# | CX_SETUP_COVERAGE                  | -Db_coverage                        |
# | CX_SETUP_LUNDEF                    | -Db_lundef                          |
# | CX_SETUP_NDEBUG                    | -Db_ndebug                          |
# | CX_SETUP_SANITIZE                  | -Db_sanitize                        |
# | CX_SETUP_ARGS                      | additional command-line arguments   |
#
#
# The test phase respects the following environment variables:
#
# | Variable                           | meson test                          |
# |------------------------------------|-------------------------------------|
# | CX_TEST_REPEAT                     | --repeat                            |
# | CX_TEST_TIMEOUT_MULTIPLIER         | --timeout-multiplier                |
# | CX_TEST_ARGS                       | additional command-line arguments   |
#
cx_suite_test() {
    if [ ! -d "${BUILDDIR}" ]; then
        # shellcheck disable=SC2086
        meson setup --buildtype="${CX_SETUP_BUILDTYPE:=debugoptimized}" \
                    -Db_coverage="${CX_SETUP_COVERAGE:=false}" \
                    -Db_lundef="${CX_SETUP_LUNDEF:=true}" \
                    -Db_ndebug="${CX_SETUP_NDEBUG:=false}" \
                    -Db_sanitize="${CX_SETUP_SANITIZE:=none}" \
                    ${CX_SETUP_ARGS} \
                    "${BUILDDIR}"
    fi

    # Build
    meson compile -C "${BUILDDIR}"

    # shellcheck disable=SC2086
    dbus-run-session xvfb-run -d \
    meson test -C "${BUILDDIR}" \
               --print-errorlogs \
               --repeat="${CX_TEST_REPEAT:=1}" \
               --timeout-multiplier="${CX_TEST_TIMEOUT_MULTIPLIER:=1}" \
               ${CX_TEST_ARGS} \
               "${@}"
}

#
# PROFILE: asan
#
# Run the "test" profile with AddressSantizer and UndefinedBehaviourSanitizer.
#
# The setup and test phases respect the same environment variables as the "test"
# profile.
#
cx_suite_asan() {
    CX_SETUP_SANITIZE="address,undefined"

    # Clang needs `-Db_lundef=false` to use sanitizers
    if [ "${CC}" = "clang" ]; then
        CX_SETUP_LUNDEF=false
    fi

    # Chain-up to the test profile
    export CX_TEST_TIMEOUT_MULTIPLIER=3
    cx_suite_test "${@}"
}

#
# PROFILE: tsan
#
# Run the "test" profile with ThreadSanitizer.
#
# The setup and test phases respect the same environment variables as the "test"
# profile.
#
cx_suite_tsan() {
    CX_SETUP_SANITIZE="thread"

    # Clang needs `-Db_lundef=false` to use sanitizers
    if [ "${CC}" = "clang" ]; then
        CX_SETUP_LUNDEF=false
    fi

    # Chain-up to the test profile
    export CX_TEST_TIMEOUT_MULTIPLIER=3
    cx_suite_test "${@}"
}

#
# PROFILE: analyzer
#
# Run the compiler's static analysis tool.
#
# The setup phase respects the following environment variables:
#
# | Variable                           | meson setup                         |
# |------------------------------------|-------------------------------------|
# | CX_SETUP_BUILDTYPE                 | --buildtype                         |
# | CX_SETUP_ARGS                      | additional command-line arguments   |
#
cx_suite_analyzer() {
    # GCC analyzer
    if [ "${CC}" = "gcc" ]; then
        export CFLAGS="${CFLAGS} -fanalyzer"

        # shellcheck disable=SC2086
        meson setup --buildtype="${CX_SETUP_BUILDTYPE:=debug}" \
                    ${CX_SETUP_ARGS} \
                    "${BUILDDIR}" &&
        meson compile -C "${BUILDDIR}"

    # clang-analyzer
    elif [ "${CC}" = "clang" ]; then
        export SCANBUILD="${WORKSPACE}/tests/extra/scanbuild.sh"

        # shellcheck disable=SC2086
        meson setup --buildtype="${CX_SETUP_BUILDTYPE:=debug}" \
                    ${CX_SETUP_ARGS} \
                    "${BUILDDIR}" &&
        ninja -C "${BUILDDIR}" scan-build
    fi
}

cx_main() {
    if [ "${CX_COMPILER}" = "gcc" ]; then
        export CC=gcc
        export CC_LD=bfd
        export CXX=g++
        export CXX_LD=bfd
    elif [ "${CX_COMPILER}" = "llvm" ]; then
        export CC=clang
        export CC_LD=lld
        export CXX=clang++
        export CXX_LD=lld
    fi

    # Compiler Profiles
    if [ "${CX_SUITE}" = "pretest" ]; then
        cx_suite_pretest "${@}";
    elif [ "${CX_SUITE}" = "test" ]; then
        cx_suite_test "${@}";
    elif [ "${CX_SUITE}" = "asan" ]; then
        cx_suite_asan "${@}";
    elif [ "${CX_SUITE}" = "tsan" ]; then
        cx_suite_tsan "${@}";
    elif [ "${CX_SUITE}" = "analyzer" ]; then
        cx_suite_analyzer;
    elif [ "${CX_SUITE}" = "coverage" ]; then
        cx_suite_coverage "${@}";

    # Other tools
    elif [ "${CX_SUITE}" = "abidiff" ]; then
        cx_suite_abidiff;
    elif [ "${CX_SUITE}" = "cppcheck" ]; then
        cx_suite_cppcheck;
    fi
}

cx_main "${@}";

