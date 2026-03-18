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

AAMP_DIR="aamp-devenv-$(date +"%Y-%m-%d-%H-%M")"

# handle decision of where aamp repostitory/build should occur.
# result is script will be in correct aamp directory and OPTION_BUILD_DIR will be updated
function install_dir_fn()
{
  # If no build option provided, prompt for directory to use
  if [ -z ${OPTION_BUILD_DIR} ] ; then
      if [ -d "../aamp" ]; then
        ABS_PATH="$(cd "../aamp" && pwd -P)"
        echo ""
        while true; do
          read -p '[!Alert!] Install script identified that the aamp folder already exists @ ../aamp.
          Press Y, if you want to use same aamp folder (../aamp) for your simulator build.
          Press N, If you want to use separate build folder for aamp simulator. Press (Y/N)'  yn
          case $yn in
             [Yy]* ) AAMP_DIR=$ABS_PATH; echo "using following aamp build directory $AAMP_DIR"; break;;
             [Nn]* ) echo "using following aamp build directory $PWD/$AAMP_DIR"; break ;;
                 * ) echo "Please answer yes or no.";;
          esac
        done
      fi
  else
    echo "Using aamp build directory under $OPTION_BUILD_DIR";
    AAMP_DIR="${OPTION_BUILD_DIR}"
  fi

  if [[ ! -d "$AAMP_DIR" ]]; then
    echo "Creating aamp build directory under $AAMP_DIR";
    mkdir -p $AAMP_DIR
    cd $AAMP_DIR

    do_clone_rdk_repo_fn $OPTION_AAMP_BRANCH aamp
    cd aamp
    AAMP_DIR="$(pwd -P)"
  else
    cd $AAMP_DIR
  fi

  pwd
}
