# - Find the native heif library and includes
#
# This module defines
#  LIBHEIF_INCLUDE_DIR, where to libheif headers.
#  LIBHEIF_LIBRARIES, the libraries to link against to support heif.
#  LIBHEIF_FOUND, If false, do not enable heif export support.
# also defined, but not for general use are
#  LIBHEIF_LIBRARY, where to find the heif library.

#=============================================================================
# Copyright 2013 Google Inc.
#=============================================================================

SET(LIBHEIF_FIND_REQUIRED ${LIBHEIF_FIND_REQUIRED})
SET(LIBHEIF_FIND_VERSION ${LIBHEIF_FIND_VERSION})
SET(LIBHEIF_FIND_VERSION_EXACT ${LIBHEIF_FIND_VERSION_EXACT})
SET(LIBHEIF_FIND_QUIETLY ${LIBHEIF_FIND_QUIETLY})


include(LibFindMacros)

# Use pkg-config to get hints about paths
libfind_pkg_check_modules(LIBHEIF_PKGCONF libheif)


find_path(LIBHEIF_INCLUDE_DIR NAMES libheif/heif.h HINTS ${LIBHEIF_PKGCONF_INCLUDE_DIRS})
mark_as_advanced(LIBHEIF_INCLUDE_DIR)

set(LIBHEIF_NAMES ${LIBHEIF_NAMES} heif libheif)
find_library(LIBHEIF_LIBRARY NAMES ${LIBHEIF_NAMES} HINTS ${LIBHEIF_PKGCONF_LIBRARY_DIRS})
mark_as_advanced(LIBHEIF_LIBRARY)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(LIBHEIF DEFAULT_MSG LIBHEIF_LIBRARY LIBHEIF_INCLUDE_DIR)


if(LIBHEIF_FIND_VERSION)
  cmake_minimum_required(VERSION 3.10.0)
  set(LIBHEIF_FAILED_VERSION_CHECK true)

  if(LIBHEIF_FIND_VERSION_EXACT)
    if(LIBHEIF_PKGCONF_VERSION VERSION_EQUAL LIBHEIF_FIND_VERSION)
      set(LIBHEIF_FAILED_VERSION_CHECK false)
    endif()
  else()
    if(LIBHEIF_PKGCONF_VERSION VERSION_EQUAL LIBHEIF_FIND_VERSION OR
       LIBHEIF_PKGCONF_VERSION VERSION_GREATER LIBHEIF_FIND_VERSION)
      set(LIBHEIF_FAILED_VERSION_CHECK false)
    endif()
  endif()

  if(LIBHEIF_FAILED_VERSION_CHECK)
    if(LIBHEIF_FIND_REQUIRED AND NOT LIBHEIF_FIND_QUIETLY)
        if(LIBHEIF_FIND_VERSION_EXACT)
            message(FATAL_ERROR "libheif version check failed.  Version ${LIBHEIF_PKGCONF_VERSION} was found, version ${LIBHEIF_FIND_VERSION} is needed exactly.")
        else(LIBHEIF_FIND_VERSION_EXACT)
            message(FATAL_ERROR "libheif version check failed.  Version ${LIBHEIF_PKGCONF_VERSION} was found, at least version ${LIBHEIF_FIND_VERSION} is required")
        endif(LIBHEIF_FIND_VERSION_EXACT)
    endif(LIBHEIF_FIND_REQUIRED AND NOT LIBHEIF_FIND_QUIETLY)

    set(LIBHEIF_FOUND false)
  endif(LIBHEIF_FAILED_VERSION_CHECK)

endif(LIBHEIF_FIND_VERSION)


IF(LIBHEIF_FOUND)

  SET(LIBHEIF_LIBRARIES ${LIBHEIF_LIBRARY})
  SET(LIBHEIF_INCLUDE_DIRS ${LIBHEIF_INCLUDE_DIR})
ENDIF(LIBHEIF_FOUND)
