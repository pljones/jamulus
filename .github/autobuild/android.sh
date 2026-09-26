#!/bin/bash
##############################################################################
# Copyright (c) 2022-2026
#
# Author(s):
#  Christian Hoffmann
#  The Jamulus Development Team
#
# As of Jamulus 3.12.1dev (commit eb172d47): All new source code contributions must be licensed
# under AGPL 3.0 or any later version.
#
# Existing code: Code contributed before 3.12.1dev (commit eb172d47) was licensed under GPL 2.0+.
# This code will be licensed under GPL 3.0 (or any later version) from
# 3.12.1dev (commit eb172d47).  When distributed as part of Jamulus, the AGPL 3.0 terms govern
# the combined work, including network use provisions.
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
# ---------------------------------------------------------------------------
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.
#
##############################################################################

set -eu

set -o pipefail

if [[ ! ${JAMULUS_BUILD_VERSION:-} =~ [0-9]+\.[0-9]+\.[0-9]+ ]]; then
    echo "Environment variable JAMULUS_BUILD_VERSION has to be set to a valid version string"
    exit 1
fi

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
readonly PROJECT_DIR

readonly ANDROID_STREAM="${JAMULUS_ANDROID_STREAM:-qt5}"
readonly PUBLISH_MODE="${JAMULUS_ANDROID_PUBLISH:-}"
case "$ANDROID_STREAM" in
    qt5 | legacy)
        ANDROID_DEPENDENCIES="${PROJECT_DIR}/.github/autobuild/android-dependencies_qt5.sh"
        DEFAULT_ARTIFACT_SUFFIX="_qt5"
        DEFAULT_PACKAGE_FORMAT="apk"
        ;;
    qt6 | play-store)
        ANDROID_DEPENDENCIES="${PROJECT_DIR}/.github/autobuild/android-dependencies_qt6.sh"
        DEFAULT_ARTIFACT_SUFFIX="_qt6"
        DEFAULT_PACKAGE_FORMAT="apk"
        ;;
    *)
        echo "Unsupported Android build stream: $ANDROID_STREAM" >&2
        exit 1
        ;;
esac

# Save environment before sourcing the dependencies file, so we can restore overrides.
EXPLICIT_COMMANDLINETOOLS_VERSION="${COMMANDLINETOOLS_VERSION:-}"
EXPLICIT_ANDROID_NDK_VERSION="${ANDROID_NDK_VERSION:-}"
EXPLICIT_ANDROID_PLATFORM="${ANDROID_PLATFORM:-}"
EXPLICIT_ANDROID_BUILD_TOOLS="${ANDROID_BUILD_TOOLS:-}"
EXPLICIT_AQTINSTALL_VERSION="${AQTINSTALL_VERSION:-}"
EXPLICIT_ANDROID_JAVA_VERSION="${ANDROID_JAVA_VERSION:-}"
EXPLICIT_QT_VERSION="${QT_VERSION:-}"

# shellcheck disable=SC1090
source "$ANDROID_DEPENDENCIES"

# Restore any explicit overrides from the environment, so they take precedence over the defaults in the dependencies file.
[[ -n "${EXPLICIT_COMMANDLINETOOLS_VERSION}" ]] && COMMANDLINETOOLS_VERSION="${EXPLICIT_COMMANDLINETOOLS_VERSION}"
[[ -n "${EXPLICIT_ANDROID_NDK_VERSION}" ]] && ANDROID_NDK_VERSION="${EXPLICIT_ANDROID_NDK_VERSION}"
[[ -n "${EXPLICIT_ANDROID_PLATFORM}" ]] && ANDROID_PLATFORM="${EXPLICIT_ANDROID_PLATFORM}"
[[ -n "${EXPLICIT_ANDROID_BUILD_TOOLS}" ]] && ANDROID_BUILD_TOOLS="${EXPLICIT_ANDROID_BUILD_TOOLS}"
[[ -n "${EXPLICIT_AQTINSTALL_VERSION}" ]] && AQTINSTALL_VERSION="${EXPLICIT_AQTINSTALL_VERSION}"
[[ -n "${EXPLICIT_ANDROID_JAVA_VERSION}" ]] && ANDROID_JAVA_VERSION="${EXPLICIT_ANDROID_JAVA_VERSION}"
[[ -n "${EXPLICIT_QT_VERSION}" ]] && QT_VERSION="${EXPLICIT_QT_VERSION}"

# Defaults must match the cache values in .github/workflows/autobuild.yml
export ANDROID_SDK_ROOT="${ANDROID_SDK_ROOT:-${HOME}/android-sdk}"
export ANDROID_NDK_ROOT="${ANDROID_NDK_ROOT:-${HOME}/android-ndk}"
QT_DIR="${QT_DIR:-${HOME}/qt}"

readonly TARGET_ARCHS="${TARGET_ARCHS:-armeabi-v7a arm64-v8a x86 x86_64}"

# Ensure tools know where things are
export ANDROID_HOME="${ANDROID_SDK_ROOT}"
export ANDROID_NDK="${ANDROID_NDK_ROOT}"
export ANDROID_NDK_HOME="${ANDROID_NDK_ROOT}"
export ANDROID_NDK_LATEST_HOME="${ANDROID_NDK_ROOT}"
export ANDROID_SDK_BUILD_TOOLS_REVISION="${ANDROID_BUILD_TOOLS}"
export ANDROID_BUILD_TOOLS_REVISION="${ANDROID_BUILD_TOOLS}"

qt_android_arch() {
    case "$1" in
        armeabi-v7a)
            printf '%s' android_armv7
            ;;
        arm64-v8a)
            printf '%s' android_arm64_v8a
            ;;
        x86 | x86_64)
            printf '%s' "android_$1"
            ;;
        *)
            echo "Unsupported Android ABI: $1" >&2
            exit 1
            ;;
    esac
}

if [[ ! ${QT_VERSION:-} =~ [0-9]+\.[0-9]+\..* ]]; then
    echo "Environment variable QT_VERSION must be set to a valid Qt version"
    exit 1
fi

if [[ "${QT_VERSION}" =~ 5\..* ]]; then
    QT_ANDROID_DIR="${QT_DIR}/${QT_VERSION}/android"
    QT_ANDROIDDEPLOYQT="${QT_ANDROID_DIR}/bin/androiddeployqt"
else
    read -r -a target_archs <<< "${TARGET_ARCHS}"
    QT_ANDROID_DIR="${QT_DIR}/${QT_VERSION}/$(qt_android_arch "${target_archs[0]}")"
    QT_ANDROIDDEPLOYQT="${QT_DIR}/${QT_VERSION}/gcc_64/bin/androiddeployqt"
fi
readonly QT_ANDROID_DIR
readonly QT_ANDROIDDEPLOYQT

export QT_SELECT
QT_SELECT="${QT_VERSION}-$(basename "${QT_ANDROID_DIR}")"
export QTTOOLDIR="${QT_ANDROID_DIR}/bin"
export QTLIBDIR="${QT_ANDROID_DIR}/lib"

# Only variables which are really needed by sub-commands are exported.
# Definitions have to stay in a specific order due to dependencies.
export PATH="${PATH}:${ANDROID_SDK_ROOT}/tools"
export PATH="${PATH}:${ANDROID_SDK_ROOT}/platform-tools"
export JAVA_HOME="${JAVA_HOME:-/usr/lib/jvm/java-${ANDROID_JAVA_VERSION:-8}-openjdk-amd64/}"

env | grep -E '^(ANDROID_BUILD_TOOLS_REVISION|ANDROID_HOME|ANDROID_NDK|ANDROID_NDK_HOME|ANDROID_NDK_LATEST_HOME|ANDROID_NDK_ROOT|ANDROID_SDK_BUILD_TOOLS_REVISION|ANDROID_SDK_ROOT|DEBIAN_FRONTEND|JAMULUS_BUILD_VERSION|JAVA_HOME|PATH|QTLIBDIR|QT_SELECT|QTTOOLDIR)=' | sort

readonly QT_QMAKE="${QT_ANDROID_DIR}/bin/qmake"

readonly BUILD_ROOT="${JAMULUS_ANDROID_BUILD_DIR:-${PROJECT_DIR}/android-build}"
readonly DEPLOY_DIR="${JAMULUS_ANDROID_DEPLOY_DIR:-${PROJECT_DIR}/deploy}"
readonly BUILD_MODES="${BUILD_MODES:-debug release}"
readonly QMAKE_CONFIG="${JAMULUS_ANDROID_QMAKE_CONFIG:-}"
MAX_MAKE_JOBS="$(nproc)"
readonly MAX_MAKE_JOBS
readonly MAKE_JOBS="${JAMULUS_ANDROID_MAKE_JOBS:-${MAX_MAKE_JOBS}}"
readonly ARTIFACT_SUFFIX="${JAMULUS_ANDROID_ARTIFACT_SUFFIX:-${DEFAULT_ARTIFACT_SUFFIX}}"
readonly PACKAGE_FORMAT="${JAMULUS_ANDROID_PACKAGE_FORMAT:-${DEFAULT_PACKAGE_FORMAT}}"

if [[ ! "$MAKE_JOBS" =~ ^[1-9][0-9]*$ ]] || ((MAKE_JOBS > MAX_MAKE_JOBS)); then
    echo "JAMULUS_ANDROID_MAKE_JOBS must be a positive integer no greater than ${MAX_MAKE_JOBS}" >&2
    exit 1
fi

readonly JAMULUS_ANDROID_KEYSTORE="${JAMULUS_ANDROID_KEYSTORE:-}"
readonly JAMULUS_ANDROID_KEYSTORE_BASE64="${JAMULUS_ANDROID_KEYSTORE_BASE64:-}"
readonly JAMULUS_ANDROID_KEY_ALIAS="${JAMULUS_ANDROID_KEY_ALIAS:-}"
readonly JAMULUS_ANDROID_KEYSTORE_PASSWORD="${JAMULUS_ANDROID_KEYSTORE_PASSWORD:-}"
readonly JAMULUS_ANDROID_KEY_PASSWORD="${JAMULUS_ANDROID_KEY_PASSWORD:-}"

readonly COMMANDLINETOOLS_DIR="${ANDROID_SDK_ROOT}"/cmdline-tools/latest
readonly ANDROID_SDKMANAGER=("${COMMANDLINETOOLS_DIR}/bin/sdkmanager" "--sdk_root=${ANDROID_SDK_ROOT}")

readonly ANDROID_NDK_HOST="${ANDROID_NDK_HOST:-linux-x86_64}"
readonly ANDROID_NDK_MAKE=${ANDROID_NDK_ROOT}/prebuilt/${ANDROID_NDK_HOST}/bin/make

env | grep -E '^(ANDROID_BUILD_TOOLS_REVISION|ANDROID_HOME|ANDROID_NDK|ANDROID_NDK_HOME|ANDROID_NDK_LATEST_HOME|ANDROID_NDK_ROOT|ANDROID_SDK_BUILD_TOOLS_REVISION|ANDROID_SDK_ROOT|DEBIAN_FRONTEND|JAMULUS_BUILD_VERSION|JAVA_HOME|PATH|QTLIBDIR|QT_SELECT|QTTOOLDIR)=' | sort
env | grep -E '^(JAMULUS_ANDROID_KEY_ALIAS|JAMULUS_ANDROID_KEY_PASSWORD|JAMULUS_ANDROID_KEYSTORE|JAMULUS_ANDROID_KEYSTORE_BASE64|JAMULUS_ANDROID_KEYSTORE_PASSWORD)=' | sort | sed -e 's/=\(.*\)/=*****/'

setup_ubuntu_dependencies() {
    export DEBIAN_FRONTEND="noninteractive"

    sudo apt-get -qq update
    sudo apt-get -qq --no-install-recommends -y install build-essential zip unzip bzip2 p7zip-full curl chrpath "openjdk-${ANDROID_JAVA_VERSION:-8}-jdk-headless"
}

setup_android_sdk() {
    # We may need to create the cmdline_tools directory and chown it to the runner user to fix permissions
    # (even after cache recovery)
    sudo mkdir -p "${COMMANDLINETOOLS_DIR}"
    sudo chown -R "$(whoami)" "${ANDROID_SDK_ROOT}"

    # sdkmanager doesn't record which cmdline-tools build we downloaded, so track that
    # ourselves; platforms/build-tools are already installed under a directory named
    # after the exact requested version, so no extra marker is needed for those.
    local cmdlinetools_version_marker="${COMMANDLINETOOLS_DIR}/.jamulus-cmdlinetools-version"

    if [[ -d "${COMMANDLINETOOLS_DIR}" && -x "${ANDROID_SDKMANAGER[0]}" &&
        -f "$cmdlinetools_version_marker" &&
        "$(cat "$cmdlinetools_version_marker")" == "${COMMANDLINETOOLS_VERSION}" &&
        -d "${ANDROID_SDK_ROOT}/platforms/${ANDROID_PLATFORM}" &&
        -d "${ANDROID_SDK_ROOT}/build-tools/${ANDROID_BUILD_TOOLS}" ]]; then
        echo "Using SDK installation (cmdline-tools: ${COMMANDLINETOOLS_VERSION}, platforms: ${ANDROID_PLATFORM}, build-tools: ${ANDROID_BUILD_TOOLS}) from previous run (actions/cache)"
        return
    fi

    echo "Installing SDK (cmdline-tools: ${COMMANDLINETOOLS_VERSION}, platforms: ${ANDROID_PLATFORM}, build-tools: ${ANDROID_BUILD_TOOLS})"
    # Clear any mismatched-version leftovers so files from different cmdline-tools builds can't mix.
    find "${COMMANDLINETOOLS_DIR:?}" -mindepth 1 -delete
    pushd "${COMMANDLINETOOLS_DIR}" > /dev/null

    curl -s -o downloadfile.zip "https://dl.google.com/android/repository/commandlinetools-linux-${COMMANDLINETOOLS_VERSION}_latest.zip"
    unzip -q downloadfile.zip -d sdk-tmpdir
    rm -f downloadfile.zip

    mv sdk-tmpdir/cmdline-tools/* .
    rm -rf sdk-tmpdir

    popd > /dev/null
    echo "${COMMANDLINETOOLS_VERSION}" > "$cmdlinetools_version_marker"

    set +o pipefail
    yes | "${ANDROID_SDKMANAGER[@]}" --licenses
    "${ANDROID_SDKMANAGER[@]}" --update
    yes | "${ANDROID_SDKMANAGER[@]}" "platforms;${ANDROID_PLATFORM}"
    yes | "${ANDROID_SDKMANAGER[@]}" "build-tools;${ANDROID_BUILD_TOOLS}"
    yes | "${ANDROID_SDKMANAGER[@]}" --licenses
    echo =====================
    echo "Android SDK licenses under ${ANDROID_SDK_ROOT}:"
    find "${ANDROID_SDK_ROOT}" -type d -name licenses -prune -exec ls -ltrR '{}' \;
    echo =====================
    set -o pipefail
}

setup_android_ndk() {
    # We may need to create the NDK installation directory and chown it to the runner user to fix permissions
    # (even after cache recovery)
    sudo mkdir -p "${ANDROID_NDK_ROOT}"
    sudo chown -R "$(whoami)" "${ANDROID_NDK_ROOT}"

    # The NDK is unpacked flat into ANDROID_NDK_ROOT, so its directory layout
    # doesn't encode the version; track it ourselves to validate cache hits.
    local ndk_version_marker="${ANDROID_NDK_ROOT}/.jamulus-ndk-version"

    if [[ -f "${ANDROID_NDK_ROOT}/source.properties" && -x "${ANDROID_NDK_MAKE}" &&
        -f "$ndk_version_marker" && "$(cat "$ndk_version_marker")" == "${ANDROID_NDK_VERSION}" ]]; then
        echo "Using NDK ${ANDROID_NDK_VERSION} installation from previous run (actions/cache)"
        return
    fi

    echo "Installing NDK ${ANDROID_NDK_VERSION}"
    # Remove any mismatched-version leftovers so files from different NDK releases can't mix.
    find "${ANDROID_NDK_ROOT:?}" -mindepth 1 -delete
    pushd "${ANDROID_NDK_ROOT}" > /dev/null

    # Ugh... Should be optimised...
    if [[ "${ANDROID_NDK_VERSION}" == "$( ( echo "r22b"; echo "${ANDROID_NDK_VERSION}" ) | sort | head -1)" ]]; then
        suffix="-x86_64"
    else
        suffix=""
    fi
    curl -s -o downloadfile.zip "https://dl.google.com/android/repository/android-ndk-${ANDROID_NDK_VERSION}-linux${suffix}.zip"
    unzip -q downloadfile.zip -d ndk-tmpdir
    rm -f downloadfile.zip

    mv ndk-tmpdir/"android-ndk-${ANDROID_NDK_VERSION}"/* .
    rm -rf ndk-tmpdir

    popd > /dev/null
    echo "${ANDROID_NDK_VERSION}" > "$ndk_version_marker"
}

setup_qt() {
    local qt_arch

    # We may need to create the Qt installation directory and chown it to the runner user to fix permissions
    # (even after cache recovery)
    sudo mkdir -p "${QT_DIR}"
    sudo chown -R "$(whoami)" "${QT_DIR}"

    # QT_QMAKE/QT_ANDROIDDEPLOYQT/per-arch qmake paths are all under a
    # ${QT_VERSION}-named directory, so their presence already confirms the
    # expected Qt version (and, for Qt6, each requested target arch) is installed.
    if [[ -x "${QT_QMAKE}" && -x "${QT_ANDROIDDEPLOYQT}" ]]; then
        if [[ "${QT_VERSION}" =~ 5\..* ]]; then
            echo "Using Qt ${QT_VERSION} installation from previous run (actions/cache)"
            return
        fi
        read -r -a target_archs <<< "${TARGET_ARCHS}"
        for target_arch in "${target_archs[@]}"; do
            qt_arch="$(qt_android_arch "$target_arch")"
            [[ -x "${QT_DIR}/${QT_VERSION}/${qt_arch}/bin/qmake" ]] || break
        done
        [[ -x "${QT_DIR}/${QT_VERSION}/${qt_arch}/bin/qmake" ]] || qt_arch=""
        if [[ -n "$qt_arch" ]]; then
            echo "Using Qt ${QT_VERSION} installation from previous run (actions/cache)"
            return
        fi
    fi

    echo "Installing Qt"
    # Remove any mismatched-version leftovers so files from different NDK releases can't mix.
    find "${QT_DIR:?}/${QT_VERSION}" -mindepth 1 -delete

    # Create and enter virtual environment
    python3 -m venv venv
    # Must hide directory as it just gets created during execution of the previous command and cannot be found by shellcheck
    # shellcheck source=/dev/null
    source venv/bin/activate

    pip install "aqtinstall==${AQTINSTALL_VERSION}"

    # Install actual Android Qt:
    local qt_archives=(qtbase qttools qttranslations)
    local qtmultimedia=()
    if [[ "${QT_VERSION}" =~ 5\..* ]]; then
        qt_archives+=(qtandroidextras qtmultimedia)
        python3 -m aqt install-qt --outputdir "${QT_DIR}" linux android "${QT_VERSION}" \
            --archives "${qt_archives[@]}"
    else
        # From Qt6 onwards, qtmultimedia is a module and cannot be installed
        # as an archive anymore.
        qtmultimedia=("--modules" qtmultimedia)
        qt_archives+=(icu)
        for target_arch in $TARGET_ARCHS; do
            qt_arch="$(qt_android_arch "$target_arch")"
            python3 -m aqt install-qt --outputdir "${QT_DIR}" linux android "${QT_VERSION}" "$qt_arch" \
                --autodesktop --archives "${qt_archives[@]}" "${qtmultimedia[@]}"
        done
    fi

    # Delete libraries, which we don't use, but which bloat the resulting package and might introduce unwanted dependencies.
    # iOS does not do this - leave for now, fix across all later.
    for target_arch in $TARGET_ARCHS; do
        if [[ "${QT_VERSION}" =~ 5\..* ]]; then
            qt_arch="$QT_ANDROID_DIR"
        else
            qt_arch="${QT_DIR}/${QT_VERSION}/$(qt_android_arch "$target_arch")"
        fi
        find "$qt_arch" -name 'libQt*Quick*.so' -delete
        rm -rf "$qt_arch/qml/"
    done

    # deactivate and remove venv as aqt is no longer needed from here on
    deactivate
    rm -rf venv
}

validate_build_mode() {
    local build_mode

    [[ -n "$BUILD_MODES" ]] || {
        echo "BUILD_MODES must select debug and/or release" >&2
        exit 1
    }
    for build_mode in $BUILD_MODES; do
        [[ "$build_mode" == debug || "$build_mode" == release ]] || {
            echo "Unsupported Android build mode: $build_mode" >&2
            exit 1
        }
    done
    [[ "$PUBLISH_MODE" == "" || "$PUBLISH_MODE" == play-store ]] || {
        echo "Unsupported Android publication mode: $PUBLISH_MODE" >&2
        exit 1
    }
    # Play Store preparation selects the artifact compiled with the release
    # configuration. Signing is decided independently from the supplied keystore.
    [[ "$PUBLISH_MODE" == "" || " $BUILD_MODES " == *" release "* ]] || {
        echo "Play Store publication requires a release build" >&2
        exit 1
    }
    [[ "$PACKAGE_FORMAT" == apk || "$PACKAGE_FORMAT" == aab ]] || {
        echo "Unsupported Android package format: $PACKAGE_FORMAT" >&2
        exit 1
    }
}

validate_android_toolchain() {
    [[ -f "${ANDROID_NDK_ROOT}/source.properties" ]] || {
        echo "Selected Android NDK is not installed: ${ANDROID_NDK_ROOT}" >&2
        echo "Run '$0 setup' or set ANDROID_NDK_ROOT to an installed NDK." >&2
        exit 1
    }
    [[ -d "${ANDROID_SDK_ROOT}/platforms/${ANDROID_PLATFORM}" ]] || {
        echo "Selected Android platform is not installed: ${ANDROID_SDK_ROOT}/platforms/${ANDROID_PLATFORM}" >&2
        echo "Run '$0 setup' or install ${ANDROID_PLATFORM} with sdkmanager." >&2
        exit 1
    }
    [[ -x "${ANDROID_SDK_ROOT}/build-tools/${ANDROID_BUILD_TOOLS}/aapt" ]] || {
        echo "Selected Android build tools are not installed: ${ANDROID_SDK_ROOT}/build-tools/${ANDROID_BUILD_TOOLS}" >&2
        echo "Run '$0 setup' or install build-tools;${ANDROID_BUILD_TOOLS} with sdkmanager." >&2
        exit 1
    }
}

validate_signing() {
    local signing_setting
    [[ -z "${JAMULUS_ANDROID_KEYSTORE:-}" ]] && echo "No Android keystore file specified" >&2
    [[ -z "${JAMULUS_ANDROID_KEYSTORE_BASE64:-}" ]] && echo "No Android keystore base64 specified" >&2
    [[ -z "${JAMULUS_ANDROID_KEY_ALIAS:-}" ]] && echo "No Android keystore alias specified" >&2
    [[ -z "${JAMULUS_ANDROID_KEYSTORE_PASSWORD:-}" ]] && echo "No Android keystore password specified" >&2
    [[ -z "${JAMULUS_ANDROID_KEY_PASSWORD:-}" ]] && echo "No Android key password specified" >&2

    [[ -z "${JAMULUS_ANDROID_KEYSTORE:-}" || -z "${JAMULUS_ANDROID_KEYSTORE_BASE64:-}" ]] || {
        echo "Set only one of JAMULUS_ANDROID_KEYSTORE and JAMULUS_ANDROID_KEYSTORE_BASE64" >&2
        exit 1
    }
    for signing_setting in JAMULUS_ANDROID_KEYSTORE JAMULUS_ANDROID_KEYSTORE_BASE64 \
        JAMULUS_ANDROID_KEY_ALIAS \
        JAMULUS_ANDROID_KEYSTORE_PASSWORD JAMULUS_ANDROID_KEY_PASSWORD; do
        [[ -z "${!signing_setting:-}" || -n "${JAMULUS_ANDROID_KEYSTORE:-}" ||
            -n "${JAMULUS_ANDROID_KEYSTORE_BASE64:-}" ]] || {
            echo "$signing_setting requires an Android keystore" >&2
            exit 1
        }
    done
    if [[ -n "${JAMULUS_ANDROID_KEYSTORE:-}" || -n "${JAMULUS_ANDROID_KEYSTORE_BASE64:-}" ]]; then
        [[ -n "${JAMULUS_ANDROID_KEY_ALIAS:-}" ]] || {
            echo "JAMULUS_ANDROID_KEY_ALIAS is required with a keystore" >&2
            exit 1
        }
        [[ -n "${JAMULUS_ANDROID_KEYSTORE_PASSWORD:-}" ]] || {
            echo "JAMULUS_ANDROID_KEYSTORE_PASSWORD is required with a keystore" >&2
            exit 1
        }
    fi
    if [[ -n "${JAMULUS_ANDROID_KEYSTORE:-}" || -n "${JAMULUS_ANDROID_KEYSTORE_BASE64:-}" ]]; then
        prepare_keystore
    else
        prepare_debug_keystore
    fi
}

prepare_keystore() {
    if [[ -n "${JAMULUS_ANDROID_KEYSTORE:-}" ]]; then
        [[ -r "$JAMULUS_ANDROID_KEYSTORE" ]] || {
            echo "Android keystore is not readable: $JAMULUS_ANDROID_KEYSTORE" >&2
            exit 1
        }
        ANDROID_KEYSTORE_FILE="$JAMULUS_ANDROID_KEYSTORE"
        return
    fi

    if [[ -n "${JAMULUS_ANDROID_KEYSTORE_BASE64:-}" ]]; then
        ANDROID_KEYSTORE_FILE=~/.jamulus-android-signing.jks
        if ! touch "$ANDROID_KEYSTORE_FILE"; then
            echo "Failed to create Android keystore file: $ANDROID_KEYSTORE_FILE" >&2
            exit 1
        fi
        trap 'rm -f $ANDROID_KEYSTORE_FILE' EXIT
        touch "$ANDROID_KEYSTORE_FILE"
        chmod 600 "$ANDROID_KEYSTORE_FILE"
        printf '%s' "$JAMULUS_ANDROID_KEYSTORE_BASE64" | base64 --decode >> $ANDROID_KEYSTORE_FILE
    fi
}

prepare_debug_keystore() {
    ANDROID_KEYSTORE_FILE="${JAMULUS_ANDROID_DEBUG_KEYSTORE:-${HOME}/.jamulus-android-signing.jks}"
    JAMULUS_ANDROID_KEYSTORE_PASSWORD="android"
    JAMULUS_ANDROID_KEY_ALIAS="androiddebugkey"
    JAMULUS_ANDROID_KEY_PASSWORD="android"

    if [[ ! -f "${ANDROID_KEYSTORE_FILE}" ]]; then
        mkdir -p "$(dirname "${ANDROID_KEYSTORE_FILE}")"

        keytool -genkeypair \
            -keystore "${ANDROID_KEYSTORE_FILE}" \
            -storepass "${JAMULUS_ANDROID_KEYSTORE_PASSWORD}" \
            -alias "${JAMULUS_ANDROID_KEY_ALIAS}" \
            -keypass "${JAMULUS_ANDROID_KEY_PASSWORD}" \
            -keyalg RSA \
            -keysize 2048 \
            -validity 10000 \
            -dname "CN=Android Debug,O=Android,C=US"
        chmod 600 "${ANDROID_KEYSTORE_FILE}"
        [[ -n "${JAMULUS_ANDROID_DEBUG_KEYSTORE:-}" ]] || trap 'rm -f ${ANDROID_KEYSTORE_FILE}' EXIT
    fi
}

keystore_type() {
    local keystore_file="$1"
    local storepass="${2:-}"
    local detected_type

    if [[ -z "$storepass" ]]; then
        detected_type="$(keytool -list -keystore "$keystore_file" 2> /dev/null | awk 'BEGIN{IGNORECASE=1; status=1} /^Keystore type:/ {print $3; status=0} END {exit status}')"
    else
        detected_type="$(keytool -list -keystore "$keystore_file" -storepass "$storepass" 2> /dev/null | awk 'BEGIN{IGNORECASE=1; status=1} /^Keystore type:/ {print $3; status=0} END {exit status}')"
    fi

    [[ -n "$detected_type" ]] || return 1
    printf '%s' "$detected_type"
}

qmake5_qmake_make() {
    local build_dir="$1"
    shift
    local -n _p="$1"
    shift
    local qmake_config=("$@")
    local -a package_args=("${_p[@]}")

    pushd "${build_dir}" > /dev/null
    "${QT_QMAKE}" "${PROJECT_DIR}/Jamulus.pro" -spec android-clang "${qmake_config[@]}" ANDROID_ABIS="${TARGET_ARCHS// /,}"

    "${ANDROID_NDK_MAKE}" -j "$MAKE_JOBS"
    "${ANDROID_NDK_MAKE}" INSTALL_ROOT="${build_dir}" install

    set -x
    "${QT_ANDROIDDEPLOYQT}" --verbose --input android-Jamulus-deployment-settings.json --output "${build_dir}" "${package_args[@]}"
    set +x
    popd > /dev/null
}

qmake6_qmake_make() {
    local build_dir="$1"
    shift
    local -n _p="$1"
    shift
    local qmake_config=("$@")
    local -a package_args=("${_p[@]}")

    local -a target_archs
    read -r -a target_archs <<< "${TARGET_ARCHS}"

    # explicitly allow this to persist out of the loop.
    local i=0
    while ((i < ${#target_archs[@]})); do
        local target_arch="${target_archs[$i]}"
        ((i += 1))
        local qt_arch
        qt_arch="$(qt_android_arch "$target_arch")"
        local qmake6_qmake="${QT_DIR}/${QT_VERSION}/${qt_arch}/bin/qmake"
        [[ -x "$qmake6_qmake" ]] || {
            echo "Qt6 qmake not found for target arch $target_arch: $qmake6_qmake" >&2
            exit 1
        }

        mkdir -p "${build_dir}/${qt_arch}"
        find "${build_dir:?}/${qt_arch}" -mindepth 1 -delete
        pushd "${build_dir}/${qt_arch}" > /dev/null

        "${qmake6_qmake}" "${PROJECT_DIR}/Jamulus.pro" -spec android-clang "${qmake_config[@]}" ANDROID_ABIS="${target_arch}"
        "${ANDROID_NDK_MAKE}" -j "$MAKE_JOBS"
        "${ANDROID_NDK_MAKE}" INSTALL_ROOT="${build_dir}" install

        set -x
        if ((i < ${#target_archs[@]})); then
            # For all but the last target arch, we need to generate an intermediate deployment settings file for the next target arch.
            "${QT_ANDROIDDEPLOYQT}" --verbose --input android-Jamulus-deployment-settings.json --output "${build_dir}" --aux-mode
        else
            # For the last arch, we need to generate the final deployment settings file and package the app.
            local qtTargetAbiList
            qtTargetAbiList="$(
                IFS=,
                echo "${target_archs[*]}"
            )"
            [[ ${#target_archs[@]} -gt 1 ]] && echo 'qtTargetAbiList='"${qtTargetAbiList}" > "${build_dir}/gradle.properties"
            "${QT_ANDROIDDEPLOYQT}" --verbose --input android-Jamulus-deployment-settings.json --output "${build_dir}" "${package_args[@]}"
        fi
        set +x
        popd > /dev/null

    done
}

build_app() {
    local build_mode="$1"
    local build_dir="${BUILD_ROOT}/${build_mode}"
    local qmake_config=()

    for config in $QMAKE_CONFIG; do
        qmake_config+=("CONFIG+=${config}")
    done
    qmake_config+=("CONFIG+=android_single_config" "CONFIG+=${build_mode}" "CONFIG-=debug_and_release")
    echo "qmake config: ${qmake_config[*]}; android ABIs: ${TARGET_ARCHS}"

    local package_args=()
    if [[ -n "${ANDROID_KEYSTORE_FILE:-}" ]]; then
        # androiddeployqt --release selects signed-release packaging. It is
        # independent from qmake's debug/release compilation configuration.
        package_args+=(--release)
        local detected_keystore_type
        detected_keystore_type="$(keystore_type "${ANDROID_KEYSTORE_FILE}" "${JAMULUS_ANDROID_KEYSTORE_PASSWORD}")" || detected_keystore_type="unknown"

        # To conceal the keystore information from the command line, use environment variables
        export QT_ANDROID_KEYSTORE_PATH="${ANDROID_KEYSTORE_FILE}"
        export QT_ANDROID_KEYSTORE_ALIAS="${JAMULUS_ANDROID_KEY_ALIAS}"
        export QT_ANDROID_KEYSTORE_STORE_PASS="${JAMULUS_ANDROID_KEYSTORE_PASSWORD}"
        export QT_ANDROID_KEYSTORE_KEY_PASS="${JAMULUS_ANDROID_KEY_PASSWORD:-}"

        package_args+=(--sign)
        if [[ "$detected_keystore_type" == "PKCS12" ]]; then
            package_args+=(--storetype PKCS12)
            echo "Android release signing: forcing --storetype PKCS12 for PKCS12 keystore"
        else
            echo "Android release signing: leaving keystore type unspecified for ${detected_keystore_type} keystore"
        fi
    fi

    [[ "$PACKAGE_FORMAT" == aab ]] && package_args+=(--aab)
    echo "Android packaging: format=$([ "$PACKAGE_FORMAT" == aab ] && echo 'AAB' || echo 'APK') platform=${ANDROID_PLATFORM}"

    # Diagnostic environment - non-secret variables only
    echo "Android environment:"
    for var in ANDROID_PLATFORM ANDROID_SDK_ROOT ANDROID_NDK_ROOT TARGET_ARCHS JAVA_HOME QT_HOST_PATH ANDROID_BUILD_TOOLS; do
        [[ -n "${!var:-}" ]] && echo "  ${var}=${!var}"
    done

    rm -rf "${build_dir}"
    mkdir -p "${build_dir}"

    if [[ "$QT_VERSION" =~ 5\..* ]]; then
        qmake5_qmake_make "${build_dir}" package_args "${qmake_config[@]}"
    else
        qmake6_qmake_make "${build_dir}" package_args "${qmake_config[@]}"
    fi
}

pass_artifact_to_job() {
    if [[ -z "${JAMULUS_BUILD_VERSION:-}" ]]; then
        JAMULUS_BUILD_VERSION="$(python3 "${PROJECT_DIR}/.github/autobuild/get_build_vars.py" --print-build-version)"
        export JAMULUS_BUILD_VERSION
    fi
    [[ ${JAMULUS_BUILD_VERSION:-} =~ [0-9]+\.[0-9]+\.[0-9]+ ]] || {
        echo "JAMULUS_BUILD_VERSION has to be a valid version string" >&2
        exit 1
    }
    mkdir -p "${DEPLOY_DIR}"
    local artifact_number=1
    local build_mode
    local extension
    local artifact
    local package_path

    for build_mode in $BUILD_MODES; do
        extension="$PACKAGE_FORMAT"
        artifact="jamulus_${JAMULUS_BUILD_VERSION}_android_${build_mode}${ARTIFACT_SUFFIX}.${extension}"
        package_path="$(find "${BUILD_ROOT}/${build_mode}/build/outputs" -type f -name "*.${extension}" -print -quit)"
        [[ -n "$package_path" ]] || {
            echo "No Android ${extension} was produced for ${build_mode}" >&2
            exit 1
        }
        mv "$package_path" "${DEPLOY_DIR}/${artifact}"
        if [[ -n "${GITHUB_OUTPUT:-}" ]]; then
            echo "artifact_${artifact_number}=${artifact}" >> "$GITHUB_OUTPUT"
        else
            echo "${DEPLOY_DIR}/${artifact}"
        fi
        artifact_number=$((artifact_number + 1))
    done
}

distclean_android() {
    local generated_path

    for generated_path in "$BUILD_ROOT" "$DEPLOY_DIR"; do
        [[ "$generated_path" != / && "$generated_path" != "$PROJECT_DIR" ]] || {
            echo "Refusing to remove unsafe Android output path: $generated_path" >&2
            exit 1
        }
        rm -rf -- "$generated_path"
    done

    for generated_path in \
        "$PROJECT_DIR/debug" "$PROJECT_DIR"/debug-* \
        "$PROJECT_DIR/release" "$PROJECT_DIR"/release-* \
        "$PROJECT_DIR/build" "$PROJECT_DIR/play-store" \
        "$PROJECT_DIR/qmake_qmake_qm_files.qrc" "$PROJECT_DIR/.qm"; do
        [[ -e "$generated_path" ]] && rm -rf -- "$generated_path"
    done
}

case "${1:-}" in
    setup)
        setup_ubuntu_dependencies
        setup_android_ndk
        setup_android_sdk
        setup_qt
        ;;
    build)
        validate_build_mode
        validate_android_toolchain
        validate_signing
        for build_mode in $BUILD_MODES; do
            build_app "$build_mode"
        done
        ;;
    get-artifacts)
        pass_artifact_to_job
        ;;
    distclean)
        distclean_android
        ;;
    *)
        echo "Unknown stage '${1:-}'"
        exit 1
        ;;
esac
