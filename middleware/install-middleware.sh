#!/usr/bin/env bash
#
# If not stated otherwise in this file or this component's license file the
# following copyright and licenses apply:
#
# Copyright 2020 RDK Management
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
#
#set -x

if [[ -z "${MAKEFLAGS}" ]]; then
    export MAKEFLAGS=-j$(nproc)
fi

# Fail the script should any step fail. To override this behavior use "|| true" on those statements
set -eo pipefail

# All include files should be done here
#
# tools and OS specifics
source scripts/tools.sh
# do_clone, et al
source scripts/install_clone.sh
# install_options_fn
source scripts/install_options.sh
# pre-built dependencies install
source scripts/install_dependencies.sh
# gtest install and build
source scripts/install_gtest.sh
# glib install and build
source scripts/install_glib.sh
# libdash install and build
source scripts/install_libdash.sh
# libcjson install and build
source scripts/install_libcjson.sh
# gstreamer install
source scripts/install_gstreamer.sh
# subtec install and build
source scripts/install_subtec.sh
# rialto install and build
source scripts/install_rialto.sh
#


# VARIABLES

# Elapsed time
SECONDS=0

declare ARCH=""
# Collect summary to be printed at the end of execution
declare -a INSTALL_STATUS_ARR
#
# See scripts/install_options.sh for other variables

# directory where we build dependencies from source
declare LOCAL_DEPS_BUILD_DIR

PLAYER_DIR=$PWD

# MAIN execution starts

# Get and process install options
install_options_fn "$@" 
INSTALL_STATUS_ARR+=("install_options_fn check passed.")

tools_banner_fn

echo "Checking for installed compilation tools"
tools_install_fn
echo ""
INSTALL_STATUS_ARR+=("tools_install_fn check passed.")


ARCH=$(tools_arch_fn)
echo "Building ${OPTION_PLAYER_BRANCH} on ${OSTYPE} ${ARCH} starting $(date)"
echo ""
INSTALL_STATUS_ARR+=("tools_arch_fn check passed.")

LOCAL_DEPS_BUILD_DIR="${PLAYER_DIR}/.libs"
echo ""
echo "Building dependencies in ${LOCAL_DEPS_BUILD_DIR}"
if [ ! -d ${LOCAL_DEPS_BUILD_DIR} ]; then
    mkdir ${LOCAL_DEPS_BUILD_DIR}
fi


# Install prebuilt dependencies
#
if [ ${OPTION_QUICK} = false ] ; then
    echo ""
    echo "*** Check/Install dependency packages"
    install_pkgs_fn 
    INSTALL_STATUS_ARR+=("install_pkgs_fn check passed.")
else
    INSTALL_STATUS_ARR+=("install_pkgs_fn check SKIPPED.")
fi


# Install built dependencies
echo ""
echo "*** Check/Install source packages"

# Install gstreamer
#
install_gstreamer_fn 
INSTALL_STATUS_ARR+=("install_gstreamer_fn check passed.")

# Build gst-plugins-good. install_gstreamer_fn must have been called first
install_gstpluginsgoodfn $OPTION_CLEAN
INSTALL_STATUS_ARR+=("install_gstplugingood_fn check passed.")

# Build googletest
#
install_build_googletest_fn "${OPTION_CLEAN}" 
INSTALL_STATUS_ARR+=("install_build_googletest check passed.")

# Build glib
#
install_build_glib_fn "${OPTION_CLEAN}" 
INSTALL_STATUS_ARR+=("install_build_glib check passed.")

# Build libcjson
install_build_libcjson_fn "${OPTION_CLEAN}" 
INSTALL_STATUS_ARR+=("install_build_libcjson check passed.")

# Build subtec
#
CLEAN=false
if [ ${OPTION_CLEAN} = true ] ; then 
    CLEAN=true
fi
if [ ${OPTION_SUBTEC_CLEAN} = true ] ; then
    CLEAN=true
fi

if [ ${OPTION_SUBTEC_SKIP} = false ] ; then 
    subtec_install_build_fn "${CLEAN}"
    INSTALL_STATUS_ARR+=("subtec_install_build check passed.")
else
    INSTALL_STATUS_ARR+=("subtec_install_build check SKIPPED.")
fi

# Build rialto
#
rialto_install_build_fn "${OPTION_CLEAN}"
INSTALL_STATUS_ARR+=("rialto_install_build_fn check passed.")

tools_print_summary_fn

# DONE
