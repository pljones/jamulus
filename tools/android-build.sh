#!/bin/bash
##############################################################################
# Copyright (c) 2026
#
# Author(s):
#  Peter L Jones <peter.l.jones.dymlyd@gmail.com>
#  The Jamulus Development Team
#
##############################################################################
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU Affero General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Affero General Public License for more details.
#
# You should have received a copy of the GNU Affero General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.
#
##############################################################################

# Apply workflow-style Android environment, optional local overrides, then run
# .github/autobuild/android.sh so build and get-artifacts share the same settings.

set -euo pipefail

SCRIPT_NAME="$(basename "$0")"
readonly SCRIPT_NAME
PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
readonly PROJECT_DIR
readonly ANDROID_SH="${PROJECT_DIR}/.github/autobuild/android.sh"
readonly ANDROID_DEPENDENCIES="${PROJECT_DIR}/.github/autobuild/android-dependencies.sh"
readonly GET_BUILD_VARS="${PROJECT_DIR}/.github/autobuild/get_build_vars.py"
readonly DEFAULT_SETTINGS_FILE="${PROJECT_DIR}/android-build.settings"

# shellcheck disable=SC1090
source "$ANDROID_DEPENDENCIES"

MANAGED_VARS=(
    JAVA_HOME
    ANDROID_SDK_ROOT
    ANDROID_NDK_ROOT
    ANDROID_BUILD_TOOLS_REVISION
    ANDROID_DEPLOYMENT_PLATFORM
    QT_SELECT
    QT_ANDROID_DIR
    QT_BASEDIR
    QT_VERSION
    JAMULUS_BUILD_VERSION
    JAMULUS_ANDROID_SDK_ROOT
    JAMULUS_ANDROID_NDK_ROOT
    JAMULUS_ANDROID_BUILD_MODES
    JAMULUS_ANDROID_ABIS
    JAMULUS_ANDROID_QMAKE_CONFIG
    JAMULUS_ANDROID_PUBLISH
    JAMULUS_ANDROID_KEYSTORE
    JAMULUS_ANDROID_KEYSTORE_BASE64
    JAMULUS_ANDROID_KEY_ALIAS
    JAMULUS_ANDROID_KEYSTORE_PASSWORD
    JAMULUS_ANDROID_KEY_PASSWORD
    JAMULUS_ANDROID_DEBUG_KEYSTORE
    JAMULUS_ANDROID_VERSION_CODE
    JAMULUS_ANDROID_BUILD_DIR
    JAMULUS_ANDROID_DEPLOY_DIR
)
declare -gA SKIP_ORIGINAL_ENV
for k in "${MANAGED_VARS[@]}"; do
    SKIP_ORIGINAL_ENV["$k"]=1
done

error() {
    printf '%s: %s\n' "$SCRIPT_NAME" "$1" >&2
    exit 1
}

is_secret_var() {
    case "$1" in
        *PASSWORD* | *KEYSTORE_BASE64*)
            return 0
            ;;
        *)
            return 1
            ;;
    esac
}

snapshot_original_env() {
    local var
    declare -gA ORIGINAL_ENV=()
    for var in "${MANAGED_VARS[@]}"; do
        if [[ -n "${!var+x}" ]]; then
            ORIGINAL_ENV["$var"]="${!var}"
        fi
    done
}

apply_local_defaults() {
    # Local developer layout from COMPILING.md. CI uses different /opt/android
    # paths after android.sh setup.
    : "${JAVA_HOME:=/usr/lib/jvm/java-11-openjdk-amd64}"
    : "${ANDROID_SDK_ROOT:=/opt/android-sdk}"
    : "${ANDROID_NDK_ROOT:=${ANDROID_SDK_ROOT}/ndk/21.0.6113669}"
    : "${ANDROID_BUILD_TOOLS_REVISION:=${ANDROID_BUILD_TOOLS}}"
    : "${QT_SELECT:=${QT_VERSION}-android}"
    : "${QT_ANDROID_DIR:=/opt/Qt/${QT_VERSION}/android}"
    : "${ANDROID_DEPLOYMENT_PLATFORM:=${ANDROID_PLATFORM}}"
    : "${JAMULUS_ANDROID_BUILD_MODES:=debug release}"
    : "${JAMULUS_ANDROID_ABIS:=armeabi-v7a arm64-v8a x86 x86_64}"
}

load_settings_file() {
    local settings_file="$1"

    [[ -n "$settings_file" ]] || return 0
    [[ -f "$settings_file" ]] || error "settings file not found: $settings_file"
    [[ -r "$settings_file" ]] || error "settings file is not readable: $settings_file"
    printf 'Using settings file %s\n' "$settings_file"
    set -a
    # Developer-supplied KEY=value file; not shipped in the repository.
    # shellcheck disable=SC1090
    source "$settings_file"
    set +a
}

restore_original_env() {
    local var
    for var in "${!ORIGINAL_ENV[@]}"; do
        # Restore original value of $var unless it was overridden by a settings file
        [[ -v SKIP_ORIGINAL_ENV["$var"] && -n "${var}" ]] && continue
        printf -v "$var" '%s' "${ORIGINAL_ENV[$var]}"
    done
}

export_managed_env() {
    local var
    for var in "${MANAGED_VARS[@]}"; do
        if [[ -n "${!var+x}" ]]; then
            # Export variable named $var
            # shellcheck disable=SC2163
            export "$var"
        fi
    done
}

ensure_java_on_path() {
    if [[ -n "${JAVA_HOME:-}" && -d "${JAVA_HOME}/bin" ]]; then
        case ":${PATH}:" in
            *":${JAVA_HOME}/bin:"*) ;;
            *)
                PATH="${JAVA_HOME}/bin:${PATH}"
                ;;
        esac
        export PATH
    fi
}

ensure_build_version() {
    if [[ -z "${JAMULUS_BUILD_VERSION:-}" ]]; then
        [[ -x "$GET_BUILD_VARS" || -f "$GET_BUILD_VARS" ]] ||
            error "cannot find $GET_BUILD_VARS"
        JAMULUS_BUILD_VERSION="$(python3 "$GET_BUILD_VARS" --print-build-version)"
        export JAMULUS_BUILD_VERSION
    fi
    [[ ${JAMULUS_BUILD_VERSION} =~ [0-9]+\.[0-9]+\.[0-9]+ ]] ||
        error "JAMULUS_BUILD_VERSION is not a valid version string: ${JAMULUS_BUILD_VERSION}"
}

print_resolved_env() {
    local var
    printf 'Resolved Android build environment:\n'
    for var in "${MANAGED_VARS[@]}"; do
        if [[ -z "${!var+x}" ]]; then
            continue
        fi
        if is_secret_var "$var"; then
            printf '  %s=<set>\n' "$var"
        else
            printf '  %s=%s\n' "$var" "${!var}"
        fi
    done
}

run_logged() {
    if [[ -n "${LOG_FILE:-}" ]]; then
        "$@" 2>&1 | tee -a "$LOG_FILE"
        return "${PIPESTATUS[0]}"
    fi
    "$@"
}

run_distclean() {
    if [[ -f "${PROJECT_DIR}/Makefile" ]]; then
        run_logged make -C "$PROJECT_DIR" distclean || :
    fi
    run_logged "$ANDROID_SH" distclean || :
}

run_stage() {
    local stage="$1"
    case "$stage" in
        setup | build | get-artifacts)
            run_logged "$ANDROID_SH" "$stage"
            ;;
        distclean)
            run_distclean
            ;;
        *)
            error "unknown stage: $stage"
            ;;
    esac
}

resolve_settings_file() {
    local requested="$1"
    if [[ -n "$requested" ]]; then
        printf '%s' "$requested"
        return
    fi
    if [[ -n "${JAMULUS_ANDROID_SETTINGS:-}" ]]; then
        printf '%s' "$JAMULUS_ANDROID_SETTINGS"
        return
    fi
    if [[ -f "$DEFAULT_SETTINGS_FILE" ]]; then
        printf '%s' "$DEFAULT_SETTINGS_FILE"
    fi
}

usage() {
    cat << EOF
Usage: $SCRIPT_NAME [options] [stage ...]

Load local Android toolchain defaults (as in COMPILING.md), apply optional
overrides from a settings file, then run .github/autobuild/android.sh with the
same environment for every stage. JAMULUS_BUILD_VERSION is taken from
Jamulus.pro (plus the git hash for development versions) unless already set.

Stages:
  setup          Install CI-style SDK/NDK/Qt under /opt (rarely needed locally)
  distclean      make distclean (if a Makefile exists) and android.sh distclean
  build          Build selected debug/release packages
  get-artifacts  Rename packages into deploy/
  all            distclean, build, and get-artifacts (default)

Options:
  -h, --help            Show this help
  --settings FILE       Override file (default: android-build.settings if present)
  --log FILE            Also write combined stdout/stderr to FILE
  --print-env           Print the resolved environment and exit
  --build-modes "MODES" Override JAMULUS_ANDROID_BUILD_MODES (default: "debug release")
  --abis "ABIS"         Override JAMULUS_ANDROID_ABIS (default: "armeabi-v7a arm64-v8a x86 x86_64")

Precedence: settings file > existing environment > documented local defaults.
See tools/android-build.settings.example.
EOF
}

SETTINGS_FILE=""
LOG_FILE=""
PRINT_ENV=0
STAGES=()
BUILD_MODES=""
ABIS=""

while [[ $# -gt 0 ]]; do
    case "$1" in
        -h | --help)
            usage
            exit 0
            ;;
        --settings)
            [[ $# -ge 2 ]] || error "missing value for --settings"
            SETTINGS_FILE="$2"
            shift 2
            ;;
        --log)
            [[ $# -ge 2 ]] || error "missing value for --log"
            LOG_FILE="$2"
            shift 2
            ;;
        --print-env)
            PRINT_ENV=1
            shift
            ;;
        --build-modes)
            [[ $# -ge 2 ]] || error "missing value for --build-mode"
            BUILD_MODES="$2"
            shift 2
            ;;
        --abis)
            [[ $# -ge 2 ]] || error "missing value for --abis"
            ABIS="$2"
            shift 2
            ;;
        all)
            STAGES+=(distclean build get-artifacts)
            shift
            ;;
        setup | distclean | build | get-artifacts)
            STAGES+=("$1")
            shift
            ;;
        *)
            error "unknown argument: $1"
            ;;
    esac
done

if [[ ${#STAGES[@]} -eq 0 && "$PRINT_ENV" -eq 0 ]]; then
    STAGES=(distclean build get-artifacts)
fi

cd "$PROJECT_DIR"
[[ -x "$ANDROID_SH" || -f "$ANDROID_SH" ]] || error "cannot find $ANDROID_SH"

snapshot_original_env
load_settings_file "$(resolve_settings_file "$SETTINGS_FILE")"
[[ -n "$BUILD_MODES" ]] && export JAMULUS_ANDROID_BUILD_MODES="$BUILD_MODES"
[[ -n "$ABIS" ]] && export JAMULUS_ANDROID_ABIS="$ABIS"
restore_original_env
apply_local_defaults
export_managed_env
ensure_java_on_path
ensure_build_version
print_resolved_env

if [[ "$PRINT_ENV" -eq 1 ]]; then
    exit 0
fi

if [[ -n "$LOG_FILE" ]]; then
    mkdir -p "$(dirname "$LOG_FILE")"
    : > "$LOG_FILE"
    printf 'Logging to %s\n' "$LOG_FILE"
fi

for stage in "${STAGES[@]}"; do
    printf '=== %s ===\n' "$stage"
    run_stage "$stage"
done
