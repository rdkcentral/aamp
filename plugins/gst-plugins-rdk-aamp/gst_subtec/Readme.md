# Subtec GStreamer Plugin for sending subtitles to Subtec

Subtitles can be sent from AAMP directly to the Subtec Application or via this GStreamer plugin.
Sending via the GStreamer plugin is enabled in AAMP through config "gstSubtecEnabled", which is
disabled by default in the AAMP simulator builds.

## Building and installing the Subtec Gstreamer Plugin for AAMP Simulator

### For Ubuntu

From the aamp folder run:

cmake -Bbuild/gst_subtec -H./Linux/gst-plugins-rdk-aamp/gst_subtec -DCMAKE_INSTALL_PREFIX=./Linux -DCMAKE_PLATFORM_UBUNTU=1
make -C build/gst_subtec
make -C build/gst_subtec install

### For macOS

From the aamp folder run:

PKG_CONFIG_PATH=/Library/Frameworks/GStreamer.framework/Versions/1.0/lib/pkgconfig/ cmake -Bgst_subtec -H./.libs/gst-plugins-rdk-aamp/gst_subtec -DCMAKE_INSTALL_PREFIX=./Debug
make -C build/gst_subtec
make -C build/gst_subtec install
