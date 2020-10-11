cmake_minimum_required(VERSION 3.10)
project(iptv_modules_test)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
#set(CMAKE_BUILD_TYPE Release)
#set(CMAKE_BUILD_TYPE Debug)

add_definitions( 
    -fdiagnostics-show-location=every-line
    -fdiagnostics-generate-patch 
    -DGST_USE_UNSTABLE_API=1
    -Wall
    )
FILE(GLOB GST_SRC ../src/*_gst.cpp)
FILE(GLOB TEST_SRC src/*.cpp)

find_package(Boost COMPONENTS system log log_setup REQUIRED)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GST REQUIRED 
    gstreamer-1.0           # for all 
    glib-2.0                # for all
    gstreamer-mpegts-1.0    # for iptv_out_epg
    gtk+-3.0                # for iptv_in_web
    webkit2gtk-4.0          # for iptv_in_web
    x11                     # for iptv_in_web
    )
set(CMAKE_MODULE_PATH /usr/local/lib/cmake )
find_package(LIBMONGOCXX REQUIRED)

add_executable(${PROJECT_NAME}
        ${TEST_SRC}
        ../src/mongo_driver.cpp 
        ../src/utils.cpp 
        ../src/gst.cpp
        ${GST_SRC})
target_include_directories(${PROJECT_NAME} PUBLIC 
        ${GST_INCLUDE_DIRS} 
        ${LIBMONGOCXX_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} 
        ${GST_LIBRARIES} 
        ${LIBMONGOCXX_LIBRARIES}
        ${Boost_LIBRARIES}
        -lpthread )
    #    add_custom_target(run_test ALL 
    #    COMMAND ./${APP}
    #    COMMENT "Run it"
    #    )

