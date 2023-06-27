#!/bin/bash


function package_exists_lin() {
    dpkg -s "$1" &> /dev/null
    return $?
}

function install_package() {
    if ! package_exists_lin $1 ; then
        echo "installing $1"
        sudo apt install $1 -y
    fi
}

function pip_package_exists_lin() {
    pip3 show "$1" &> /dev/null
    return $?
}

function pip_install_package() {
    if ! pip_package_exists_lin $1 ; then
        echo "installing $1"
        sudo pip3 install $1
    fi
}

sudo apt update
install_package git
install_package cmake
install_package gcc
install_package g++
install_package libcurl4-openssl-dev
install_package libgstreamer1.0-dev 
install_package libgstreamer-plugins-bad1.0-dev 
install_package libssl-dev 
install_package libxml2-dev 
install_package pkg-config 
install_package zlib1g-dev
install_package libreadline-dev
install_package libgstreamer-plugins-base1.0-dev
install_package gstreamer1.0-libav
install_package lcov
install_package gcovr
install_package libcjson-dev
install_package curl
install_package xz-utils
install_package freeglut3-dev
install_package build-essential
install_package libglew-dev
install_package libboost-all-dev
install_package ninja-build
install_package libwebsocketpp-dev
install_package libjansson-dev
install_package libwayland-dev
install_package libxkbcommon-dev
install_package libfontconfig-dev
install_package libharfbuzz-dev
install_package snapd
install_package libcppunit-dev
install_package wayland-protocols
install_package libcppunit-dev

ver=$(grep -oP 'VERSION_ID="\K[\d.]+' /etc/os-release)

if [ ${ver:0:2} -eq 22 ]; then
	# Install and verify the version of meson
	install_package python3-pip
	pip_install_package meson

	mesonversion=$(meson --version)
	if $(dpkg --compare-versions "${mesonversion}" lt "1.2.3"); then
	    echo ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>"
	    echo "Meson version ${mesonversion} is not supported"
	    echo "Please uninstall and use version 1.2.3 or later"
	    echo " sudo apt remove meson"
	    echo " sudo pip3 install meson"
	    echo " hash -r"
	    echo ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>"
	    exit 1
	fi

	# Temporary check for gcc version as subtec currently fails to build with gcc-11
	# on 22.04. Once the build issues have been fixed this can be removed
	gccver=$(g++ --version | awk '{ print $NF }' | head -n 1 | cut -d. -f1)
	if [ $gccver -eq 11 ]; then
	    echo ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>"
	    echo "Currently subtec will not build with gcc-11"
	    echo "Please install and use gcc-9 as follows:"
	    echo " sudo apt update"
	    echo " sudo apt install g++-9 -y"
	    echo " sudo update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-9 10"
	    echo ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>"
	    exit 1
	fi
elif [ ${ver:0:2} -eq 20 ]; then
	install_package python3-pip
	pip_install_package meson
elif [ ${ver:0:2} -eq 18 ]; then
	install_package python3-pip
	pip_install_package meson
else
	echo "Please upgrade your Ubuntu version to at least 20:04 LTS. OS version is $ver"
fi
