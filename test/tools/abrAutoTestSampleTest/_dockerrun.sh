#!/bin/bash
set -e

cat << DOCKERWHALE

                 ##        .
              ## ## ##       ==
           ## ## ## ##      ===
       /""""""""""""""""\___/ ===
  ~~~ {~~ ~~~~ ~~~ ~~~~ ~~ ~ /  ===- ~~~
       \______ o          __/
         \    \        __/
          \____\______/

DOCKERWHALE

echo "RDK AAMP Build and Test Environment"

chmod +x ./install-aamp.sh
./install-aamp.sh -d $(pwd -P) || true

if [  -f "./Linux/bin/aamp-cli" ]; then

   AAMP_HOME=$(pwd)
   pushd test/tools/abrAutoTestSampleTest
   pipreqs .
   pip3 install --no-cache-dir -r requirements.txt
   AAMP_HOME=$AAMP_HOME ./run_test.py
   popd

else
   echo "FAILED to build aamp-cli"
   exit 1
fi

# Exit the docker container
exit
