#!/usr/bin/env bash


function install_build_libcjson_fn()
{
    if [[ "$OSTYPE" == "darwin"* ]]; then

        # Should not be built or installed, but aampmetrics needs it in /usr/local
        # which is a problem on MacOS ARM64 as brew installs to /opt/homebrew now and
        # aampmetrics assumes/hardcodes -L/usr/local/lib
        # Once fixed by using ${LIBCJSON_LINK_LIBRARIES} instead of -lcjson this
        # script can be removed

        cd $LOCAL_DEPS_BUILD_DIR

        # $OPTION_CLEAN == true
        if [ $1 = true ] ; then
            echo "libcjson clean"
            if [ -d cJSON ] ; then
                rm -rf cJSON
                # uninstall?
                rm -rf $LOCAL_DEPS_BUILD_DIR/include/cjson
            fi
        fi

        if [ -d "cJSON" ]; then
            echo "libcjson is already installed"
            INSTALL_STATUS_ARR+=("libcjson was already installed.")
        else
            echo "Installing libcjson..."
            do_clone_fn https://github.com/DaveGamble/cJSON.git
            cd cJSON
            mkdir -p build
            cd build
            cmake .. -DCMAKE_INSTALL_PREFIX=${LOCAL_DEPS_BUILD_DIR}
            make
            make install

            INSTALL_STATUS_ARR+=("cjson was successfully installed.")
        fi

        if [ "$(uname -m)" = "arm64" ]; then

            SOURCE="/opt/homebrew/lib/libcjson.dylib"
            TARGET="/usr/local/lib/libcjson.dylib"

            if [ ! -f "$SOURCE" ]; then
                echo "cJSON not installed. Please install using "brew install cjson""
                exit 1
            fi

            if [ -L "$TARGET" ]; then
                echo "Required symlink $TARGET for arm64 already exists."

            else

                echo "Creating required symlink for arm64: $TARGET -> $SOURCE"
                sudo ln -s "$SOURCE" "$TARGET"
            fi


        fi

    elif [[ "$OSTYPE" == "linux"* ]]; then
        echo "libcjson is installed via apt on Linux targets."
    fi
}
