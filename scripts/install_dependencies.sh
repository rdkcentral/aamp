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

declare DEFAULT_OPENSSL_VERSION="openssl@3"

function install_pkgs_pkgconfig_darwin_fn() 
{
    echo "`brew --prefix ${1}`/lib/pkgconfig"
}

function install_pkgs_darwin_fn() 
{
    # Check if brew package $1 is installed
    # http://stackoverflow.com/a/20802425/1573477
    for PKG in "$@";
    do
        if brew ls --versions $PKG > /dev/null; then
            echo "${PKG} is already installed."
            INSTALL_STATUS_ARR+=("${PKG} is already installed.")
        else
            echo "Installing ${PKG}"
            brew install $PKG
            #update summery
            if brew ls --versions $PKG > /dev/null; then
                #The package is successfully installed
                INSTALL_STATUS_ARR+=("The package was ${PKG} was successfully installed.")

            else
                #The package is failed to be installed
                INSTALL_STATUS_ARR+=("The package ${PKG} FAILED to be installed.")
            fi
        fi
        #if pkg is openssl and its successfully installed every time ensure to symlink to the latest version
        if [ $PKG = "${DEFAULT_OPENSSL_VERSION}" ]; then
            OPENSSL_PATH=$(brew --prefix ${DEFAULT_OPENSSL_VERSION})
            # link may not exist so don't fail
            OPENSSL_CUR_PATH=`readlink /usr/local/ssl` || true
            if [ "$OPENSSL_CUR_PATH" != "{$OPENSSL_PATH}" ] ; then
                sudo rm -f /usr/local/ssl || true
                sudo ln -s $OPENSSL_PATH /usr/local/ssl
            fi 
        fi
        PKGDIR="`brew --prefix ${PKG}`/lib/pkgconfig:"
        INSTALLED_PKGCONFIG=$PKGDIR$INSTALLED_PKGCONFIG

	# Add the path to the pkgconfig directory to the PKG_CONFIG_PATH for openldap and krb5
        if [ $PKG = "openldap" ] || [ $PKG = "krb5" ]; then
            brew link $PKG --force
            if [ "$(uname -m)" = "arm64" ]; then
                export PKG_CONFIG_PATH="/opt/homebrew/opt/krb5/lib/pkgconfig:/opt/homebrew/opt/openldap/lib/pkgconfig:$PKG_CONFIG_PATH"
            else
                export PKG_CONFIG_PATH="/usr/local/opt/krb5/lib/pkgconfig:/usr/local/opt/openldap/lib/pkgconfig:$PKG_CONFIG_PATH"
            fi

        fi
    done
    echo "${INSTALLED_PKGCONFIG}"
}

function package_exists_lin_fn() {
    dpkg -s "$1" &> /dev/null
    return $?
}

function install_package_fn() {
    if ! package_exists_lin_fn $1 ; then
        echo "Installing $1"
        sudo apt install $1 -y
        if [ $? == 0 ] ; then
            INSTALL_STATUS_ARR+=("$1 was successfully installed.")
        else
            INSTALL_STATUS_ARR+=("The package $1 FAILED to be installed.")
        fi
    else
       echo "$1 is already installed."
       INSTALL_STATUS_ARR+=("$1 is already installed.")
    fi
}

function pip_package_exists_lin_fn() {
    pip3 show "$1" &> /dev/null
    return $?
}

function pip_install_package_fn()
{
    if ! pip_package_exists_lin_fn $1 ; then
        echo "installing $1"
        sudo pip3 install $1
    fi
}


function install_pkgs_linux_fn()
{
    sudo apt update
    install_package_fn git
    install_package_fn cmake
    install_package_fn gcc
    install_package_fn g++
    install_package_fn libcurl4-openssl-dev
    install_package_fn libgstreamer1.0-dev
    install_package_fn libgstreamer-plugins-bad1.0-dev
    install_package_fn libssl-dev
    install_package_fn libxml2-dev
    install_package_fn pkg-config
    install_package_fn zlib1g-dev
    install_package_fn libreadline-dev
    install_package_fn libgstreamer-plugins-base1.0-dev
    install_package_fn gstreamer1.0-libav
    install_package_fn lcov
    install_package_fn gcovr
    install_package_fn libcjson-dev
    install_package_fn curl
    install_package_fn xz-utils
    install_package_fn freeglut3-dev
    install_package_fn build-essential
    install_package_fn libglew-dev
    install_package_fn libboost-all-dev
    install_package_fn ninja-build
    install_package_fn libwebsocketpp-dev
    install_package_fn libjansson-dev
    install_package_fn libwayland-dev
    install_package_fn libxkbcommon-dev
    install_package_fn libfontconfig-dev
    install_package_fn libharfbuzz-dev
    install_package_fn snapd
    install_package_fn libcppunit-dev
    install_package_fn wayland-protocols
    install_package_fn libjsoncpp-dev
    install_package_fn libasio-dev
    install_package_fn libsystemd-dev
    install_package_fn jq
    install_package_fn libtinyxml2-dev
    install_package_fn openjdk-21-jre-headless
    install_package_fn libglib2.0-dev

    VER=$(grep -oP 'VERSION_ID="\K[\d.]+' /etc/os-release)

    if [ ${VER:0:2} -ge 24 ]; then
        install_package_fn libjavascriptcoregtk-4.1-dev
        install_package_fn meson-1.5
    elif [ ${VER:0:2} -ge 22 ]; then
        install_package_fn libjavascriptcoregtk-4.1-dev
        # Install and verify the version of meson
        install_package_fn python3-pip
        pip_install_package_fn meson

        MESON_VERSION=$(meson --version)
        if $(dpkg --compare-versions "${MESON_VERSION}" lt "1.4.0"); then
            echo ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>"
            echo "Meson version ${MESON_VERSION} is not supported"
            echo "Please uninstall and use version 1.4.0 or later"
            echo " sudo apt remove meson"
            echo " sudo pip3 install meson"
            echo " hash -r"
            echo ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>"
            return 1
        fi
    elif [ ${VER:0:2} -eq 20 ]; then
        install_package_fn libjavascriptcoregtk-4.0-dev
        install_package_fn python3-pip
        pip_install_package_fn meson
    elif [ ${VER:0:2} -eq 18 ]; then
        install_package_fn libjavascriptcoregtk-4.0-dev
        install_package_fn python3-pip
        pip_install_package_fn meson
    else
        echo "Please upgrade your Ubuntu version to at least 20:04 LTS. OS version is $VER"
        return 1
    fi
}

function install_asio_fn()
{
    cd ${LOCAL_DEPS_BUILD_DIR}
    if [ ! -d asio-1.18.2 ]; then
        echo "Installing asio"
        curl -o asio-1.18.2.tar.gz "https://excellmedia.dl.sourceforge.net/project/asio/asio/1.18.2%20%28Stable%29/asio-1.18.2.tar.bz2?viasf=1"
        tar -xf asio-1.18.2.tar.gz
        pushd asio-1.18.2
        mkdir build && cd build
        ../configure
        make -j$(nproc)
        sudo make install
        popd
        INSTALL_STATUS_ARR+=("asio was successfully installed.")
    else
        echo "asio is already installed."
        INSTALL_STATUS_ARR+=("asio was already installed.")
    fi
}

function install_pkgs_fn()
{
  if [[ "$OSTYPE" == "darwin"* ]]; then

      #Check/Install brew
      which -s brew
      if [[ $? != 0 ]] ; then
          echo "Installing homebrew"
          /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
          # TODO: Need to add to path or have user do it and fail this script
      else
          echo "Updating homebrew"
          brew update
      fi

      install_pkgs_darwin_fn git glib json-glib cmake "openssl@3" libxml2 ossp-uuid cjson gnu-sed meson ninja pkg-config jsoncpp lcov gcovr jq curl
      install_pkgs_darwin_fn coreutils websocketpp "boost@1.85" jansson libxkbcommon cppunit gnu-sed fontconfig doxygen graphviz tinyxml2 openldap krb5 "openjdk@21"

      if [[ "$ARCH" == "arm64" && "$CUR_MACOS_VER" == "15.5" ]]; then
          # Install downgraded version of asio for arm64.
          # This is to fix compatibility issues with websocketpp and subtec
          # "|| true" prevents the script from exiting if orc is not found, that is not an error
          ASIO_FOUND=`brew list | grep -i asio | wc -l` || true
          if [ "${ASIO_FOUND}" -gt 0 ]; then
              read -p "Found ASIO, remove ASIO package (Y/N)" remove_asio
              case $remove_asio in
                [Yy]* ) brew remove -f --ignore-dependencies asio
                    echo "Installing asio 1.18.2 after removal"
                    install_asio_fn
                    ;;
                * ) echo "Exiting without removal ..."
                    return 1
                    ;;
              esac
          else
              echo "Installing asio 1.18.2, as no asio found"
              install_asio_fn
          fi
      else
          # Install asio standalone on other platforms
          install_pkgs_darwin_fn asio
      fi

      #L1 dependency, we don't build this so treat like an installed pkg
      cd ${LOCAL_DEPS_BUILD_DIR}
      if [ ! -d ./systemd ]; then
          echo "Installing systemd (required for L1 tests)"
          do_clone_fn https://github.com/systemd/systemd.git
      else
          echo "systemd is already installed."

      fi


  elif [[ "$OSTYPE" == "linux"* ]]; then  
      install_pkgs_linux_fn
  fi
}
