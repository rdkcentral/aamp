#!/usr/bin/env bash


function aampdeps_install_build_fn() {
    
    cd $LOCAL_DEPS_BUILD_DIR
    
    # OPTION_CLEAN == true
    if [ $1 = true ] ; then
        echo " clean"
        rm -rf aampabr
        rm -rf aampmetrics
    fi


   # Install 
    if [ -d "aampabr" ]; then
        echo "aampabr is already installed"
        INSTALL_STATUS_ARR+=("aampabr was already installed.")
    else
        do_clone_rdk_repo_fn ${OPTION_AAMP_BRANCH} aampabr

        #Build aampabr
        echo "Building aampabr..."
        pwd
        pushd aampabr
        mkdir -p build 
        cd build
        cmake .. -DCMAKE_INSTALL_PREFIX=${LOCAL_DEPS_BUILD_DIR} -DCMAKE_MACOSX_RPATH=TRUE
        make
        make install
        popd
        INSTALL_STATUS_ARR+=("aampabr was successfully installed.")
    fi

    if [ -d "aampmetrics" ]; then
        echo "aampmetrics is already installed"
        INSTALL_STATUS_ARR+=("aampmetrics was already installed.")
    else
        do_clone_rdk_repo_fn ${OPTION_AAMP_BRANCH} aampmetrics

        #Build aampmetrics
        echo "Building aampmetrics..." 
        pushd aampmetrics
        mkdir -p build
        cd build
        # CMAKE_CXX_FLAGS may not be needed on Linux, can also be removed when aampmetrics uses LIBCJSON_LINK_LIBRARIES as it should.
        cmake .. -DCMAKE_INSTALL_PREFIX=${LOCAL_DEPS_BUILD_DIR} -DCMAKE_MACOSX_RPATH=TRUE -DCMAKE_CXX_FLAGS="-L $LOCAL_DEPS_BUILD_DIR/lib"
        make    
        make install
        popd
        INSTALL_STATUS_ARR+=("aampmetrics was successfully installed.")
    fi 

}
