#----------------------------------------------------------------
# Generated CMake target import file for configuration "Release".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "FFMedia::ff_media" for configuration "Release"
set_property(TARGET FFMedia::ff_media APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(FFMedia::ff_media PROPERTIES
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/aarch64-linux-gnu/libff_media.so.2.6.1"
  IMPORTED_SONAME_RELEASE "libff_media.so.1"
  )

list(APPEND _IMPORT_CHECK_TARGETS FFMedia::ff_media )
list(APPEND _IMPORT_CHECK_FILES_FOR_FFMedia::ff_media "${_IMPORT_PREFIX}/lib/aarch64-linux-gnu/libff_media.so.2.6.1" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
