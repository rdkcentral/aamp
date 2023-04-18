# Dockerfile for generating:
#     - Aamp 
#     - Subtec 
# build environments, including test environment for:
#     - GoogleTest/GoogleMock 
#     - CPPUnit
#
# Build instructions:
#     docker pull ubuntu:20.04
#     docker build <dir containing this file>
#
# Docker Image stored at:
# aamp-docker.artifactory.comcast.com/aamp-default
#
FROM ubuntu:20.04

ARG DEBIAN_FRONTEND=noninteractive

# Following libs were added for subtec build & test:
#   libwebsocketpp-dev \
#   libjansson-dev \
#   libfontconfig-dev \
#   libharfbuzz-dev \
#   libxkbcommon-dev \
#   libwayland-dev \
#   libboost-all-dev \
#   libcppunit-dev \
#   lcov
RUN apt-get update && apt-get --no-install-recommends install -y \
    git \
    cmake \
    gcc \
    g++ \
    libgtest-dev \
    libgmock-dev \
    libcurl4-openssl-dev \
    libgstreamer1.0-dev \
    gstreamer1.0-plugins-base \
    libgstreamer-plugins-bad1.0-dev \
    libssl-dev \
    libxml2-dev \
    pkg-config \
    zlib1g-dev \
    sudo \
    make \
    libglew-dev \
    freeglut3-dev \
    patch \
    libreadline-dev \
    libwebsocketpp-dev \
    libjansson-dev \
    libfontconfig-dev \
    libharfbuzz-dev \
    libxkbcommon-dev \
    libwayland-dev \
    libboost-all-dev \
    libcppunit-dev \
    lcov \
    python3-pip \
    && echo '<<<<<<< install pipreqs for python3 >>>>>>>>>' && \
    pip3 install pipreqs


