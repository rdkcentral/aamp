#!/bin/bash

aampdir=$PWD/../..
linuxbuilddir=$aampdir/Linux

export LD_LIBRARY_PATH=${linuxbuilddir}/lib
export RIALTO_SESSION_SERVER_PATH=${linuxbuilddir}/bin/RialtoServer
export SESSION_SERVER_ENV_VARS="DISPLAY=:0"
${linuxbuilddir}/bin/RialtoServerManagerSim
