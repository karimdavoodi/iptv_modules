cmake_minimum_required(VERSION 3.10)
project(iptv_out)
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

set(ALL_LIBS ${Boost_LIBRARIES} ${LIBMONGOCXX_LIBRARIES} ${GSTMM_LIBRARIES} -lpthread -ldl )
set(ALL_INCS  /usr/local/include ${LIBMONGOCXX_INCLUDE_DIRS} ${GSTMM_INCLUDE_DIRS})
set(MYLIBS src/mongo_driver.cpp src/utils.cpp)


set(APP iptv_from_archive)
add_executable(${APP} src/${APP}.cpp src/iptv_archive_play.cpp  ${MYLIBS})
target_link_libraries(${APP} ${ALL_LIBS})
target_include_directories(${APP} PUBLIC ${ALL_INCS})

set(APP iptv_to_http)
add_executable(${APP} src/${APP}.cpp src/iptv_to_http_play.cpp ${MYLIBS})
target_link_libraries(${APP} ${ALL_LIBS})
target_include_directories(${APP} PUBLIC ${ALL_INCS})
#
#set(APP channels_epg)
#add_executable(${APP} src/${APP}.cpp ${MYLIBS})
#target_link_libraries(${APP} ${ALL_LIBS})
#target_include_directories(${APP} PUBLIC ${ALL_INCS})
