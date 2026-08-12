
####### Expanded from @PACKAGE_INIT@ by configure_package_config_file() #######
####### Any changes to this file will be overwritten by the next CMake run ####
####### The input file was FFMediaConfig.cmake.in                            ########

get_filename_component(PACKAGE_PREFIX_DIR "${CMAKE_CURRENT_LIST_DIR}/../../../" ABSOLUTE)

macro(set_and_check _var _file)
  set(${_var} "${_file}")
  if(NOT EXISTS "${_file}")
    message(FATAL_ERROR "File or directory ${_file} referenced by variable ${_var} does not exist !")
  endif()
endmacro()

macro(check_required_components _NAME)
  foreach(comp ${${_NAME}_FIND_COMPONENTS})
    if(NOT ${_NAME}_${comp}_FOUND)
      if(${_NAME}_FIND_REQUIRED_${comp})
        set(${_NAME}_FOUND FALSE)
      endif()
    endif()
  endforeach()
endmacro()

####################################################################################

set(FFMedia_VERSION "2.6.0")
set(FFMedia_MODULE_ABI_VERSION "1")
set(FFMedia_GLIBCXX_USE_CXX11_ABI "1")
set(FFMedia_CXX_STANDARD 17)
set(FFMedia_WITH_AUDIO ON)
set(FFMedia_WITH_FFMPEG ON)
set(FFMedia_WITH_OPENGL ON)
set(FFMedia_WITH_INFERENCE ON)

set(FFMedia_core_FOUND TRUE)
set(FFMedia_audio_FOUND ${FFMedia_WITH_AUDIO})
set(FFMedia_ffmpeg_FOUND ${FFMedia_WITH_FFMPEG})
set(FFMedia_opengl_FOUND ${FFMedia_WITH_OPENGL})
set(FFMedia_inference_FOUND ${FFMedia_WITH_INFERENCE})

include("${CMAKE_CURRENT_LIST_DIR}/FFMediaTargets.cmake")

if(NOT TARGET FFMedia::FFMedia)
    add_library(FFMedia::FFMedia INTERFACE IMPORTED)
    set_property(TARGET FFMedia::FFMedia PROPERTY
        INTERFACE_LINK_LIBRARIES FFMedia::ff_media)
endif()

check_required_components(FFMedia)
