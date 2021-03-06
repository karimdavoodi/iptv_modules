cmake_minimum_required(VERSION 3.10)
project(iptv_modules)
#set(CMAKE_CXX_COMPILER  "/usr/bin/clang++-10")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
#set(CMAKE_BUILD_TYPE Release)
#set(CMAKE_BUILD_TYPE Debug)

add_definitions( 
    -fdiagnostics-show-location=every-line
    -DBOOST_LOG_DYN_LINK=1
    -DGST_USE_UNSTABLE_API=1
    -Wall
    -g
    )

#BOOST
find_package(Boost COMPONENTS system log log_setup REQUIRED)

#GSTREAMER
set(CMAKE_MODULE_PATH /usr/local/lib/x86_64-linux-gnu/pkgconfig )
find_package(PkgConfig REQUIRED)
pkg_check_modules(GSTMM REQUIRED 
    gstreamer-1.0           # for all 
    glib-2.0                # for all
    gstreamer-mpegts-1.0    # for iptv_out_epg
    #gstreamer-transcoder-1.0
    gtk+-3.0                # for iptv_in_web
    webkit2gtk-4.0          # for iptv_in_web
    x11                     # for iptv_in_web
    )

#LIBMONGOCXX
set(CMAKE_MODULE_PATH /usr/local/lib/cmake )
find_package(LIBMONGOCXX REQUIRED)

set(ALL_LIBS  
    ${Boost_LIBRARIES} 
    ${LIBMONGOCXX_LIBRARIES} 
    ${GSTMM_LIBRARIES} 
    -ldvbcsa                    # for iptv_in_scramble
    /opt/sms/src/lock/licpp.a 
    )
set(ALL_INCS  ${LIBMONGOCXX_INCLUDE_DIRS} ${GSTMM_INCLUDE_DIRS})
set(MYLIBS    src/mongo_driver.cpp src/utils.cpp src/gst.cpp)

add_library(utils ${MYLIBS})
target_include_directories(utils PUBLIC ${ALL_INCS})
target_link_libraries(utils ${ALL_LIBS} )
set_target_properties(utils PROPERTIES
    IMPORTED_LOCATION "${CMAKE_SOURCE_DIR}/libutils.a"
    )

set( project_EXES
     # TODO: test memory leak
     # TODO: test for aac_latm that not accepted by mpegtsmux

     iptv_in_archive       # TODO: fix ts file timestamp
     iptv_in_network       # TODO: test it
     iptv_in_transcoder    # TODO:l test it, memory leak
     iptv_in_web           # TODO: test it
     iptv_in_dvb           # TODO: do by Gst
     iptv_in_scramble      # TODO: test it. do only BISS
     iptv_in_mix           ##TODO: test it
     #iptv_in_mixer_dvb    ##TODO: ... => mpegts + logo = dvb (by dvbsubenc) 
     iptv_out_hls          # TODO:l test it, drain memory in some condition 
     iptv_out_epg          # TODO: test it 
     iptv_out_multicast    # TODO: test it
     iptv_out_record       # TODO:l add moov table in mp4 when kill 
     iptv_out_rtsp         # TODO: do by Gst-rtsp-server library
     iptv_out_rf           # TODO: do by Gst
     iptv_out_http         # TODO: work by keyframe 
     #iptv_out_gold         # TODO: ...
     #iptv_out_dlna        # TODO: disabled. do by Rygel library
     iptv_out_snapshot     # TODO: test it 
     iptv_utils            # TODO: test it   
     )

foreach( APP ${project_EXES} )
    add_executable(${APP} src/${APP}.cpp src/${APP}_gst.cpp)
    target_link_libraries(${APP} utils ${ALL_LIBS})
    target_include_directories(${APP} PUBLIC ${ALL_INCS})
endforeach()

add_custom_target(copy ALL 
    COMMAND cp -f iptv_* /opt/sms/bin && strip /opt/sms/bin/iptv_*
    COMMENT "Copy to /opt/sms/bin"
    DEPENDS ${project_EXES}
    )
