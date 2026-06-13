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

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# Note: the Debian package installs the .so manually via debian/rules; this
# install/export path is used by find_package() consumers (CI build-checks,
# FetchContent, `cmake --install --prefix ...`).
install(TARGETS tiBackupLib
    EXPORT tiBackupLibTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

# Public headers, mirroring the tibackuplib-dev layout: include/tibackuplib/{,obj,workers}
install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/"
    DESTINATION include/tibackuplib
    FILES_MATCHING
        PATTERN "*.h"
        PATTERN "build*" EXCLUDE
        PATTERN "obj-*" EXCLUDE
        PATTERN ".git" EXCLUDE
        PATTERN "cmake" EXCLUDE
)

install(EXPORT tiBackupLibTargets
    FILE tiBackupLibTargets.cmake
    NAMESPACE tiBackupLib::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/tiBackupLib
)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/tiBackupLibConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/tiBackupLibConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/tiBackupLib
)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/tiBackupLibConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/tiBackupLibConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/tiBackupLibConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/tiBackupLib
)
