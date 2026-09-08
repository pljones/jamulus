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
# shellcheck disable=SC1091
source "${PROJECT_DIR}/.github/autobuild/android-dependencies.sh"

# Defaults must match the cache values in .github/workflows/autobuild.yml
export ANDROID_SDK_ROOT="${JAMULUS_ANDROID_SDK_ROOT:-${ANDROID_SDK_ROOT:-${HOME}/android-sdk}}"
export ANDROID_NDK_ROOT="${JAMULUS_ANDROID_NDK_ROOT:-${ANDROID_NDK_ROOT:-${HOME}/android-ndk}}"
QT_DIR="${QT_DIR:-${HOME}/qt}"

# Ensure tools know where things are
export ANDROID_HOME="${ANDROID_SDK_ROOT}"
export ANDROID_NDK="${ANDROID_NDK_ROOT}"
export ANDROID_NDK_HOME="${ANDROID_NDK_ROOT}"
export ANDROID_NDK_LATEST_HOME="${ANDROID_NDK_ROOT}"

if [[ ! ${QT_VERSION:-} =~ [0-9]+\.[0-9]+\..* ]]; then
    echo "Environment variable QT_VERSION must be set to a valid Qt version"
    exit 1
fi

readonly QT_ANDROID_DIR="${QT_DIR}/${QT_VERSION}/android"
export QT_SELECT="${QT_VERSION}-android"
export QTTOOLDIR="${QT_ANDROID_DIR}/bin"
export QTLIBDIR="${QT_ANDROID_DIR}/lib"

readonly QT_QMAKE="${QT_ANDROID_DIR}/bin/qmake"
readonly QT_ANDROIDDEPLOYQT="${QT_ANDROID_DIR}/bin/androiddeployqt"

ANDROID_PLATFORM="${ANDROID_DEPLOYMENT_PLATFORM:-${ANDROID_PLATFORM}}"

readonly BUILD_ROOT="${JAMULUS_ANDROID_BUILD_DIR:-${PROJECT_DIR}/android-build}"
readonly DEPLOY_DIR="${JAMULUS_ANDROID_DEPLOY_DIR:-${PROJECT_DIR}/deploy}"
readonly BUILD_MODES="${BUILD_MODES:-debug release}"
readonly TARGET_ARCHS="${TARGET_ARCHS:-armeabi-v7a arm64-v8a x86 x86_64}"
readonly QMAKE_CONFIG="${JAMULUS_ANDROID_QMAKE_CONFIG:-}"
readonly PUBLISH_MODE="${JAMULUS_ANDROID_PUBLISH:-}"

# Only variables which are really needed by sub-commands are exported.
# Definitions have to stay in a specific order due to dependencies.
export PATH="${PATH}:${ANDROID_SDK_ROOT}/tools"
export PATH="${PATH}:${ANDROID_SDK_ROOT}/platform-tools"
export JAVA_HOME="${JAVA_HOME:-/usr/lib/jvm/java-8-openjdk-amd64/}"

export JAMULUS_ANDROID_KEYSTORE="${JAMULUS_ANDROID_KEYSTORE:-}"
export JAMULUS_ANDROID_KEYSTORE_BASE64="${JAMULUS_ANDROID_KEYSTORE_BASE64:-}"
export JAMULUS_ANDROID_KEY_ALIAS="${JAMULUS_ANDROID_KEY_ALIAS:-}"
export JAMULUS_ANDROID_KEYSTORE_PASSWORD="${JAMULUS_ANDROID_KEYSTORE_PASSWORD:-}"
export JAMULUS_ANDROID_KEY_PASSWORD="${JAMULUS_ANDROID_KEY_PASSWORD:-}"

readonly COMMANDLINETOOLS_DIR="${ANDROID_SDK_ROOT}"/cmdline-tools/latest
readonly ANDROID_SDKMANAGER=("${COMMANDLINETOOLS_DIR}/bin/sdkmanager" "--sdk_root=${ANDROID_SDK_ROOT}")

readonly ANDROID_NDK_HOST="${ANDROID_NDK_HOST:-linux-x86_64}"
readonly ANDROID_NDK_MAKE=${ANDROID_NDK_ROOT}/prebuilt/${ANDROID_NDK_HOST}/bin/make

setup_ubuntu_dependencies() {
    export DEBIAN_FRONTEND="noninteractive"

    sudo apt-get -qq update
    sudo apt-get -qq --no-install-recommends -y install build-essential zip unzip bzip2 p7zip-full curl chrpath openjdk-8-jdk-headless
}

setup_android_sdk() {
    # We may need to create the cmdline_tools directory and chown it to the runner user to fix permissions
    # (even after cache recovery)
    sudo mkdir -p "${COMMANDLINETOOLS_DIR}"
    sudo chown -R "$(whoami)" "${ANDROID_SDK_ROOT}"

    if [[ -d "${COMMANDLINETOOLS_DIR}" && -x "${ANDROID_SDKMANAGER[0]}" ]]; then
        echo "Using SDK installation from previous run (actions/cache)"
        return
    fi

    echo "Installing SDK"
    pushd "${COMMANDLINETOOLS_DIR}" > /dev/null

    curl -s -o downloadfile.zip "https://dl.google.com/android/repository/commandlinetools-linux-${COMMANDLINETOOLS_VERSION}_latest.zip"
    unzip -q downloadfile.zip -d sdk-tmpdir
    rm -f downloadfile.zip

    mv sdk-tmpdir/cmdline-tools/* .
    rm -rf sdk-tmpdir

    popd > /dev/null

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

    if [[ -f "${ANDROID_NDK_ROOT}/source.properties" && -x "${ANDROID_NDK_MAKE}" ]]; then
        echo "Using NDK installation from previous run (actions/cache)"
        return
    fi

    echo "Installing NDK"
    pushd "${ANDROID_NDK_ROOT}" > /dev/null

    curl -s -o downloadfile.zip "https://dl.google.com/android/repository/android-ndk-${ANDROID_NDK_VERSION}-linux-x86_64.zip"
    unzip -q downloadfile.zip -d ndk-tmpdir
    rm -f downloadfile.zip

    mv ndk-tmpdir/"android-ndk-${ANDROID_NDK_VERSION}"/* .
    rm -rf ndk-tmpdir

    popd > /dev/null
}

setup_qt() {
    # We may need to create the Qt installation directory and chown it to the runner user to fix permissions
    # (even after cache recovery)
    sudo mkdir -p "${QT_DIR}"
    sudo chown -R "$(whoami)" "${QT_DIR}"

    if [[ -x "${QT_QMAKE}" && -x "${QT_ANDROIDDEPLOYQT}" ]]; then
        echo "Using Qt installation from previous run (actions/cache)"
        return
    fi

    echo "Installing Qt"

    # Create and enter virtual environment
    python3 -m venv venv
    # Must hide directory as it just gets created during execution of the previous command and cannot be found by shellcheck
    # shellcheck source=/dev/null
    source venv/bin/activate

    pip install "aqtinstall==${AQTINSTALL_VERSION}"

    # Install actual Android Qt:
    local qtmultimedia=()
    if [[ ! "${QT_VERSION}" =~ 5\..* ]]; then
        # From Qt6 onwards, qtmultimedia is a module and cannot be installed
        # as an archive anymore.
        qtmultimedia=("--modules")
    fi
    qtmultimedia+=("qtmultimedia")

    python3 -m aqt install-qt --outputdir "${QT_DIR}" linux android "${QT_VERSION}" \
        --archives qtbase qttools qttranslations qtandroidextras \
        "${qtmultimedia[@]}"

    # Delete libraries, which we don't use, but which bloat the resulting package and might introduce unwanted dependencies.
    # iOS does not do this - leave for now, fix across all later.
    find "${QT_ANDROID_DIR}" -name 'libQt5*Quick*.so' -delete
    rm -r "${QT_ANDROID_DIR}/qml/"

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
    [[ "$PUBLISH_MODE" == "" || " $BUILD_MODES " == *" release "* ]] || {
        echo "Play Store publication requires a release build" >&2
        exit 1
    }
    if [[ "$PUBLISH_MODE" == play-store && -z "${JAMULUS_ANDROID_KEYSTORE:-}" &&
        -z "${JAMULUS_ANDROID_KEYSTORE_BASE64:-}" ]]; then
        echo "Play Store publication requires an Android keystore" >&2
        exit 1
    fi
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
        printf '%s' "$JAMULUS_ANDROID_KEYSTORE_BASE64" | base64 --decode > $ANDROID_KEYSTORE_FILE
        chmod 600 $ANDROID_KEYSTORE_FILE
        trap 'rm -f $ANDROID_KEYSTORE_FILE' EXIT
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
        detected_type="$(keytool -list -keystore "$keystore_file" 2> /dev/null | awk 'BEGIN{IGNORECASE=1} /^Keystore type:/ {print $3; exit 0} END {exit 1}')"
    else
        detected_type="$(keytool -list -keystore "$keystore_file" -storepass "$storepass" 2> /dev/null | awk 'BEGIN{IGNORECASE=1} /^Keystore type:/ {print $3; exit 0} END {exit 1}')"
    fi

    [[ -n "$detected_type" ]] || return 1
    printf '%s' "$detected_type"
}

build_app() {
    local build_mode="$1"
    local build_dir="${BUILD_ROOT}/${build_mode}"

    rm -rf "${build_dir}"
    mkdir -p "${build_dir}"

    local package_args=(--input "${build_dir}/android-Jamulus-deployment-settings.json"
        --output "${build_dir}" --android-platform "${ANDROID_PLATFORM}" --jdk "${JAVA_HOME}")
    if [[ -n "${ANDROID_KEYSTORE_FILE:-}" ]]; then
        package_args+=(--release)
        local detected_keystore_type
        detected_keystore_type="$(keystore_type "${ANDROID_KEYSTORE_FILE}" "${JAMULUS_ANDROID_KEYSTORE_PASSWORD}")" || detected_keystore_type="unknown"
        echo "Android release signing: keystore=${ANDROID_KEYSTORE_FILE} type=${detected_keystore_type} alias=${JAMULUS_ANDROID_KEY_ALIAS}"
        package_args+=(--sign "${ANDROID_KEYSTORE_FILE}" "${JAMULUS_ANDROID_KEY_ALIAS}"
            --storepass "${JAMULUS_ANDROID_KEYSTORE_PASSWORD}")
        [[ -n "${JAMULUS_ANDROID_KEY_PASSWORD:-}" ]] &&
            package_args+=(--keypass "${JAMULUS_ANDROID_KEY_PASSWORD}")
        if [[ "$detected_keystore_type" == "PKCS12" ]]; then
            package_args+=(--storetype PKCS12)
            echo "Android release signing: forcing --storetype PKCS12 for PKCS12 keystore"
        else
            echo "Android release signing: leaving keystore type unspecified for ${detected_keystore_type} keystore"
        fi
    fi
    [[ "$build_mode" == release && "$PUBLISH_MODE" == play-store ]] && package_args+=(--aab)
    echo "Android deploy args: ${package_args[*]}"

    local qmake_config=()
    for config in $QMAKE_CONFIG; do
        qmake_config+=("CONFIG+=${config}")
    done
    qmake_config+=("ANDROID_ABIS=${TARGET_ARCHS}")
    echo "qmake config: ${qmake_config[*]} ${build_mode}"

    pushd "${build_dir}" > /dev/null
    "${QT_QMAKE}" "${PROJECT_DIR}/Jamulus.pro" -spec android-clang \
        CONFIG+=android_single_config CONFIG+="${build_mode}" CONFIG-=debug_and_release \
        "${qmake_config[@]}"
    "${ANDROID_NDK_MAKE}" -j "$(nproc)"
    "${ANDROID_NDK_MAKE}" INSTALL_ROOT="${build_dir}" install
    popd > /dev/null

    env | grep ANDROID | sort
    "${QT_ANDROIDDEPLOYQT}" "${package_args[@]}"
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
        extension=apk
        [[ "$build_mode" == release && "$PUBLISH_MODE" == play-store ]] && extension=aab
        artifact="jamulus_${JAMULUS_BUILD_VERSION}_android_${build_mode}.${extension}"
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
        "$PROJECT_DIR/.qm"; do
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
