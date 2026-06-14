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
    pbsclient.cpp
    pbsclient.h
    tibackupdiskobserver.cpp
    tibackupdiskobserver.h
    ticonf.cpp
    ticonf.h
    tibackupservice.cpp
    tibackupservice.h
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

target_include_directories(tiBackupLib PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include/tibackuplib>
)

# Lets consumers link tiBackupLib::tiBackupLib uniformly, whether they pull the
# library in via find_package() (installed/exported) or add_subdirectory()/FetchContent.
add_library(tiBackupLib::tiBackupLib ALIAS tiBackupLib)

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

# Install layout matches the Debian package (prefix /usr): the runtime .so under
# lib/, public headers under include/tibackuplib/, and a relocatable CMake package
# config under lib/cmake/tiBackupLib/ so consumers can find_package(tiBackupLib).
# The Debian -dev package ships the same cmake/tiBackupLibConfig.cmake (see
# tibackuplib-dev/debian/rules); the .so is installed by the main debian/rules.
install(TARGETS tiBackupLib
    LIBRARY DESTINATION lib
)

install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/"
    DESTINATION include/tibackuplib
    FILES_MATCHING
        PATTERN "*.h"
        PATTERN "build*" EXCLUDE
        PATTERN "cmake-build*" EXCLUDE
        PATTERN "obj-*" EXCLUDE
        PATTERN ".git" EXCLUDE
        PATTERN "cmake" EXCLUDE
)

install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/cmake/tiBackupLibConfig.cmake"
    DESTINATION lib/cmake/tiBackupLib
)
