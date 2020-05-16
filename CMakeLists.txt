cmake_minimum_required(VERSION 3.10)
project(iptv_modules)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_definitions( 
                #-fdiagnostics-color=always 
                 -fdiagnostics-show-location=every-line
                 -fdiagnostics-generate-patch 
                 -DBOOST_LOG_DYN_LINK=1
                 -DGST_USE_UNSTABLE_API=1
                 )
#BOOST
find_package(Boost COMPONENTS system log REQUIRED)
#GSTREAMERMM
find_package(PkgConfig REQUIRED)
pkg_check_modules(GSTMM REQUIRED 
        gstreamer-1.0 
        glib-2.0 
        gstreamermm-1.0 
        glibmm-2.4
        gstreamer-mpegts-1.0
        )
#LIBMONGOCXX
set(CMAKE_MODULE_PATH /usr/local/lib/cmake )
find_package(LIBMONGOCXX REQUIRED)

set(ALL_LIBS  ${Boost_LIBRARIES} ${LIBMONGOCXX_LIBRARIES} ${GSTMM_LIBRARIES} )
set(ALL_INCS  /usr/local/include ${LIBMONGOCXX_INCLUDE_DIRS} ${GSTMM_INCLUDE_DIRS})
set(MYLIBS    src/mongo_driver.cpp src/utils.cpp)

add_library(utils src/mongo_driver.cpp src/utils.cpp)
target_include_directories(utils PUBLIC ${ALL_INCS})
set_target_properties(utils PROPERTIES
    IMPORTED_LOCATION "${CMAKE_SOURCE_DIR}/libutils.a"
    )

set( project_EXES
     iptv_in_archive       # TODO: save stat of last streaming
     iptv_in_dvb           # TODO: do by Gst
     iptv_in_network       # TODO: test
     iptv_in_transcoder    # TODO: do by Gst
     iptv_in_unscramble    # TODO: ...
  
     iptv_out_hls          # TODO: do by Gst 
     iptv_out_epg          # TODO: test 
     iptv_out_multicast    # TODO: test
     iptv_out_record       # TODO: test
     iptv_out_rtsp         # TODO: do by Gst-rtsp-server library
     iptv_out_rf           # TODO: ...
     iptv_out_http         # TODO: ... 
     iptv_out_gold         # TODO: ...
     iptv_out_dlna         # TODO: do by Rygel library
   )

foreach( APP ${project_EXES} )
    add_executable(${APP} src/${APP}.cpp src/${APP}_gst.cpp)
    target_link_libraries(${APP} utils ${ALL_LIBS} )
    target_include_directories(${APP} PUBLIC ${ALL_INCS})
endforeach()

add_custom_target(copy ALL 
    COMMAND cp -f iptv_* /opt/sms/bin
    COMMENT "Copy to /opt/sms/bin"
    DEPENDS ${project_EXES}
    )
