
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
