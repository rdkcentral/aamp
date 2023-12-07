
transcode.py

This tool provides the ability to substitute (by default) encrypted content
within a download stream using transcoded clear content from a supplied file.

See ../harvest/README.txt for example usage

 ===================================================================================
 
 Wrote new module :
~/aamp/test/tools/replace_segments/generate_harvest_details.py

This module is used to generate harvest_details.json which is required for
transcode.py

Also it checks the harvested segments and manifest. If any of the representation is
not harvested then script will remove that representation from manifest files so that
404 errors while playback can be reduced.

Module scans manifest (mpd) files ending with numerical value. Example :
manifest.mpd.1 or manifest.mpd.25 etc.

{"recording_start_time": "2023-10-04T08:19:20.902778", "args": ["../harvest.py", "-
b", "562800", "-m", "1800", "/edge-mm.spectrum.net/linear.stvacdn.spectrum.com/LIVE/2
002/bpk-tv/04755/drm/manifest.mpd"], "url": "/edge-mm.spectrum.net/linear.stvacdn.spectrum.com/LIVE/2002/bpk-tv/04755/drm/manifest.mpd", "contains_ad_segments": false}
 ===================================================================================
 transcode.py (~/aamp/test/tools/replace_segments/transcode.py)
 
Some manifest contains BaseURL.

Transcoder uses this BaseURL to get segments. If the BaseURL contains
production endpoint. transcode.py tries to fetch segments from production server
instead of harvested segments.

If we remove the http:// or https:// from BaseURL will get a directory wherein all segment files are available.

Patch is added to generate harvest_details.json if the same is not found and to remove http:// or https:// from segment names.

python ~/comcast/aamp/test/tools/replace_segments/transcode.py -a --transcode /home/m
utsl02618/Desktop/doner_videos/big_buck_bunny_720p_surround.mp4 > ./transcode.log
 ===================================================================================
RDKAAMP-1645 Modify transcoder to not restart donor video at start of each period and not reset time stamp in overlay

1 Modified mp4_tools.py.
   - Code enhanced with new method get_video_duration(file_path), to get the duration of an AV file.
   - Code enhanced with new method mesr_time_adj(donor_file, tot_dur_pids, cur_sample_dur), to skip the start position on donor.
   - It also checks that the new period starts is longer/shorter than the leftout part of donor; and if the period is longer than doner then what is the difference value.
   - Code enhanced with new method tot_dur_format(cur_tot_dur), to get required Time Format.
   - Code also enhanced to focus on Period change; and handle the Audio/Video media types separately to address correct new start time on donor.

2 Modified transcode.py.
   - Enhanced with new method "get_video_duration(file_path)".
   - Code enhanced to increase the size of Donor file & manage relative files/folders.
   - Enhanced temp files/folders clean up.
   
3 Modified segment_audio.sh - ffmpeg command enhanced 
   - Handles scenario when harvested content smaller than donor.
   - Handles scenario when harvested content larger than donor - new period starts & it is greater than remaining part of donor.
   - Handles scenario when harvested content larger than donor - current period not yet ended & donor needs to retart from beginning.

4 Modified segment_video.sh - ffmpeg command enhanced 
   - Handles scenario when harvested content smaller than donor.
   - Handles scenario when harvested content larger than donor - new period starts & it is greater than remaining part of donor.
   - Handles scenario when harvested content larger than donor - current period not yet ended & donor needs to retart from beginning.
 =======================================================================================
