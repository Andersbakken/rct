# Locate V8
# This module defines
# V8_LIBRARY
# V8_FOUND, if false, do not try to link to gdal
# V8_INCLUDE_DIR, where to find the headers
#
# $V8_DIR is an environment variable that would
# correspond to the ./configure --prefix=$V8_DIR
#
# Created by Robert Osfield (based on FindFLTK.cmake)

FIND_PATH(V8_INCLUDE_DIR v8.h
    $ENV{V8_DIR}/include
    $ENV{V8_DIR}
    ~/Library/Frameworks
    /Library/Frameworks
    /usr/local/include
    /usr/include
    /sw/include # Fink
    /opt/local/include # DarwinPorts
    /opt/csw/include # Blastwave
    /opt/include
    /usr/freeware/include
)

FIND_LIBRARY(V8_LIBRARY
    NAMES v8 libv8
    PATHS
    $ENV{V8_DIR}/lib
    $ENV{V8_DIR}
    ~/Library/Frameworks
    /Library/Frameworks
    /usr/local/lib
    /usr/lib
    /sw/lib
    /opt/local/lib
    /opt/csw/lib
    /opt/lib
    /usr/freeware/lib64
)

if (V8_LIBRARY)
    SET(V8_LIBRARIES ${V8_LIBRARY})
else()
    FIND_LIBRARY(V8_LIBRARY_BASE
        NAMES v8_base
        PATHS
        $ENV{V8_DIR}/lib
        $ENV{V8_DIR}
        ~/Library/Frameworks
        /Library/Frameworks
        /usr/local/lib
        /usr/lib
        /sw/lib
        /opt/local/lib
        /opt/csw/lib
        /opt/lib
        /usr/freeware/lib64
    )
    FIND_LIBRARY(V8_LIBRARY_LIBBASE
        NAMES v8_libbase
        PATHS
        $ENV{V8_DIR}/lib
        $ENV{V8_DIR}
        ~/Library/Frameworks
        /Library/Frameworks
        /usr/local/lib
        /usr/lib
        /sw/lib
        /opt/local/lib
        /opt/csw/lib
        /opt/lib
        /usr/freeware/lib64
    )
    FIND_LIBRARY(V8_LIBRARY_LIBPLATFORM
        NAMES v8_libplatform
        PATHS
        $ENV{V8_DIR}/lib
        $ENV{V8_DIR}
        ~/Library/Frameworks
        /Library/Frameworks
        /usr/local/lib
        /usr/lib
        /sw/lib
        /opt/local/lib
        /opt/csw/lib
        /opt/lib
        /usr/freeware/lib64
    )
    FIND_LIBRARY(V8_LIBRARY_SNAPSHOT
        NAMES v8_snapshot
        PATHS
        $ENV{V8_DIR}/lib
        $ENV{V8_DIR}
        ~/Library/Frameworks
        /Library/Frameworks
        /usr/local/lib
        /usr/lib
        /sw/lib
        /opt/local/lib
        /opt/csw/lib
        /opt/lib
        /usr/freeware/lib64
    )
    FIND_LIBRARY(V8_LIBRARY_ICUI18N
	NAMES icui18n
	PATHS
	$ENV{V8_DIR}/lib
	$ENV{V8_DIR}
	~/Library/Frameworks
	/Library/Frameworks
	/usr/local/lib
	/usr/lib
	/sw/lib
	/opt/local/lib
	/opt/csw/lib
	/opt/lib
	/usr/freeware/lib64
    )
    FIND_LIBRARY(V8_LIBRARY_ICUUC
	NAMES icuuc
	PATHS
	$ENV{V8_DIR}/lib
	$ENV{V8_DIR}
	~/Library/Frameworks
	/Library/Frameworks
	/usr/local/lib
	/usr/lib
	/sw/lib
	/opt/local/lib
	/opt/csw/lib
	/opt/lib
	/usr/freeware/lib64
    )
    FIND_LIBRARY(V8_LIBRARY_ICUDATA
	NAMES icudata
	PATHS
	$ENV{V8_DIR}/lib
	$ENV{V8_DIR}
	~/Library/Frameworks
	/Library/Frameworks
	/usr/local/lib
	/usr/lib
	/sw/lib
	/opt/local/lib
	/opt/csw/lib
	/opt/lib
	/usr/freeware/lib64
    )
    if (V8_LIBRARY_BASE AND V8_LIBRARY_LIBBASE AND V8_LIBRARY_LIBPLATFORM AND V8_LIBRARY_SNAPSHOT AND V8_LIBRARY_ICUI18N AND V8_LIBRARY_ICUUC AND V8_LIBRARY_ICUDATA)
	set(V8_LIBRARIES ${V8_LIBRARY_BASE} ${V8_LIBRARY_LIBBASE} ${V8_LIBRARY_LIBPLATFORM} ${V8_LIBRARY_SNAPSHOT} ${V8_LIBRARY_ICUI18N} ${V8_LIBRARY_ICUUC} ${V8_LIBRARY_ICUDATA})
    endif()
endif()

SET(V8_FOUND "NO")
IF(V8_LIBRARIES AND V8_INCLUDE_DIR)
    SET(V8_FOUND "YES")
ENDIF()
