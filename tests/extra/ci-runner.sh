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
# | CX_PROFILE                 | Test Profile                    |
#
#
# Setup Variables
#
# | Variable                   | meson setup                     |
# |----------------------------|---------------------------------|
# | CX_SETUP_BUILDTYPE         | --buildtype                     |
# | CX_SETUP_COVERAGE          | -Db_coverage                    |
# | CX_SETUP_SANITIZE          | -Db_sanitize                    |
# | CX_SETUP_OPTIONS           | extra options                   |
#
#
# Test Variables
#
# | Variable                   | meson test                      |
# |----------------------------|---------------------------------|
# | CX_TEST_REPEAT             | --repeat                        |
# | CX_TEST_TIMEOUT_MULTIPLIER | --timeout-multiplier            |
# | CX_TEST_OPTIONS            | extra options                   |
#
#
# Other Variables
#
# | Variable                   | CLI flag                        |
# |----------------------------|---------------------------------|
# | CX_CPPCHECK_LIBRARY        | --library                       |
# | CX_CPPCHECK_SUPPRESSIONS   | --timeout-multiplier            |
# | CX_CPPCHECK_OPTIONS        | extra options                   |
# | CX_LCOV_EXCLUDE_PATH       | glob paths to exclude           |
# | CX_LCOV_INCLUDE_PATH       | glob paths to include           |
#


#
# pretest
#
cx_pre_test() {
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


#
# analyze
#
cx_analyze_abidiff_build() {
    TARGET_REF="${1}"
    TARGET_DIR="${2}"

    git fetch origin "${TARGET_REF}" && \
    git checkout "${TARGET_REF}"

    BUILDSUBDIR="${BUILDDIR}/$(git rev-parse "${TARGET_REF}")"

    if [ ! -d "${BUILDSUBDIR}" ]; then
        meson setup --prefix=/usr \
                    --libdir=lib \
                    -Db_coverage=false \
                    -Ddocumentation=false \
                    -Dintrospection=false \
                    -Dtests=false \
                    -Dplugin_battery=false \
                    -Dplugin_bluez=false \
                    -Dplugin_clipboard=false \
                    -Dplugin_contacts=false \
                    -Dplugin_eds=false \
                    -Dplugin_fdo=false \
                    -Dplugin_findmyphone=false \
                    -Dplugin_gtk=false \
                    -Dplugin_lan=false \
                    -Dplugin_lock=false \
                    -Dplugin_mousepad=false \
                    -Dplugin_mpris=false \
                    -Dplugin_notification=false \
                    -Dplugin_photo=false \
                    -Dplugin_ping=false \
                    -Dplugin_presenter=false \
                    -Dplugin_pulseaudio=false \
                    -Dplugin_runcommand=false \
                    -Dplugin_sftp=false \
                    -Dplugin_share=false \
                    -Dplugin_sms=false \
                    -Dplugin_systemvolume=false \
                    -Dplugin_telephony=false \
                    -Dplugin_xdp=false \
                    "${BUILDSUBDIR}"
    fi

    meson compile -C "${BUILDSUBDIR}" && \
    DESTDIR="${TARGET_DIR}" meson install -C "${BUILDSUBDIR}"
}

cx_analyze_abidiff() {
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

    cx_analyze_abidiff_build "${BASE_REF}" "${BASE_DIR}" > /dev/null 2>&1
    cx_analyze_abidiff_build "${HEAD_REF}" "${HEAD_DIR}" > /dev/null 2>&1

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

cx_analyze_cppcheck() {
    # Ensure a log directory exists where it is expected
    mkdir -p "${BUILDDIR}/meson-logs"

    if [ "${CX_CPPCHECK_LIBRARY}" != "" ]; then
        CX_CPPCHECK_OPTIONS="${CX_CPPCHECK_OPTIONS} --library=${CX_CPPCHECK_LIBRARY}"
    fi

    if [ "${CX_CPPCHECK_SUPPRESSIONS}" != "" ]; then
        CX_CPPCHECK_OPTIONS="${CX_CPPCHECK_OPTIONS} --suppressions-list=${CX_CPPCHECK_SUPPRESSIONS}"
    fi

    # shellcheck disable=SC2086
    cppcheck --error-exitcode=1 \
             --library=gtk \
             --quiet \
             --xml \
             ${CX_CPPCHECK_OPTIONS} \
             src 2> "${BUILDDIR}/meson-logs/cppcheck.xml" || \
    (cppcheck-htmlreport --file "${BUILDDIR}/meson-logs/cppcheck.xml" \
                         --report-dir "${BUILDDIR}/meson-logs/cppcheck-html" \
                         --source-dir "${WORKSPACE}" && exit 1);
}

cx_analyze_gcc() {
    export CC=gcc
    export CC_LD=bfd
    export CXX=g++
    export CXX_LD=bfd
    export CFLAGS="${CFLAGS} -fanalyzer"

    # shellcheck disable=SC2086
    meson setup --buildtype="${CX_SETUP_BUILDTYPE:=debug}" \
                ${CX_SETUP_OPTIONS} \
                "${BUILDDIR}" &&
    meson compile -C "${BUILDDIR}"
}

cx_analyze_llvm() {
    export CC=clang
    export CC_LD=lld
    export CXX=clang++
    export CXX_LD=lld
    export SCANBUILD="${WORKSPACE}/tests/extra/scanbuild.sh"

    # shellcheck disable=SC2086
    meson setup --buildtype="${CX_SETUP_BUILDTYPE:=debug}" \
                ${CX_SETUP_OPTIONS} \
                "${BUILDDIR}" &&
    ninja -C "${BUILDDIR}" scan-build
}

cx_analyze() {
    if [ "${CX_PROFILE}" = "abidiff" ]; then
        cx_analyze_abidiff
    elif [ "${CX_PROFILE}" = "cppcheck" ]; then
        cx_analyze_cppcheck
    elif [ "${CX_PROFILE}" = "gcc" ]; then
        cx_analyze_gcc
    elif [ "${CX_PROFILE}" = "llvm" ]; then
        cx_analyze_llvm
    else
        echo "Unknown profile: ${CX_PROFILE}";
        exit 1;
    fi
}


#
# test
#
cx_test_coverage() {
    if [ ! -d "${BUILDDIR}" ]; then
        echo "No build directory found at '${BUILDDIR}'"
        exit 1;
    fi

    # Capture, filter and generate
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

    # Filter out tests and subprojects
    # shellcheck disable=SC2086
    lcov --extract "${BUILDDIR}/meson-logs/coverage.info" \
         "${CX_LCOV_INCLUDE_PATH}" \
         --rc lcov_branch_coverage=1 \
         --output-file "${BUILDDIR}/meson-logs/coverage.info" && \
    lcov --remove "${BUILDDIR}/meson-logs/coverage.info" \
         "${CX_LCOV_EXCLUDE_PATH}" \
         --rc lcov_branch_coverage=1 \
         --output-file "${BUILDDIR}/meson-logs/coverage.info"

    # Generate HTML
    genhtml --prefix "${WORKSPACE}" \
            --output-directory "${BUILDDIR}/meson-logs/coverage-html" \
            --title 'Code Coverage' \
            --legend \
            --show-details \
            --branch-coverage \
            "${BUILDDIR}/meson-logs/coverage.info"
}

cx_test() {
    # Compiler Profiles
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

    # Instrumentation Profiles
    if [ "${CX_PROFILE}" = "asan" ]; then
        CX_SETUP_SANITIZE=address,undefined
        CX_TEST_REPEAT=1
        CX_TEST_TIMEOUT=3
    elif [ "${CX_PROFILE}" = "tsan" ]; then
        CX_SETUP_SANITIZE=thread
        CX_TEST_REPEAT=1
        CX_TEST_TIMEOUT=3
    fi

    # Clang needs `-Db_lundef=false` to use the sanitizers
    if [ "${CX_SETUP_SANITIZE:=none}" != "none" ] && [ "${CC}" = "clang" ]; then
        CX_SETUP_LUNDEF=false
    fi

    # Build
    if [ ! -d "${BUILDDIR}" ]; then
        # shellcheck disable=SC2086
        meson setup --buildtype="${CX_SETUP_BUILDTYPE:=debugoptimized}" \
                    -Db_coverage="${CX_SETUP_COVERAGE:=false}" \
                    -Db_lundef="${CX_SETUP_LUNDEF:=true}" \
                    -Db_sanitize="${CX_SETUP_SANITIZE:=none}" \
                    ${CX_SETUP_OPTIONS} \
                    "${BUILDDIR}"
    fi

    meson compile -C "${BUILDDIR}"

    # Test
    dbus-run-session \
    xvfb-run -d \
    meson test -C "${BUILDDIR}" \
               --print-errorlogs \
               --repeat="${CX_TEST_REPEAT:=1}" \
               --timeout-multiplier="${CX_TEST_TIMEOUT:=1}" \
               "${@}"

    # Coverage (Optional)
    if [ "${CX_SETUP_COVERAGE}" = "true" ]; then
        cx_test_coverage;
    fi
}


#
# The main function of the script, used to select a job and subjob to execute.
# $1: the job ID
# $2: the subjob ID
#
if [ "${1}" = "pre-test" ]; then
    shift;
    cx_pre_test;

elif [ "${1}" = "analyze" ]; then
    shift;
    cx_analyze "${@}";
elif [ "${1}" = "test" ]; then
    shift;
    cx_test "${@}";
elif [ "${1}" = "" ]; then
    echo "$(basename "${0}"): no command specified";
    exit 2;
else
    echo "$(basename "${0}"): unknown command '${1}'";
    exit 2;
fi

