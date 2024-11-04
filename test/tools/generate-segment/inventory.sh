mkdir -p out

#                                             codec    time duration timescale number        media            init      source
bash generate-audio-segment.sh                  aac       0    85760     44100      1  out/audio-1.m4s  out/audio-init.m4s silence.wav
bash generate-audio-segment.sh                  aac   85760    85760     44100      2  out/audio-2.m4s  out/audio-init.m4s silence.wav
bash generate-audio-segment.sh                  aac  171520    85760     44100      3  out/audio-3.m4s  out/audio-init.m4s silence.wav
bash generate-audio-segment.sh                  aac  257280    85760     44100      4  out/audio-4.m4s  out/audio-init.m4s silence.wav
bash generate-audio-segment.sh                  aac  343040    85760     44100      5  out/audio-5.m4s  out/audio-init.m4s silence.wav
bash generate-audio-segment.sh                  aac  428800    85760     44100      6  out/audio-6.m4s  out/audio-init.m4s silence.wav
bash generate-audio-segment.sh                  aac  514560    85760     44100      7  out/audio-7.m4s  out/audio-init.m4s silence.wav
bash generate-audio-segment.sh                  aac  600320    85760     44100      8  out/audio-8.m4s  out/audio-init.m4s silence.wav
bash generate-audio-segment.sh                  aac  686080    85760     44100      9  out/audio-9.m4s  out/audio-init.m4s silence.wav
bash generate-audio-segment.sh                  aac  771840    85760     44100      10 out/audio-10.m4s out/audio-init.m4s silence.wav

#                                 w    h fps  codec    time duration timescale number        media            init      source
bash generate-video-segment.sh 1920 1080  50   hevc       0    92160     48000      1  out/video-1.m4s  out/video-init.m4s testpat.jpg
bash generate-video-segment.sh 1920 1080  50   hevc   92160    92160     48000      2  out/video-2.m4s  out/video-init.m4s testpat.jpg
bash generate-video-segment.sh 1920 1080  50   hevc  184320    92160     48000      3  out/video-3.m4s  out/video-init.m4s testpat.jpg
bash generate-video-segment.sh 1920 1080  50   hevc  276480    92160     48000      4  out/video-4.m4s  out/video-init.m4s testpat.jpg
bash generate-video-segment.sh 1920 1080  50   hevc  368640    92160     48000      5  out/video-5.m4s  out/video-init.m4s testpat.jpg
bash generate-video-segment.sh 1920 1080  50   hevc  460800    92160     48000      6  out/video-6.m4s  out/video-init.m4s testpat.jpg
bash generate-video-segment.sh 1920 1080  50   hevc  552960    92160     48000      7  out/video-7.m4s  out/video-init.m4s testpat.jpg
bash generate-video-segment.sh 1920 1080  50   hevc  645120    92160     48000      8  out/video-8.m4s  out/video-init.m4s testpat.jpg
bash generate-video-segment.sh 1920 1080  50   hevc  737280    92160     48000      9  out/video-9.m4s  out/video-init.m4s testpat.jpg
bash generate-video-segment.sh 1920 1080  50   hevc  829440    92160     48000     10 out/video-10.m4s  out/video-init.m4s testpat.jpg
