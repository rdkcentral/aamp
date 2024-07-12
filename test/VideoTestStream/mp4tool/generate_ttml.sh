#!/bin/bash

generate_ttml_tracks() {
cd mp4tool
mkdir -p build
cd build
cmake ../
make
./ttml_gen "$1"

cp ttml_text_*.mp4 ../../dash/

}

