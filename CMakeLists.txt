cmake_minimum_required(VERSION 3.16)

project(tiBackupLib VERSION 0.7.7 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Core Network)

qt_standard_project_setup()

qt_add_library(tiBackupLib SHARED
    tibackuplib.cpp
    tibackuplib.h
    backupmanager.cpp
    backupmanager.h
    ipcclient.cpp
    ipcclient.h
    pbsclient.cpp
    pbsclient.h
    tibackupdiskobserver.cpp
    tibackupdiskobserver.h
    ticonf.cpp
    ticonf.h
    tibackupservice.cpp
    tibackupservice.h
    tibackupapi.cpp
    tibackupapi.h
    tibackuplib_global.h
    config.h
    obj/devicedisk.cpp
    obj/devicedisk.h
    obj/pbserver.cpp
    obj/pbserver.h
    obj/tibackupjob.cpp
    obj/tibackupjob.h
    obj/exitcodes.cpp
    obj/exitcodes.h
    obj/HttpStatusCodes.h
    workers/tibackupjobworker.cpp
    workers/tibackupjobworker.h
    resdata.qrc
)

target_include_directories(tiBackupLib PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_compile_definitions(tiBackupLib PRIVATE TIBACKUPLIB_LIBRARY)

target_compile_options(tiBackupLib PRIVATE
    $<$<CONFIG:Debug>:-pipe>
    $<$<CONFIG:Release>:-pipe;-O2>
)

target_link_libraries(tiBackupLib
    PRIVATE
        Qt6::Core
        Qt6::Network
        PocoFoundation
        PocoNet
        udev
        blkid
)

install(TARGETS tiBackupLib
    LIBRARY DESTINATION /usr/lib
)
