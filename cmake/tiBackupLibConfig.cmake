# Relocatable package config for tiBackupLib.
#
# Provides the imported target tiBackupLib::tiBackupLib. Works both for the
# Debian package (installed under prefix /usr) and a `cmake --install --prefix`
# tree, because the prefix is derived from this file's location:
#   <prefix>/lib/cmake/tiBackupLib/tiBackupLibConfig.cmake  ->  <prefix>
#
# tiBackupLib exposes only Qt types in its public headers (consumers find Qt6
# themselves) and resolves Poco/udev/blkid internally (linked PRIVATE), so no
# transitive dependencies are propagated here.

get_filename_component(_tibackuplib_prefix "${CMAKE_CURRENT_LIST_DIR}/../../.." REALPATH)

if(NOT TARGET tiBackupLib::tiBackupLib)
    add_library(tiBackupLib::tiBackupLib SHARED IMPORTED)
    set_target_properties(tiBackupLib::tiBackupLib PROPERTIES
        IMPORTED_LOCATION "${_tibackuplib_prefix}/lib/libtiBackupLib.so"
        INTERFACE_INCLUDE_DIRECTORIES "${_tibackuplib_prefix}/include/tibackuplib"
    )
endif()

unset(_tibackuplib_prefix)
