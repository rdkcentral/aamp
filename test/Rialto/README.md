# Rialto Test Server

AAMP can play content using the [Rialto](https://github.com/rdkcentral/rialto) test server on Ubuntu.

In a terminal window, build AAMP and Rialto on Ubuntu:

	$ cd aamp
	$ bash install-aamp.sh rialto

This takes several minutes longer than the normal AAMP build.

Run the Rialto server manager simulator on Ubuntu:

	$ cd test/Rialto
	$ ./startmanager.sh

In a second terminal window, start the Rialto test server:

	$ curl -X POST -d "" localhost:9008/SetState/YouTube/Active

Repeat this each time the server manager is started.

Configure AAMP to use Rialto, for example:

	$ cat ~/aamp.cfg
	useRialtoSink=true

Run AAMP, for example:

	$ cd aamp
	$ RIALTO_SOCKET_PATH=/tmp/rialto-0 \
	  LD_LIBRARY_PATH=$PWD/.libs/lib \
	  GST_PLUGIN_PATH=$PWD/.libs/lib/gstreamer-1.0 \
	  $PWD/build/aamp-cli \
	  https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main.mpd

The gstreamer test harness can also be run using the Rialto test server.
Ensure that the Rialto test server has been started as described above.
Then, for example:

	$ cd aamp
	$ RIALTO_SOCKET_PATH=/tmp/rialto-0 \
	  LD_LIBRARY_PATH=$PWD/.libs/lib \
	  GST_PLUGIN_PATH=$PWD/.libs/lib/gstreamer-1.0 \
	  $PWD/Linux/bin/gstTestHarness

Run some test harness commands, for example:

	path https://cpetestutility.stb.r53.xcal.tv/VideoTestStream
	abr

To verify that Rialto is used, create a dump of the pipeline and exit the test
harness:

	dump
	exit

Convert the generated pipeline dump file gst-test.dot into SVG format:

	$ dot -Tsvg gst-test.dot -o gst-test.svg

View this by double clicking on gst-test.svg using Files.
This should show that video is consumed by a RialtoMSEVideoSink element and
audio is consumed by a RialtoMSEAudioSink element.

## Troubleshooting

Previous versions of install-linux.sh may have created a Linux/bin folder with root ownership,
this should be removed prior to calling install-aamp.sh so that it will be re-created with user ownership;
the Rialto build copies the Rialto server to this folder.

Delete the gstreamer cache:

	$ rm ~/.cache/gstreamer-1.0/registry.x86_64.bin

Inspect the Rialto gstreamer sink plugins:

	$ cd aamp
	$ RIALTO_SOCKET_PATH=/tmp/rialto-0 \
	  LD_LIBRARY_PATH=$PWD/Linux/lib \
	  GST_PLUGIN_PATH=$PWD/Linux/lib/gstreamer-1.0 \
	  gst-inspect-1.0 rialtomsevideosink
	$ RIALTO_SOCKET_PATH=/tmp/rialto-0 \
	  LD_LIBRARY_PATH=$PWD/Linux/lib \
	  GST_PLUGIN_PATH=$PWD/Linux/lib/gstreamer-1.0 \
	  gst-inspect-1.0 rialtomseaudiosink

To see Rialto server logs:

	$ journalctl -f

Enable gstreamer logging, for example:

	$ export GST_DEBUG="2,*rialto*:6"

Enable Rialto logging, for example:

	$ export RIALTO_DEBUG=5
	$ export RIALTO_CONSOLE_LOG=1
