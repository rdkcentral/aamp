#!/usr/bin/env bash


function install_build_libdash_fn()
{
    cd $LOCAL_DEPS_BUILD_DIR

    # $OPTION_CLEAN == true
    if [ $1 = true ] ; then
        echo "libdash clean"
        if [ -d libdash ] ; then
            rm -rf libdash
            # uninstall?
            rm -rf $LOCAL_DEPS_BUILD_DIR/include/libdash
        fi
    fi

    if [ -d "libdash" ]; then
        echo "libdash is already installed"
        INSTALL_STATUS_ARR+=("libdash was already installed.")
    else
        echo "Installing libdash..."
        do_clone_fn  https://github.com/bitmovin/libdash.git

        cd libdash/libdash
        git checkout stable_3_0
        do_clone_fn -b rdk-next "https://code.rdkcentral.com/r/rdk/components/generic/rdk-oe/meta-rdk-ext"
        patch -p1 < meta-rdk-ext/recipes-multimedia/libdash/libdash/0001-libdash-build.patch
        patch -p1 < meta-rdk-ext/recipes-multimedia/libdash/libdash/0002-libdash-starttime-uint64.patch 
        patch -p1 < meta-rdk-ext/recipes-multimedia/libdash/libdash/0003-libdash-presentationTimeOffset-uint64.patch 
        patch -p1 < meta-rdk-ext/recipes-multimedia/libdash/libdash/0004-Support-of-EventStream.patch
        patch -p1 < meta-rdk-ext/recipes-multimedia/libdash/libdash/0005-DELIA-39460-libdash-memleak.patch
        patch -p1 < meta-rdk-ext/recipes-multimedia/libdash/libdash/0006-RDK-32003-LLD-Support.patch
        patch -p1 < meta-rdk-ext/recipes-multimedia/libdash/libdash/0007-DELIA-51645-Event-Stream-RawAttributes-Support.patch
        patch -p1 < meta-rdk-ext/recipes-multimedia/libdash/libdash/0008-DELIA-53263-Use-Label-TAG.patch
        patch -p1 < meta-rdk-ext/recipes-multimedia/libdash/libdash/0009-RDK-35134-Support-for-FailoverContent.patch
        patch -p1 < meta-rdk-ext/recipes-multimedia/libdash/libdash/0010-RDKAAMP-121-Failover-Tag-on-SegmentTemplate.patch
        patch -p1 < meta-rdk-ext/recipes-multimedia/libdash/libdash/0011-RDKAAMP-61-AAMP-low-latency-dash-stream-evaluation.patch	
        patch -p1 < meta-rdk-ext/recipes-multimedia/libdash/libdash/0012-To-retrieves-the-text-content-of-CDATA-section.patch
        mkdir -p build
        cd build
        cmake .. -DCMAKE_INSTALL_PREFIX=${LOCAL_DEPS_BUILD_DIR} -DCMAKE_MACOSX_RPATH=TRUE
        make
        make install

        # why doesn't make install do this for us
        cd ..
        mkdir -p $LOCAL_DEPS_BUILD_DIR/include/libdash
        mkdir -p $LOCAL_DEPS_BUILD_DIR/include/libdash/xml
        mkdir -p $LOCAL_DEPS_BUILD_DIR/include/libdash/mpd
        mkdir -p $LOCAL_DEPS_BUILD_DIR/include/libdash/network
        mkdir -p $LOCAL_DEPS_BUILD_DIR/include/libdash/portable
        mkdir -p $LOCAL_DEPS_BUILD_DIR/include/libdash/helpers
        mkdir -p $LOCAL_DEPS_BUILD_DIR/include/libdash/metrics
        cp -p libdash/include/* $LOCAL_DEPS_BUILD_DIR/include/libdash
        cp -p libdash/source/xml/*.h $LOCAL_DEPS_BUILD_DIR/include/libdash/xml
        cp -p libdash/source/mpd/*.h $LOCAL_DEPS_BUILD_DIR/include/libdash/mpd
        cp -p libdash/source/network/*.h $LOCAL_DEPS_BUILD_DIR/include/libdash/network
        cp -p libdash/source/portable/*.h $LOCAL_DEPS_BUILD_DIR/include/libdash/portable
        cp -p libdash/source/helpers/*.h $LOCAL_DEPS_BUILD_DIR/include/libdash/helpers
        cp -p libdash/source/metrics/*.h $LOCAL_DEPS_BUILD_DIR/include/libdash/metrics
        echo -e 'prefix='$LOCAL_DEPS_BUILD_DIR'/lib \nexec_prefix='$LOCAL_DEPS_BUILD_DIR' \nlibdir='$LOCAL_DEPS_BUILD_DIR'/lib \nincludedir='$LOCAL_DEPS_BUILD_DIR'/include/libdash \n \nName: libdash \nDescription: ISO/IEC MPEG-DASH library \nVersion: 3.0 \nRequires: libxml-2.0 \nLibs: -L${libdir} -ldash \nLibs.private: -lxml2 \nCflags: -I${includedir}' > $LOCAL_DEPS_BUILD_DIR/lib/pkgconfig/libdash.pc

        INSTALL_STATUS_ARR+=("libdash was successfully installed.")
    fi
}
