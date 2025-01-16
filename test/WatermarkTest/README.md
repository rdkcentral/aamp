# Setup
## On a pc
1. host the 'server' directory on a local web server e.g. from the directory containing this file:
	cd server; sudo python3 -m http.server 80
2. Update any ip addresses in the scripts contained in the wmScripts directory to match the PC webserver started in step 1.
3. Copy the wmScripts directory to the tmp directory of the device under test e.g.
	$ scp -P10022 -r wmScripts/ root@10.42.0.54:/opt/
	wmtest_testapp.sh                                                                              100% 1227   604.9KB/s   00:00
	wmonly.sh                                                                                      100% 1413    85.2KB/s   00:00
	wmjsShowReplaceHide.sh                                                                         100% 1171   640.6KB/s   00:00
	Config.sh                                                                                      100% 1420   909.2KB/s   00:00
	wmimage_1.png

## On the device under test
1. run /opt/wmScripts/wmonly.sh - if the watermark plugin is configured this should show a test video with a red '1' displayed over the top.  Note wmonly.sh directly uses the watermark plugin and does not test the JS interface.
2. if the red '1' was not visible during the previous step run /opt/wmScripts/Config.sh & repeat step 1.

# Positive Tests
Verify that Watermark JS interface works as expected on devices that support this feature.

## Check that the JS interface is available
### Method
check the logs for 'PersistentWatermark_LoadJS' log entries.

### Expected Results
	root@skyxione:~# egrep -i "PersistentWatermark_LoadJS" /opt/logs/*
	/opt/logs/sky-messages.log:2023 Apr 19 09:46:20.960152 WPEWebProcess[10195]:  [AAMP-JS] WARN : PersistentWatermark_LoadJS: 275 :PersistentWatermark:register persistent watermark class
	/opt/logs/sky-messages.log:2023 Apr 19 09:46:20.960405 WPEWebProcess[10195]:  [AAMP-JS] WARN : PersistentWatermark_LoadJS: 284 :PersistentWatermark:done with registering persistent watermark class
## Show-replace-hide Persistent Test
### Method
On the device:
1. run /opt/wmScripts/wmjsShowReplaceHidePersist.sh
2. power cycle the device (i.e. disconnect & reconnect the power lead, this is the best test of persistence as other methods of rebooting the system e.g. the reboot command should independantly sync the file system)
2. run /opt/wmScripts/wmjsShowReplaceHidePersist.sh (again)

### Expected Results
For each run through
1. On the first run through the last loaded watermark (if any) is shown for ~ 1 second.  On the second run through a blue '2' is displayed for ~1 second (as it is set by the first run through).
2. A blue '1' is displayed for ~1 second
3. A blue '2' is displayed for ~1 second
4. The blue '2' is removed (i.e. no watermark is shown).
5. The final console output is similar to that shown below for each run through.  On the first run through the value of the 'Persistent Metadata' entry may vary but on the second run through it should be 'Persist 2'(as it is set by the first run through).

```
root@skyxione:/opt/wmScripts# ./wmjsShowReplaceHidePersist.sh
{"jsonrpc":"2.0","id":4,"result":null}
{"jsonrpc":"2.0","id":4,"result":null}
{"jsonrpc":"2.0","id":4,"result":null}destroy
{"jsonrpc":"2.0","id":4,"result":{"success":true}}
launch
{"jsonrpc":"2.0","id":4,"result":{"launchType":"activate","success":true}}
May 24 02:26:49 skyxione /usr/bin/WPEFramework[26437]: [Wed, 24 May 2023 02:26:49 ]:[PluginServer.cpp:404]: Startup: Activated plugin [WebKitBrowser]:[htmlapp1]
May 24 02:26:50 skyxione WPEFramework[26437]: [htmlapp1]:9 Show Success!
May 24 02:26:50 skyxione WPEFramework[26437]: [htmlapp1]:36 Persistent Metadata: 'Persist 2'
May 24 02:26:51 skyxione WPEFramework[26437]: [htmlapp1]:39 updateAndShow 1
May 24 02:26:51 skyxione WPEFramework[26437]: [htmlapp1]:6 getAndCallback(): response successful
May 24 02:26:51 skyxione WPEFramework[26437]: [htmlapp1]:10 getAndCallback(): got array buffer of 19760 bytes
May 24 02:26:51 skyxione WPEFramework[26437]: [htmlapp1]:9 Show Success!
May 24 02:26:52 skyxione WPEFramework[26437]: [htmlapp1]:44 Metadata 1: 'Persist 1'
May 24 02:26:52 skyxione WPEFramework[26437]: [htmlapp1]:46 updateAndShow 2
May 24 02:26:52 skyxione WPEFramework[26437]: [htmlapp1]:6 getAndCallback(): response successful
May 24 02:26:52 skyxione WPEFramework[26437]: [htmlapp1]:10 getAndCallback(): got array buffer of 23878 bytes
May 24 02:26:52 skyxione WPEFramework[26437]: [htmlapp1]:9 Show Success!
May 24 02:26:53 skyxione WPEFramework[26437]: [htmlapp1]:51 Metadata 2: 'Persist 2'
May 24 02:26:53 skyxione WPEFramework[26437]: [htmlapp1]:53 hide
May 24 02:26:53 skyxione WPEFramework[26437]: [htmlapp1]:19 Hide Success!
done
```

## Show-replace-hide Volatile Test
### Method
On the device run /opt/wmScripts/wmjsShowReplaceHide.sh
### Expected Results
1. The last loaded watermark (if any) is shown for ~1 second (it should be a blue '2' if 'Show-replace-hide Persistent Test' has just completed successfully).  If this script is run immediatly following a reboot and a persistent watermark is present on the system this will be displayed here.
2. A red '1' is displayed for ~1 second
3. A red '2' is displayed for ~1 second
4. The red '2' is removed (i.e. no watermark is shown).
5. The final console output is similar to that below although the value of the 'Persistent Metadata' entry may vary (but it should be 'Persist 2' if 'Show-replace-hide Persistent Test' has just completed successfully).

```
root@skyxione:/opt/wmScripts# ./wmjsShowReplaceHide.sh
{"jsonrpc":"2.0","id":4,"result":null}
{"jsonrpc":"2.0","id":4,"result":null}
{"jsonrpc":"2.0","id":4,"result":null}destroy
{"jsonrpc":"2.0","id":4,"result":{"success":true}}
launch
{"jsonrpc":"2.0","id":4,"result":{"launchType":"activate","success":true}}
May 24 02:22:10 skyxione /usr/bin/WPEFramework[26437]: [Wed, 24 May 2023 02:22:10 ]:[PluginServer.cpp:404]: Startup: Activated plugin [WebKitBrowser]:[htmlapp1]
May 24 02:22:11 skyxione WPEFramework[26437]: [htmlapp1]:9 Show Success!
May 24 02:22:11 skyxione WPEFramework[26437]: [htmlapp1]:36 Persistent Metadata: 'Persist 2'
May 24 02:22:12 skyxione WPEFramework[26437]: [htmlapp1]:39 updateAndShow 1
May 24 02:22:12 skyxione WPEFramework[26437]: [htmlapp1]:6 getAndCallback(): response successful
May 24 02:22:12 skyxione WPEFramework[26437]: [htmlapp1]:10 getAndCallback(): got array buffer of 11108 bytes
May 24 02:22:12 skyxione WPEFramework[26437]: [htmlapp1]:9 Show Success!
May 24 02:22:13 skyxione WPEFramework[26437]: [htmlapp1]:44 Metadata 1: 'volatile 1'
May 24 02:22:13 skyxione WPEFramework[26437]: [htmlapp1]:46 updateAndShow 2
May 24 02:22:13 skyxione WPEFramework[26437]: [htmlapp1]:6 getAndCallback(): response successful
May 24 02:22:13 skyxione WPEFramework[26437]: [htmlapp1]:10 getAndCallback(): got array buffer of 21852 bytes
May 24 02:22:13 skyxione WPEFramework[26437]: [htmlapp1]:9 Show Success!
May 24 02:22:14 skyxione WPEFramework[26437]: [htmlapp1]:51 Metadata 2: 'volatile 2'
May 24 02:22:14 skyxione WPEFramework[26437]: [htmlapp1]:53 hide
May 24 02:22:14 skyxione WPEFramework[26437]: [htmlapp1]:19 Hide Success!
done
```

## Automated Tests
### Method
On the device run /opt/wmScripts/wmtest_testapp.sh.

## Expected Results
* Test grid shown
* tests should start (most will complete almost instantly the last few will take a few seconds)
* The red '1' watermark is briefly shown over the test grid & then hidden
* Final state of the table should show ResultSetup as complete and all other cells in the 'Result' column should read 'pass'

# Negative Test
Verify that Watermark JS interface is not available on devices that do not support this feature.

## Expected result
* The only message logged from 'PersistentWatermark_LoadJS' is: 'JS bindings not registered.'
* The positive tests detailed above fail (although the wmonly.sh script which does not use the js interface will work on some devices)


## Additional Persistence Tests
	preconditions, ensure that there is persistent data stored e.g. run above tests
	1. clear & powercycle (not reboot), check cleared
	2. update persistent & power cycle, check image display, crc & metadata
	3. update none persistent & power cycle, check identical results
