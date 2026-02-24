#!/usr/bin/env bash

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

# default values
OPTION_AAMP_BRANCH="dev_sprint_25_2"
OPTION_MIDDLEWARE_PLAYER_INTERFACE_COMMIT_ID="231dbb281a4a920a19dec01665e3e43f14d1902d"
OPTION_PLAYER_INTERFACE_SOURCE="internal"
OPTION_BUILD_DIR=""
OPTION_BUILD_ARGS=""
OPTION_CLEAN=false
OPTION_COVERAGE=false
OPTION_NET_TRACE=false
OPTION_DONT_RUN_AAMPCLI=false
OPTION_PROTOBUF_REFERENCE="3.11.x"
OPTION_QUICK=false
OPTION_RIALTO_REFERENCE="v0.2.2"
OPTION_RIALTO_BUILD=false
OPTION_SUBTEC_SKIP=false
OPTION_AAMPCLIKOTLIN_SKIP=true
OPTION_SUBTEC_BUILD=true
OPTION_SUBTEC_CLEAN=false
OPTION_CLEAN_BUILD=false
OPTION_GOOGLETEST_REFERENCE="tags/release-1.11.0"
OPTION_UBUNTU_SANITIZER=false

function install_options_fn()
{
  
  # Parse long-form options first to avoid getopts limitations
  local remaining_args=()
  while [[ $# -gt 0 ]]; do
    case $1 in
      --player-interface-source=*)
        OPTION_PLAYER_INTERFACE_SOURCE="${1#*=}"
        if [[ "${OPTION_PLAYER_INTERFACE_SOURCE}" != "internal" && "${OPTION_PLAYER_INTERFACE_SOURCE}" != "external" ]]; then
          echo "Error: --player-interface-source must be 'internal' or 'external'"
          return 1
        fi
        echo "Player interface source: ${OPTION_PLAYER_INTERFACE_SOURCE}"
        ;;
      --middleware-player-interface-commit-id=*)
        OPTION_MIDDLEWARE_PLAYER_INTERFACE_COMMIT_ID="${1#*=}"
        echo "Middleware player interface commit ID: ${OPTION_MIDDLEWARE_PLAYER_INTERFACE_COMMIT_ID}"
        ;;
      *)
        remaining_args+=("$1")
        ;;
    esac
    shift
  done

  # Set remaining arguments for getopts processing
  set -- "${remaining_args[@]}"

  # Reset OPTIND to ensure getopts starts from $1
  OPTIND=1

  # Parse optional command line parameters
  while getopts ":d:b:cef:np:r:g:qsktu" OPT; do
    case ${OPT} in
      d ) # process option d install base directory name
        OPTION_BUILD_DIR=${OPTARG}
        echo "${OPTARG}"
        ;;
      b ) # process option b code branch name
        OPTION_AAMP_BRANCH=${OPTARG}
        ;;
      c ) # process option c coverage
        OPTION_COVERAGE=ON
        echo coverage "${OPTION_COVERAGE}"
        ;;
      e ) # process option e enable network trace
        OPTION_NET_TRACE=ON
        echo "Network trace enabled: ${OPTION_NET_TRACE}"
        ;;
      f )# process option f to get compiler flags
         # add flags for cmake build by splitting buildargs with separator ','
        OPTION_BUILD_ARGS=${OPTARG}
        echo "Additional build flags specified '${OPTARG}'"
        ;;
      g )# process option f to get googletest revision to build
        OPTION_GOOGLETEST_REFERENCE=${OPTARG}
        echo "${OPTARG}"
        ;;
      n )# process option n not to run aamp-cli on MAC
        OPTION_DONT_RUN_AAMPCLI=true
        echo "Skip AAMPCli : ${DONTRUNAAMPCLI}"
        ;;
      q )# quick option, skips installed (not built) dependency checks
        OPTION_QUICK=true
        echo "Skip AAMPCli : ${QUICK}"
        ;;
      r )
        OPTION_RIALTO_REFERENCE=${OPTARG}
        echo "rialto tag : ${RIALTO_REFERENCE}"
        ;;
      s )
        OPTION_SUBTEC_SKIP=true
        # overrides any subtec or subtec clean setting
        echo "Skip subtec: ${OPTION_SKIP_SUBTEC}"
        ;;
      u )
        OPTION_UBUNTU_SANITIZER=true
        echo "Enable Ubuntu sanitizer: ${OPTION_UBUNTU_SANITIZER}"
        ;;
      k )
        OPTION_AAMPCLIKOTLIN_SKIP=false
        echo "Skip aamp-cli on Kotlin: ${OPTION_AAMPCLIKOTLIN_SKIP}"
        ;;
      p )
        OPTION_PROTOBUF_REFERENCE=${OPTARG}
        echo "protobuf branch : ${PROTOBUF_REFERENCE}"
        ;;
      t )
        OPTION_CLEAN_BUILD=true
        echo "Will remove .libs and build directories before build"
        ;;
      * )
        echo "'Usage: No flags/options specified - build AAMP with default options
        [-b] Specify aamp branch name (default: current sprint branch)
        [-d] Local setup directory name (default: current working directory)
        [-c] Test coverage scan on
        [-e] Enable network activity tracing (ENABLE_AAMP_NET_TRACE)
        [-f] Add compiler flags (e.g., -f \"-DENABLE_SOMETHING=ON\")
        [-g] Specify gtest release test to be built. Default - tags/release-1.11.0
        [-q] Quick build, skips installed (not built) dependency checks

        [-s] Skip subtec build and installation]"
        echo "        Note:  Subtec is built by default but can be rebuilt separately with the subtec
        [-k] Build aamp-cli Kotlin module (Linux and MacOS only)]
        [--player-interface-source=internal|external] Choose player interface source (default: internal)
        [--middleware-player-interface-commit-id=<commit>] Specify commit ID when using external (default: 231dbb281a4a920a19dec01665e3e43f14d1902d)
        [-t] Remove .libs and build directories before build (full rebuild)
        [-u] Enable Ubuntu address sanitizer (Linux only)"

        echo "
        [-r] Specify rialto to be built
        [-p] Specify protobuf branch name] (Linux only)"

        echo "        Note:  Rialto is built with the 'rialto' option. Use '-r' to set the rialto tag, "
        echo "       '-p' to set the Protobuf branch used for Rialto (Linux only)."
        return 1
        ;;
    esac
  done

  # Shift off all processed option arguments, leaving only positional args
  shift $((OPTIND - 1))

  # Parse project clean first, allows for subtec [clean]
  if [[ ${1} = "clean" ]]; then
      OPTION_CLEAN=true
      shift
  fi

  # Parse subtec options
  if  [[  ${1} = "subtec" ]]; then
    OPTION_SUBTEC_BUILD=true
    shift
    if  [[  ${1} = "clean" ]]; then
        OPTION_SUBTEC_CLEAN=true
        shift
    fi
  fi

  if  [[ ${1} = "rialto" ]]; then
    OPTION_RIALTO_BUILD=true
    shift
  fi

}

