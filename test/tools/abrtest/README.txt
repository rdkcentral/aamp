Assumptions:
- fog and aamp checked out and built under ~/Documents/rdkdev/

Harvested, transcoded test content available under path specified in abrtest.sh

testhelper usage:

testhelper <pre> <mid> <size> <networkTimeout> <downloadLowBWTimeout> <fog>
Where 'pre' is millisecond delay before first bytes of requested file is served up.
Where 'mid' is millisecond value used for mid-download throttling.
Where 'networkTimeout' and 'downloadLowBWTimeout' are timeouts in seconds.
Where 'fog' if present tests playback using AAMP+FOG.
If final 'fog' omitted, tests AAMP-only playback.

Plays for ~30s, then archives results.

AAMP and FOG logs are filtered to compactly only include httpRequestEnd logs.
plot/index.html tool supports copy/paste and drag drop of logs; visualizes FOG & AAMP downloads within a unified timeline.
