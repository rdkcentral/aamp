#!/bin/bash

# hack - ensure simlinear.py has all needed dependencies
cd ~/Documents/rdkdev/aamp/test/l2test/
./l2framework_testenv.sh
source l2venv/bin/activate
pip install pipreqs
pip install flask
pip install webargs
cd ~/Documents/rdkdev/aamp/test/tools/abrtest

testhelper() {
   pre=$1
   mid=$2
   size=$3
   networkTimeout=$4
   downloadLowBWTimeout=$5
   fog=$6
   
   echo launching simlinear

   # set working directory to harvested/transcoded content
   path="v1/frag/bmff/enc/cenc/t/SKWITHD_HD_SU_SKYUK_4066_0_6112559918033517163.mpd"
   cd ~/Downloads/skywitnessMP
   
   # start simlinear in background
   ~/Documents/rdkdev/aamp/test/tools/simlinear/simlinear.py --dash 8085 --throttle $pre:$mid:$size &
   
   # launch fog
   cd ~/Documents/rdkdev/fog/build/Debug
   ./fog-cli > ~/Documents/rdkdev/aamp/test/tools/abrtest/fog.log &

# give fog time to start up
   sleep 1
# run aampcli
   cd ~/Documents/rdkdev/aamp/build/Debug
   ./aamp-cli > ~/Documents/rdkdev/aamp/test/tools/abrtest/aamp.log <<EOSXXX
# enable aamp/fog logging and configure timeouts
setconfig {"info":true,"networkTimeout":$networkTimeout,"downloadLowBWTimeout":$downloadLowBWTimeout}
$fog http://127.0.0.1:8085/$path
# play for 30s
sleep 30000
stop
sleep 1000
exit
EOSXXX

   cd ~/Documents/rdkdev/aamp/test/tools/abrtest
   suffix=$pre-$mid-$networkTimeout-$downloadLowBWTimeout-$fog
   grep -w HttpRequestEnd  aamp.log > "aamp-$suffix.log"
   if [ "$fog" = "fog" ]; then
      grep -w HttpRequestEnd  fog.log > "fog-$suffix.log"
      cat "aamp-$suffix.log" "fog-$suffix.log" > "aampfog-$suffix.log"
      rm "fog-$suffix.log"
      rm "aamp-$suffix.log"
   fi

   pkill fog-cli

   # kill simlinear
   pkill Python
}

# aamp+fog
testhelper 200 15 2048 5 2 fog

# aamp-only
# testhelper 100 15 2048 5 2

