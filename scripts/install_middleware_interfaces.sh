#!/usr/bin/env bash
# If not stated otherwise in this file or this component's license file the
# following copyright and licenses apply:
#
# Copyright 2026 RDK Management
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# PREREQUISITES:
# This script requires the following variables to be set:
# - LOCAL_DEPS_BUILD_DIR: Directory where dependencies will be built and installed
# - AAMP_DIR: Path to the aamp repository (used to auto-detect sibling middleware-player-interface)
# - INSTALL_STATUS_ARR: Array to store installation status messages
#
# Optional variables (from install_options.sh):
# - OPTION_MIDDLEWARE_PLAYER_INTERFACE_LOCAL_PATH: explicit path to a local middleware-player-interface repo
# - OPTION_MIDDLEWARE_PLAYER_INTERFACE_COMMIT_ID: specific commit to checkout (only applies to GitHub clones)
#
# This script requires the following functions to be available (for GitHub clone fallback only):
# - do_clone_fn: Function to clone git repositories
#
# Required tools: cmake, make, pkg-config (git only needed for GitHub clone fallback)

# Sync internal middleware headers to .libs/include.
# When using --player-interface-source=internal (the default), headers in
# .libs/include are never refreshed by the install flow.  If the source tree
# is updated (e.g. a new constructor parameter), the stale installed copy is
# found first on the include path and causes build errors.
# This function compares every header already present in .libs/include against
# its counterpart in middleware/ and overwrites any that differ.
function sync_internal_middleware_headers_fn()
{
    local include_dir="${LOCAL_DEPS_BUILD_DIR}/include"
    local middleware_dir="${AAMP_DIR}/middleware"
    local updated=0

    if [[ ! -d "${include_dir}" ]] || [[ ! -d "${middleware_dir}" ]]; then
        return 0
    fi

    while IFS= read -r -d '' installed; do
        local base
        base=$(basename "${installed}")
        local src
        # Search only the middleware source tree, pruning the .libs build
        # artifact directory.  .libs holds vendored third-party build outputs
        # (e.g. glib, protobuf) whose headers share common basenames such as
        # config.h.  Matching those by basename would overwrite unrelated
        # installed headers (e.g. libdash/config.h), breaking the build.
        src=$(find "${middleware_dir}" -maxdepth 4 -name .libs -prune -o -name "${base}" -print 2>/dev/null | head -1)
        if [[ -n "${src}" ]] && ! diff -q "${installed}" "${src}" > /dev/null 2>&1; then
            echo "Refreshing stale header: ${base}"
            cp "${src}" "${installed}" || {
                echo "Error: Failed to copy ${src} -> ${installed}"
                return 1
            }
            (( updated++ )) || true
        fi
    done < <(find "${include_dir}" \( -name "*.h" -o -name "*.hpp" \) -print0)

    if [[ ${updated} -eq 0 ]]; then
        echo "Internal middleware headers are up to date."
        INSTALL_STATUS_ARR+=("sync_internal_middleware_headers_fn: all headers up to date.")
    else
        echo "Refreshed ${updated} stale middleware header(s)."
        INSTALL_STATUS_ARR+=("sync_internal_middleware_headers_fn: refreshed ${updated} header(s).")
    fi
}

function install_build_middleware_interface_fn()
{
    # Validate required variables
    if [[ -z "$LOCAL_DEPS_BUILD_DIR" ]]; then
        echo "Error: LOCAL_DEPS_BUILD_DIR variable is not set"
        return 1
    fi

    # Check if LOCAL_DEPS_BUILD_DIR exists or can be created
    if [[ ! -d "$LOCAL_DEPS_BUILD_DIR" ]]; then
        echo "Creating LOCAL_DEPS_BUILD_DIR: $LOCAL_DEPS_BUILD_DIR"
        mkdir -p "$LOCAL_DEPS_BUILD_DIR" || {
            echo "Error: Failed to create LOCAL_DEPS_BUILD_DIR: $LOCAL_DEPS_BUILD_DIR"
            return 1
        }
    fi

    # Check for required tools
    for tool in cmake make pkg-config; do
        if ! command -v "$tool" &> /dev/null; then
            echo "Error: Required tool '$tool' is not installed"
            return 1
        fi
    done

    # Initialize INSTALL_STATUS_ARR if not already initialized
    if [[ -z "${INSTALL_STATUS_ARR+x}" ]]; then
        declare -g -a INSTALL_STATUS_ARR=()
    fi

    # Resolve the middleware-player-interface source directory.
    # Priority: (1) explicit --middleware-player-interface-local-path option,
    #           (2) sibling repo at <parent-of-AAMP_DIR>/middleware-player-interface,
    #           (3) clone from GitHub into LOCAL_DEPS_BUILD_DIR.
    local mw_src=""
    local mw_cloned=false

    if [[ -n "${OPTION_MIDDLEWARE_PLAYER_INTERFACE_LOCAL_PATH:-}" ]]; then
        mw_src="$(cd "${OPTION_MIDDLEWARE_PLAYER_INTERFACE_LOCAL_PATH}" 2>/dev/null && pwd -P)" || {
            echo "Error: --middleware-player-interface-local-path '${OPTION_MIDDLEWARE_PLAYER_INTERFACE_LOCAL_PATH}' does not exist"
            return 1
        }
        echo "Using explicit local middleware-player-interface: ${mw_src}"
        if [[ -n "${OPTION_MIDDLEWARE_PLAYER_INTERFACE_COMMIT_ID:-}" ]]; then
            echo "Warning: --middleware-player-interface-commit-id is ignored when --middleware-player-interface-local-path is set"
        fi
    else
        local sibling_path
        sibling_path="$(dirname "${AAMP_DIR}")/middleware-player-interface"
        if [[ -d "${sibling_path}" ]]; then
            mw_src="$(cd "${sibling_path}" && pwd -P)"
            echo "Using sibling middleware-player-interface repo at HEAD: ${mw_src}"
            if [[ -n "${OPTION_MIDDLEWARE_PLAYER_INTERFACE_COMMIT_ID:-}" ]]; then
                echo "Warning: --middleware-player-interface-commit-id is ignored when a sibling repo is present; checkout the desired commit manually in ${mw_src}"
            fi
        fi
    fi

    # If no local source found, clone from GitHub
    if [[ -z "${mw_src}" ]]; then
        if ! command -v git &> /dev/null; then
            echo "Error: git is not installed (needed to clone middleware-player-interface)"
            return 1
        fi
        if ! declare -f do_clone_fn > /dev/null; then
            echo "Error: do_clone_fn function is not available"
            return 1
        fi

        cd "$LOCAL_DEPS_BUILD_DIR" || {
            echo "Error: Failed to change directory to $LOCAL_DEPS_BUILD_DIR"
            return 1
        }

        # $OPTION_CLEAN == true
        if [[ "$1" == true ]] ; then
            echo "middleware-player-interface clean"
            if [ -d middleware-player-interface ] ; then
                rm -rf middleware-player-interface || {
                    echo "Error: Failed to remove middleware-player-interface directory"
                    return 1
                }
            fi
        fi

        if [ -d "middleware-player-interface" ]; then
            echo "middleware-player-interface clone already present in .libs"
            mw_src="${LOCAL_DEPS_BUILD_DIR}/middleware-player-interface"
        else
            echo "Cloning middleware-player-interface from GitHub (develop branch)..."
            if ! do_clone_fn -b develop https://github.com/rdkcentral/middleware-player-interface.git; then
                echo "Error: Failed to clone middleware-player-interface repository"
                return 1
            fi

            mw_src="${LOCAL_DEPS_BUILD_DIR}/middleware-player-interface"
            mw_cloned=true
        fi
    fi

    # Checkout specific commit if requested (only applies to GitHub clones)
    if [[ -n "${OPTION_MIDDLEWARE_PLAYER_INTERFACE_COMMIT_ID:-}" ]] && [[ "${mw_cloned}" == true ]]; then
        echo "Checking out specified commit: ${OPTION_MIDDLEWARE_PLAYER_INTERFACE_COMMIT_ID}"
        cd "${mw_src}" || {
            echo "Error: Failed to change directory to ${mw_src}"
            return 1
        }
        if ! git checkout "${OPTION_MIDDLEWARE_PLAYER_INTERFACE_COMMIT_ID}" 2>/dev/null; then
            echo "Warning: Failed to checkout commit ${OPTION_MIDDLEWARE_PLAYER_INTERFACE_COMMIT_ID}, using HEAD"
        else
            echo "Checked out commit: $(git rev-parse HEAD)"
        fi
        cd - > /dev/null
    fi

    # Build and install from mw_src
    local mw_build_marker="${LOCAL_DEPS_BUILD_DIR}/.mw_installed"

    # On clean, remove installed artifacts so we rebuild
    if [[ "$1" == true ]] ; then
        rm -f "${mw_build_marker}"
        rm -rf "${LOCAL_DEPS_BUILD_DIR}/include/middleware-player-interface" 2>/dev/null || true
    fi

    # Check if we need to rebuild based on commit hash
    local current_commit=""
    if [[ -d "${mw_src}/.git" ]]; then
        current_commit=$(cd "${mw_src}" && git rev-parse HEAD 2>/dev/null || echo "unknown")
    fi
    
    local needs_rebuild=false
    if [[ ! -f "${mw_build_marker}" ]]; then
        needs_rebuild=true
    elif [[ -n "${current_commit}" ]] && [[ "${current_commit}" != "unknown" ]]; then
        local built_commit=$(cat "${mw_build_marker}" 2>/dev/null || echo "")
        if [[ "${built_commit}" != "${current_commit}" ]]; then
            echo "Middleware commit changed from ${built_commit:0:8} to ${current_commit:0:8}, rebuilding..."
            needs_rebuild=true
        fi
    fi

    if [[ "${needs_rebuild}" == false ]] && [[ "${mw_cloned}" == false ]]; then
        echo "middleware-player-interface headers/libs already installed (commit ${current_commit:0:8})"
        INSTALL_STATUS_ARR+=("middleware-player-interface was already installed.")
        return 0
    fi

    local mw_build_dir="${mw_src}/build_aamp"
    rm -rf "${mw_build_dir}"
    mkdir -p "${mw_build_dir}" || {
        echo "Error: Failed to create build directory ${mw_build_dir}"
        return 1
    }

    cd "${mw_build_dir}" || {
        echo "Error: Failed to change directory to ${mw_build_dir}"
        return 1
    }

    echo "Running cmake configuration for middleware-player-interface..."
    local cmake_platform_flag=""
    if [[ "$OSTYPE" == "linux"* ]]; then
        cmake_platform_flag="-DCMAKE_PLATFORM_UBUNTU=ON"
    fi

    local pkg_config_path_arg=""
    if [[ -d "${LOCAL_DEPS_BUILD_DIR}/lib/pkgconfig" ]]; then
        pkg_config_path_arg="${LOCAL_DEPS_BUILD_DIR}/lib/pkgconfig"
    fi

    # On macOS, add GStreamer pkg-config paths (framework install takes priority, homebrew is fallback)
    if [[ "$OSTYPE" == "darwin"* ]]; then
        local _GST_FRAMEWORK_PKG="/Library/Frameworks/GStreamer.framework/Versions/1.0/lib/pkgconfig"
        if [ -d "${_GST_FRAMEWORK_PKG}" ]; then
            pkg_config_path_arg="${_GST_FRAMEWORK_PKG}:${pkg_config_path_arg}"
        else
            local _GST_BREW_PREFIX
            _GST_BREW_PREFIX=$(brew --prefix gstreamer 2>/dev/null) || true
            if [ -n "${_GST_BREW_PREFIX}" ] && [ -d "${_GST_BREW_PREFIX}/lib/pkgconfig" ]; then
                pkg_config_path_arg="${_GST_BREW_PREFIX}/lib/pkgconfig:${pkg_config_path_arg}"
                local _GST_BASE_PREFIX
                _GST_BASE_PREFIX=$(brew --prefix gst-plugins-base 2>/dev/null) || true
                if [ -n "${_GST_BASE_PREFIX}" ] && [ -d "${_GST_BASE_PREFIX}/lib/pkgconfig" ]; then
                    pkg_config_path_arg="${_GST_BASE_PREFIX}/lib/pkgconfig:${pkg_config_path_arg}"
                fi
            fi
        fi
    fi

    local openssl_root_flag=""
    if [[ "$OSTYPE" == "darwin"* ]]; then
        local _OPENSSL_PREFIX
        _OPENSSL_PREFIX=$(brew --prefix openssl@3 2>/dev/null) || true
        if [ -n "${_OPENSSL_PREFIX}" ]; then
            openssl_root_flag="-DOPENSSL_ROOT_DIR=${_OPENSSL_PREFIX}"
            if [ -d "${_OPENSSL_PREFIX}/lib/pkgconfig" ]; then
                pkg_config_path_arg="${_OPENSSL_PREFIX}/lib/pkgconfig:${pkg_config_path_arg}"
            fi
        fi
        local _UUID_PREFIX
        _UUID_PREFIX=$(brew --prefix ossp-uuid 2>/dev/null) || true
        if [ -n "${_UUID_PREFIX}" ] && [ -d "${_UUID_PREFIX}/lib/pkgconfig" ]; then
            pkg_config_path_arg="${_UUID_PREFIX}/lib/pkgconfig:${pkg_config_path_arg}"
        fi
    fi

    if ! PKG_CONFIG_PATH="${pkg_config_path_arg}:${PKG_CONFIG_PATH:-}" cmake "${mw_src}" \
            -DCMAKE_INSTALL_PREFIX="${LOCAL_DEPS_BUILD_DIR}" \
            ${cmake_platform_flag} \
            ${openssl_root_flag} \
            -DCMAKE_BUILD_TYPE=Debug; then
        echo "Error: CMake configuration failed for middleware-player-interface"
        return 1
    fi

    echo "Building middleware-player-interface..."
    if ! make; then
        echo "Error: Build failed for middleware-player-interface"
        return 1
    fi

    echo "Installing middleware-player-interface..."
    if ! make install; then
        echo "Error: Installation failed for middleware-player-interface"
        return 1
    fi

    # Create pkg-config directory if it doesn't exist
    mkdir -p "$LOCAL_DEPS_BUILD_DIR/lib/pkgconfig" || {
        echo "Error: Failed to create pkgconfig directory"
        return 1
    }

    # Write commit hash to marker file for future rebuild detection
    if [[ -n "${current_commit}" ]] && [[ "${current_commit}" != "unknown" ]]; then
        echo "${current_commit}" > "${mw_build_marker}"
    else
        touch "${mw_build_marker}"
    fi
    echo "middleware-player-interface installation completed successfully"
    INSTALL_STATUS_ARR+=("middleware was successfully installed.")
}
