# Try to find cppunit - http://freedesktop.org/wiki/Software/cppunit
# Defines:
#  CPPUNIT_FOUND
#  CPPUNIT_INCLUDE_DIRS
#  CPPUNIT_LIBRARIES

# try to find using pkgconfig
find_package(PkgConfig)
pkg_check_modules(PC_CPPUNIT QUIET cppunit)

find_path(CPPUNIT_INCLUDE_DIR cppunit/Test.h
          HINTS ${PC_CPPUNIT_INCLUDEDIR} ${PC_CPPUNIT_INCLUDE_DIRS}
          PATH_SUFFIXES cppunit)

find_library(CPPUNIT_LIBRARY NAMES cppunit
             HINTS ${PC_CPPUNIT_LIBDIR} ${PC_CPPUNIT_LIBRARY_DIRS} )

include(FindPackageHandleStandardArgs)
# handle the QUIETLY and REQUIRED arguments and set CPPUNIT_FOUND to TRUE
# if all listed variables are TRUE
find_package_handle_standard_args(CPPUNIT DEFAULT_MSG
                                  CPPUNIT_LIBRARY CPPUNIT_INCLUDE_DIR)

mark_as_advanced(CPPUNIT_INCLUDE_DIR CPPUNIT_LIBRARY )

set(CPPUNIT_LIBRARIES ${CPPUNIT_LIBRARY} )
set(CPPUNIT_INCLUDE_DIRS ${CPPUNIT_INCLUDE_DIR} )
