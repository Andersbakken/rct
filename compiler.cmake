include(CheckCXXCompilerFlag)

set(SHADOW "")
if (NOT CMAKE_COMPILER_IS_GNUCC OR CMAKE_CXX_COMPILER_VERSION VERSION_GREATER 5.0)
    set(SHADOW "-Wshadow")
endif ()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpointer-arith -Wnon-virtual-dtor -Wformat ${SHADOW}")
if (NOT RCT_RTTI_ENABLED)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti")
endif ()


set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Wpointer-arith -Wformat ${SHADOW}") # -pthread")
if (CMAKE_SYSTEM_NAME MATCHES "Darwin")
    add_definitions(-D_DARWIN_UNLIMITED_SELECT)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
else ()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pthread")
    if (NOT CMAKE_SYSTEM_NAME MATCHES "CYGWIN")
        # Use pic instead of PIC, which produces faster and smaller code,
        # but could eventully lead to linker problems.
        if(NOT WIN32)
            set(CMAKE_CXX_COMPILE_OPTIONS_PIC  "-fpic")
            set(CMAKE_C_COMPILE_OPTIONS_PIC  "-fpic")
        endif()
    endif ()
    if (RCT_USE_LIBCXX)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -lc++abi")
        string(STRIP "${CMAKE_EXE_LINKER_FLAGS}" CMAKE_EXE_LINKER_FLAGS)
    endif ()
endif ()
