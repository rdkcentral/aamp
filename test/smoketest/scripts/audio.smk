#######################################################################################
# Audio languages that are in https://cpetestutility.stb.r53.xcal.tv/multilang/main.mpd:
#[{
#	"name": "5",
#	"language":     "ger",
#	"codec":        "mp4a.40.2",
#	"rendition":    "german",
#	"bandwidth":    288000,
#	"Type": "audio",
#	"availability": true
#}, {
#	"name": "6",
#	"language":     "eng",
#	"codec":        "mp4a.40.2",
#	"rendition":    "english",
#	"bandwidth":    288000,
#	"Type": "audio",
#	"availability": true
#}, {
#	"name": "7",
#	"language":     "spa",
#	"codec":        "mp4a.40.2",
#	"rendition":    "spanish",
#	"bandwidth":    288000,
#	"Type": "audio",
#	"availability": true
#}, {
#	"name": "0",
#	"language":     "fra",
#	"codec":        "mp4a.40.2",
#	"rendition":    "french",
#	"bandwidth":    288000,
#	"Type": "audio",
#	"availability": true
#}, {
#	"name": "8",
#	"language":     "pol",
#	"codec":        "mp4a.40.2",
#	"rendition":    "polish",
#	"bandwidth":    288000,
#	"Type": "audio",
#	"availability": true
#}]

# Set the tune URL
SET_VAR_TAGS(TUNE_URL) https://cpetestutility.stb.r53.xcal.tv/multilang/main.mpd




######################################################################
# First tests check that the config settings work as expected
#

# Set a list of existing preferred languages and check first is chosen
config preferredAudioLanguage(spa, ger, fra)
tune $TUNE_URL
waitfor 5 TUNED PLAYING
check audio(language:spa)
sleep 5
stop

# Set a list of preferred languages first one not existing and check second is chosen
config preferredAudioLanguage(rus, fra, pol)
tune $TUNE_URL
waitfor 5 TUNED PLAYING
check audio(language:fra)
sleep 5
stop

# Set an existing preferred language and a different rendition and check the language is chosen
config preferredAudioLanguage(spa) preferredAudioRendition(french)
tune $TUNE_URL
waitfor 5 TUNED PLAYING
check audio(language:spa)
sleep 5
stop

# Set an non-existing preferred language and a different rendition and check the rendition is chosen
config preferredAudioLanguage(rus) preferredAudioRendition(spanish)
tune $TUNE_URL
waitfor 5 TUNED PLAYING
check audio(language:spa, rendition:spanish)
sleep 5
stop

# Set a list of languages with only the last existing and a list of renditions and check the existing
# language is chosen
config preferredAudioLanguage(rus, czk, eng) preferredAudioRendition(russian, spanish)
tune $TUNE_URL
waitfor 5 TUNED PLAYING
check audio(language:eng)
sleep 5
stop

# Set a list of languages that do not exist and a renditions that does and check the rendition is chosen
config preferredAudioLanguage(rus, czk, swe) preferredAudioRendition(spanish)
tune $TUNE_URL
waitfor 5 TUNED PLAYING
check audio(rendition:spanish)
sleep 5
stop

# NOTE - we do not currently support a list of renditions
## Set a list of languages that do not exist and a list of renditions where only the last one exists and 
## check the existing rendition is chosen
#config preferredAudioLanguage(rus, czk, swe) preferredAudioRendition(russian, spanish)
#tune $TUNE_URL
#waitfor 5 TUNED PLAYING
#check audio(rendition:spanish)
#sleep 5
#stop



###########################################################################
# Set of tests to switch audio track while playing by specifying audio 
# language, rendition etc.:
#   - configure check for AUDIO_TRACKS_CHANGED (non-blocking)
#     (before setaudiotrack() as we get the notification during setting the track))
#   - set the track
#   - (allow up to 2s to) check for AUDIO_TRACKS_CHANGED
#   - check the selected audio track is set

# Default audio track to english
config preferredAudioLanguage(eng)

# Tune and check that the audio track selected is 'eng'
tune $TUNE_URL
waitfor 5 TUNED PLAYING
check audio(language:eng)
sleep 5

# Select a new audio track by language:
waitfor 0 AUDIO_TRACKS_CHANGED
setaudiotrack language:spa
waitfor 2
check audio(language:spa)
sleep 5

# Select a new audio track by rendition:
waitfor 0 AUDIO_TRACKS_CHANGED
setaudiotrack language:rus rendition:french
waitfor 2
check audio(language:fra, rendition:french)
sleep 5




# Repeat tests but select tracks by index:

waitfor 0 AUDIO_TRACKS_CHANGED
setaudiotrack 0
waitfor 2
check audio(language:ger, track:0)
sleep 5

waitfor 0 AUDIO_TRACKS_CHANGED
setaudiotrack 2
waitfor 2
check audio(language:spa, track:2)
sleep 5

waitfor 0 AUDIO_TRACKS_CHANGED
setaudiotrack 4
waitfor 2
check audio(language:pol, track:4)
sleep 5

# Same track id
waitfor 0 !AUDIO_TRACKS_CHANGED
setaudiotrack 4
waitfor 2
check audio(language:pol, track:4)
sleep 5

# Invalid track id
waitfor 0 !AUDIO_TRACKS_CHANGED
setaudiotrack 5
waitfor 2
check audio(language:pol, track:4)
sleep 5

stop


