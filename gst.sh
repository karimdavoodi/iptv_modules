#!/bin/bash
D="-v --gst-debug-level=6"

TMM=/home/karim/Music/Video_Music/kitaro.mp4
TMP4=/home/karim/Music/test_h264_aac.mp4
TTS=/home/karim/Music/test_h264_aac.ts
TMP1=/home/karim/Music/test_mpeg1_mp2.mpeg
TMP2=/home/karim/Music/test_mpeg2_mp2.mpeg
TMKV=/home/karim/Music/test_h264_mp2.mkv
######## iptv_in_archive  ##########################################
gst-launch-1.0 --gst-debug-level=3  \
	dvbbasebin adapter=5 frequency=11996000 \
	program-numbers=32:22:23:24:25 polarity=H symbol-rate=27500  \
	delsys=dvb-s diseqc-source=1  \
	! filesink location=1.ts
	#modulation=qpsk \

######## iptv_in_archive  ##########################################
gst-launch-1.0 $D  \
  udpsrc uri="udp://229.2.68.223:3200" ! queue ! \
  tsdemux ! "video/x-h264" !  \
  mpegtsmux name=mux ! \
  queue ! udpsink host=229.1.1.1 port=3200 \
  videotestsrc pattern=green ! \
 "video/x-raw, width=1280, height=720, framerate=25/1" ! \
  timeoverlay ! alpha method=green ! queue ! \
  videoconvert ! dvbsubenc ! mux.
exit 0

gst-launch-1.0 \
  videotestsrc ! \
  video/x-raw,width=800,height=800 ! comp. \
  videotestsrc pattern=ball  ! \
  video/x-raw,width=600,height=600 ! \
  alpha method=3 target-r=255 target-g=255 target-b=255  \
  black-sensitivity=0 white-sensitivity=128 ! \
  compositor name=comp \
  sink_1::xpos=50 sink_1::ypos=50  \
  sink_1::height=600 sink_1::width=600 ! \
  videoconvert ! xvimagesink 

#  1  -> as background 
#  2  -> set dimension -> set location -> set transparent color -> 

  #videobox border-alpha=1 top=-70 bottom=-70 right=-220 ! \
exit 0
gst-launch-1.0 \
  videotestsrc pattern=ball flip=true ! \
  video/x-raw,width=600,height=600 ! \
  alpha method=0 target-r=0 target-g=0 target-b=0 ! \
  videoconvert ! xvimagesink 
exit 0


gst-launch-1.0 -e videomixer name=mix \
    ! videoconvert ! autovideosink \
    videotestsrc pattern="blue" \
         ! video/x-raw, framerate=10/1, width=640, height=360 \
         ! mix.sink_0 \
    videotestsrc pattern=smpte100 \
         ! video/x-raw, framerate=10/1, width=640, height=330 \
         ! alpha method=custom target-r=255 target-g=255 target-b=255  \
         black-sensitivity=100 white-sensitivity=120 \
         ! mix.sink_1
exit 0



gs-launch-1.0 filesrc location=$TTS ! tsdemux ! mpegtsmux  
   # pars.video_0 ! video/x-h264 ! mpegtsmux name=mux ! udpsink host=229.5.5.5 port=3200 \
   # pars.audio_0 ! mux.


exit 0


gst-launch-1.0 $DEBUG  filesrc location=/opt/sms/www/iptv/media/Video/1001.ts \
    ! queue ! parsebin name=parse \
    ! "video/x-h264,framerate=25" ! mpegtsmux name=mux \
    ! queue ! tsparse set-timestamps=true ! queue \
    ! rndbuffersize min=1316 max=1316  ! queue \
    ! udpsink  multicast-iface=lo host=229.2.0.1 port=3200 sync=false \
    parse. ! audio/mpeg ! mux. 


mkdir -p /tmp/hls
gst-launch-1.0 -v --gst-debug-level=3  udpsrc uri=udp://229.2.0.1:3200 \
    ! queue ! tsdemux name=demux \
    ! video/x-h264 ! h264parse \
    ! queue ! hlssink2  name=sink \
    #demux. ! queue ! audio/mpeg ! aacparse ! sink.

exit 0
gst-launch-1.0 -v urisourcebin uri=http://192.168.56.12:8001/34/hdd11.ts ! queue \
    ! parsebin name=bin \
    bin. ! queue ! video/x-h264 ! \
           mpegtsmux name=mux ! queue ! filesink location=/tmp/11.ts \
    bin. ! queue ! audio/mpeg ! mux.  

gst-launch-1.0  udpsrc uri=udp://238.2.1.2:3200 \
    ! tsdemux name=demux \
    demux. ! video/x-h264 !  queue name=q_v ! h264parse ! \
    qtmux name=mux ! queue ! filesink location=/tmp/1.mp4 \
    demux. ! audio/aac !  queue name=q_a ! aacparse ! mux. 


gst-launch-1.0 -e --gst-debug=**:4 videotestsrc is-live=true \
! queue ! videoconvert \
! videorate silent=false \
! videoscale \
! "video/x-raw, width=1280, height=720, framerate=25/1" \
! queue ! x264enc speed-preset=3 tune=zerolatency bitrate=3800 key-int-max=0 \
! queue ! muxer.video_0 \
audiotestsrc is-live=true \
! audioconvert ! audioresample ! audiorate ! "audio/x-raw, rate=48000, channels=2" \
! queue ! faac bitrate=128000 rate-control=2 \
! queue ! muxer.audio_0 \
mp4mux name=muxer streamable=true \
! queue ! filesink location="/home/myenc/mystream.mp4" sync=false




gst-launch-1.0   \
    filesrc location=/tmp/2.ts \
    ! decodebin  \
    ! encodebin profile="mpegtsmux:mpeg2enc:avenc_aac" \
    ! filesink location=/tmp/22.ts 
exit 0
gst-launch-1.0  -vt \
    filesrc location=/tmp/2.ts \
    ! tsdemux  name=dmux      \
    dmux.video_0_0 \
    ! queue   \
    ! decodebin    \
    ! videoconvert   \
    ! x264enc          \
    ! h264parse          \
    ! mpegtsmux name=mux   \
    ! filesink location=/tmp/22.ts  \
    dmux.audio_0_0 \
    ! queue   \
    ! decodebin    \
    ! audioconvert   \
    ! voaacenc        \
    ! aacparse        \
    ! mux.        

#  file -> demux ->     
#    ! queue          \
