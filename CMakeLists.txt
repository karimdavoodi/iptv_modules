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
                 -g
                 )
#BOOST
find_package(Boost COMPONENTS system log REQUIRED)
#GSTREAMERMM
find_package(PkgConfig REQUIRED)
pkg_check_modules(GSTMM REQUIRED gstreamermm-1.0  glibmm-2.4 )
#LIBMONGOCXX
set(CMAKE_MODULE_PATH /usr/local/lib/cmake )
find_package(LIBMONGOCXX REQUIRED)

set(ALL_LIBS  ${Boost_LIBRARIES} ${LIBMONGOCXX_LIBRARIES} ${GSTMM_LIBRARIES} -lpthread -ldl )
set(ALL_INCS  /usr/local/include ${LIBMONGOCXX_INCLUDE_DIRS} ${GSTMM_INCLUDE_DIRS})
set(MYLIBS    src/mongo_driver.cpp src/utils.cpp)

set( project_EXES
     iptv_in_archive
     iptv_in_dvb
     iptv_in_network
#     iptv_in_transcoder
#  
#     iptv_out_hls
#     iptv_out_epg
     iptv_out_multicast
#     iptv_out_record
   )

foreach( APP ${project_EXES} )
    add_executable(${APP} src/${APP}.cpp src/${APP}_gst.cpp  ${MYLIBS})
    target_link_libraries(${APP} ${ALL_LIBS})
    target_include_directories(${APP} PUBLIC ${ALL_INCS})
endforeach()
