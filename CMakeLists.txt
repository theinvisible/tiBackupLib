cmake_minimum_required(VERSION 3.16)

# Single source of truth for the version of both tiBackupLib and the tiBackup
# server: the server reads it at runtime through the generated config.h
# (tibackup_config::version), so the version is maintained only on this line.
project(tiBackupLib VERSION 0.9.2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Core Network)

qt_standard_project_setup()

# Generate config.h from config.h.in, substituting @PROJECT_VERSION@ with the
# project() version above. The generated header lives in the build tree; it is
# compiled in and installed as a public header (see install(FILES ...) below).
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/config.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/config.h"
    @ONLY
)

qt_add_library(tiBackupLib SHARED
    tibackuplib.cpp
    tibackuplib.h
    backupmanager.cpp
    backupmanager.h
    pbsclient.cpp
    pbsclient.h
    sshclient.cpp
    sshclient.h
    tibackupdiskobserver.cpp
    tibackupdiskobserver.h
    ticonf.cpp
    ticonf.h
    tibackupservice.cpp
    tibackupservice.h
    tibackupscheduler.cpp
    tibackupscheduler.h
    tibackupmailer.cpp
    tibackupmailer.h
    tibackuplib_global.h
    "${CMAKE_CURRENT_BINARY_DIR}/config.h"
    obj/devicedisk.cpp
    obj/devicedisk.h
    obj/pbserver.cpp
    obj/pbserver.h
    obj/sshserver.cpp
    obj/sshserver.h
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
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>  # generated config.h
    $<INSTALL_INTERFACE:include/tibackuplib>
)

# Give the shared object a real soname (libtiBackupLib.so.<major> ->
# libtiBackupLib.so.<version>), so ABI breaks become loader-visible instead of
# silently rebinding against an unversioned .so. SOVERSION is the major only, so
# compatible releases keep the same soname; the Debian package (debian/rules
# ships libtiBackupLib.so*) picks up the symlinks automatically.
set_target_properties(tiBackupLib PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
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
        PocoNetSSL
        PocoCrypto
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

# config.h is generated (not in the source tree), so install it explicitly
# alongside the other public headers.
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/config.h"
    DESTINATION include/tibackuplib
)

install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/cmake/tiBackupLibConfig.cmake"
    DESTINATION lib/cmake/tiBackupLib
)
