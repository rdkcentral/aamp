#!/usr/bin/env bash
#set -x

# Fail the script should any step fail. To override this behavior use "|| true" on those statements
set -eo pipefail


# All include files should be done here
#
# tools and OS specifics
source scripts/tools.sh
# do_clone, et al
source scripts/install_clone.sh
# install_dir_fn
source scripts/install_dir.sh
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
# aampmetrics / aampabr build and install
source scripts/install_aampdeps.sh 
# rialto install and build
source scripts/install_rialto.sh
# gst-plugin-rdk install and build
source scripts/install_gstplugins.sh
# aamp-cli install and build
source scripts/install_aampcli.sh
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
echo "Building ${OPTION_AAMP_BRANCH} on ${OSTYPE} ${ARCH} starting $(date)"
echo ""
INSTALL_STATUS_ARR+=("tools_arch_fn check passed.")


# Decide on build directory, BUILD_DIR initialized in install_options.sh and updated in install_dir_fn
#
install_dir_fn
INSTALL_STATUS_ARR+=("install_dir_fn check passed.")
# 
# ! We're in the aamp repository directory at this point!

# create the build directory
aampcli_install_prebuild_fn ${OPTION_CLEAN}

# Location for dependencies built from source
# Needed here for install_pkgs_fn/systemd on MacOS
#
LOCAL_DEPS_BUILD_DIR="${AAMP_DIR}/.libs"
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

# Build libdash
install_build_libdash_fn "${OPTION_CLEAN}" 
INSTALL_STATUS_ARR+=("install_build_libdash check passed.")

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
subtec_install_build_fn "${CLEAN}"
INSTALL_STATUS_ARR+=("subtec_install_build check passed.")

# Build aampabr / aampmetrics
#
aampdeps_install_build_fn "${OPTION_CLEAN}" 
INSTALL_STATUS_ARR+=("install_build_aampdeps check passed.")

# Build rialto
#
rialto_install_build_fn "${OPTION_CLEAN}"
INSTALL_STATUS_ARR+=("rialto_install_build_fn check passed.")

# Install subtec-app script
# Needs the AAMP build directory to be created by aampcli_install_build first
subtec_install_run_script_fn
INSTALL_STATUS_ARR+=("subtec_install_run_script check passed.")

# Build libaamp / aamp-cli
#
aampcli_install_build_fn "${CLEAN}"
INSTALL_STATUS_ARR+=("aampcli_install_build check passed.")

# gst plugin needs to be build after aamp-cli/libsubtec
gstplugin_install_build_fn "${CLEAN}"
INSTALL_STATUS_ARR+=("gstplugin_install_build_script check passed.")

# Post build aamp-cli
#
aampcli_install_postbuild_fn "${CLEAN}"
INSTALL_STATUS_ARR+=("aampcli_install_postbuild check passed.")

tools_print_summary_fn

# DONE
