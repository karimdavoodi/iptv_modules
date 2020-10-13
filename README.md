# iptv_modules
Sample Implements of Video Streaming by Gstreamer. It contains:

     - iptv_in_archive: streaming a media file as multicast MPEGTS     
     - iptv_in_network: streaming an network url as multicast MPEGTS     
     - iptv_in_transcoder: transcode one MPEGTS stream to another 
     - iptv_in_web: streaming a web URL to multicast MPEGTS         
     - iptv_in_dvb: streaming a DVBT/DVBS to multicast MPEGTS          
     - iptv_in_scramble: encrypt/decrypt multicast MPEGTS by AES/BISS  
     - iptv_in_mix: mix two MpegTS by overlapping/scaling/tranparency filter          
     - iptv_out_hls: convert MpegTS format to HLS        
     - iptv_out_epg: Take EPG of MpegTS streams        
     - iptv_out_multicast: relay multicast MpegTS to another  
     - iptv_out_record: record MpegTS stream in MKV format   
     - iptv_out_rtsp: relay MpegTS stream on RTSP protocol    
     - iptv_out_rf: multiplex and send MpegTS streams on DVBT(RF)         
     - iptv_out_http: serve MpegTS streams on unicast HTTP stream    
     - iptv_out_snapshot: take snapshut from MpegTS stream   


## dependency 
    - libgstreamer: library for constructing graphs of media-handling components.
    - libmongocxx: C++ wrapper for MongoDB
    - Boost: C++ libraries
