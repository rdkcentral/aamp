# Subtec GStreamer Plugin for sending subtitles to Subtec

Subtitles can be sent from PLAYER directly to the Subtec Application or via this GStreamer plugin.
Sending via the GStreamer plugin is enabled in PLAYER through config "gstSubtecEnabled", which is
disabled by default in the PLAYER simulator builds.

## Building and installing the Subtec Gstreamer Plugin for PLAYER Simulator

### For Ubuntu

From the player(a_mp) folder run:

cmake -Bbuild/gst_subtec -H./Linux/gst-plugins/gst_subtec -DCMAKE_INSTALL_PREFIX=./Linux -DCMAKE_PLATFORM_UBUNTU=1
make -C build/gst_subtec
make -C build/gst_subtec install

### For macOS

From the player(a_mp) folder run:

PKG_CONFIG_PATH=/Library/Frameworks/GStreamer.framework/Versions/1.0/lib/pkgconfig/ cmake -Bgst_subtec -H./.libs/gst-plugins/gst_subtec -DCMAKE_INSTALL_PREFIX=./Debug
make -C build/gst_subtec
make -C build/gst_subtec install
