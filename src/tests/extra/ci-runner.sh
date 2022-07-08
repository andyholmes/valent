#!/bin/sh -e

# SPDX-License-Identifier: CC0-1.0
# SPDX-FileCopyrightText: No rights reserved

# Fallbacks
WORKSPACE="${GITHUB_WORKSPACE:=$(git rev-parse --show-toplevel)}"
BUILDDIR="${BUILDDIR:=$WORKSPACE/_build}"


#
# CI/Static Analysis
#
ci_analyze_abidiff_build() {
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

ci_analyze_abidiff() {
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

    ci_analyze_abidiff_build "${BASE_REF}" "${BASE_DIR}" > /dev/null 2>&1
    ci_analyze_abidiff_build "${HEAD_REF}" "${HEAD_DIR}" > /dev/null 2>&1

    # Run `abidiff`
    mkdir -p "${BUILDDIR}/meson-logs"
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

ci_analyze_cppcheck() {
    # We're not building the project, so create logs where they would be
    mkdir -p "${BUILDDIR}/meson-logs"

    cppcheck --quiet \
             --error-exitcode=1 \
             -I"${WORKSPACE}/src/tests/fixtures" \
             --library=gtk \
             --library="${WORKSPACE}/src/tests/extra/cppcheck.cfg" \
             --suppressions-list="${WORKSPACE}/src/tests/extra/cppcheck.supp" \
             --xml \
             src 2> "${BUILDDIR}/meson-logs/cppcheck.xml" || \
    (cppcheck-htmlreport --file "${BUILDDIR}/meson-logs/cppcheck.xml" \
                         --report-dir "${BUILDDIR}/meson-logs/cppcheck-html" \
                         --source-dir "${WORKSPACE}" && exit 1);
}

ci_analyze_gcc() {
    export CC=gcc
    export CC_LD=bfd
    export CXX=g++
    export CXX_LD=bfd
    export CFLAGS="-fanalyzer"

    meson setup --buildtype=debug \
                -Dintrospection=false \
                -Dtests=true \
                "${BUILDDIR}" &&
    meson compile -C "${BUILDDIR}"
}

ci_analyze_llvm() {
    export CC=clang
    export CC_LD=lld
    export CXX=clang++
    export CXX_LD=lld
    export SCANBUILD="${WORKSPACE}/src/tests/extra/scanbuild.sh"

    meson setup --buildtype=debug \
                -Dintrospection=false \
                -Dtests=true \
                "${BUILDDIR}" &&
    ninja -C "${BUILDDIR}" scan-build
}

ci_analyze() {
    ANALYZER_PROFILE="${ANALYZER_PROFILE:=$1}"

    if [ "${ANALYZER_PROFILE}" = "abidiff" ]; then
        ci_analyze_abidiff
    elif [ "${ANALYZER_PROFILE}" = "cppcheck" ]; then
        ci_analyze_cppcheck
    elif [ "${ANALYZER_PROFILE}" = "gcc" ]; then
        ci_analyze_gcc
    elif [ "${ANALYZER_PROFILE}" = "llvm" ]; then
        ci_analyze_llvm
    else
        echo "Unknown profile: ${ANALYZER_PROFILE}";
        exit 1;
    fi
}


#
# CI/Tests
#
ci_test_coverage() {
    # If we're passed arguments, re-run the `ci_test()` function
    if [ $# -gt 0 ]; then
        ci_test gcc "${@}";

    # If the build directory doesn't exist, we need to run the tests
    elif [ ! -d "${BUILDDIR}" ]; then
        ci_test gcc;
    fi

    # Capture, filter and generate
    lcov --directory "${BUILDDIR}" \
         --capture \
         --initial \
         --output-file "${BUILDDIR}"/meson-logs/coverage.p1 && \
    lcov --directory "${BUILDDIR}" \
         --capture \
         --no-checksum \
         --rc lcov_branch_coverage=1 \
         --output-file "${BUILDDIR}"/meson-logs/coverage.p2 && \
    lcov --add-tracefile "${BUILDDIR}"/meson-logs/coverage.p1 \
         --add-tracefile "${BUILDDIR}"/meson-logs/coverage.p2 \
         --rc lcov_branch_coverage=1 \
         --output-file "${BUILDDIR}"/meson-logs/coverage.info

    # Filter out tests and subprojects
    lcov --extract "${BUILDDIR}"/meson-logs/coverage.info \
         "${WORKSPACE}/src/*" \
         --rc lcov_branch_coverage=1 \
         --output-file "${BUILDDIR}"/meson-logs/coverage.info && \
    lcov --remove "${BUILDDIR}"/meson-logs/coverage.info \
         '*/src/tests/*' \
         '*/subprojects/*' \
         --rc lcov_branch_coverage=1 \
         --output-file "${BUILDDIR}"/meson-logs/coverage.info

    # Generate HTML
    genhtml --prefix "${WORKSPACE}" \
            --output-directory "${BUILDDIR}"/meson-logs/coverage-html \
            --title 'Code Coverage' \
            --legend \
            --show-details \
            --branch-coverage \
            "${BUILDDIR}"/meson-logs/coverage.info
}

ci_test() {
    TEST_PROFILE="${TEST_PROFILE:=$1}"
    shift;

    # Generate Coverage
    if [ "${TEST_PROFILE}" = "coverage" ]; then
        ci_test_coverage "${@}"
        return;
    fi

    # Compiler Toolchain
    if [ "${TEST_PROFILE}" = "llvm" ]; then
        export CC=clang
        export CC_LD=lld
        export CXX=clang++
        export CXX_LD=lld
    else
        export CC=gcc
        export CC_LD=bfd
        export CXX=g++
        export CXX_LD=bfd
    fi

    # Meson Options
    if [ "${TEST_PROFILE}" = "gcc" ]; then
        BUILD_COVERAGE=true
    elif [ "${TEST_PROFILE}" = "asan" ]; then
        BUILD_SANITIZE=address,undefined
        BUILD_INTROSPECTION=false
        TEST_REPEAT=1
        TEST_TIMEOUT=3
    elif [ "${TEST_PROFILE}" = "tsan" ]; then
        BUILD_SANITIZE=thread
        BUILD_INTROSPECTION=false
        TEST_REPEAT=1
        TEST_TIMEOUT=3
    fi

    # NOTE: Local Overrides
    if [ "${GITHUB_ACTIONS}" != "true" ]; then
        BUILD_FUZZING=false
        TEST_REPEAT=1
    fi

    # Build
    if [ ! -d "${BUILDDIR}" ]; then
        meson setup --buildtype="${BUILD_TYPE:=debugoptimized}" \
                    -Db_coverage="${BUILD_COVERAGE:=false}" \
                    -Db_sanitize="${BUILD_SANITIZE:=none}" \
                    -Dfuzz_tests="${BUILD_FUZZING:=true}" \
                    -Dintrospection="${BUILD_INTROSPECTION:=true}" \
                    -Dtests=true \
                    "${BUILDDIR}"
    fi

    meson compile -C "${BUILDDIR}"

    # Test
    dbus-run-session \
    xvfb-run -d \
    meson test -C "${BUILDDIR}" \
               --print-errorlogs \
               --repeat="${TEST_REPEAT:=3}" \
               --timeout-multiplier="${TEST_TIMEOUT:=1}" \
               "${@}"
}


#
# The main function of the script, used to select a job and subjob to execute.
# $1: the job ID
# $2: the subjob ID
#
if [ "${1}" = "analyze" ]; then
    shift;
    ci_analyze "${@}";
elif [ "${1}" = "test" ]; then
    shift;
    ci_test "${@}";
else
    echo "Unknown command: ${*}";
    exit 1;
fi

