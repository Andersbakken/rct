cmake_minimum_required(VERSION 2.8)
add_definitions(-DOS_${CMAKE_SYSTEM_NAME})

include(CheckSymbolExists)
include(CheckCXXSymbolExists)

include(CheckCXXCompilerFlag)
if(NOT CMAKE_SYSTEM_NAME MATCHES "Darwin")
  CHECK_CXX_COMPILER_FLAG("-std=c++0x" COMPILER_SUPPORTS_CXX_0X)
  if(COMPILER_SUPPORTS_CXX_0X)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x")
  endif()
else()
  add_definitions(-D_DARWIN_UNLIMITED_SELECT)
endif()

check_cxx_symbol_exists(backtrace "execinfo.h" HAVE_BACKTRACE)
check_cxx_symbol_exists(CLOCK_MONOTONIC_RAW "time.h" HAVE_CLOCK_MONOTONIC_RAW)
check_cxx_symbol_exists(CLOCK_MONOTONIC "time.h" HAVE_CLOCK_MONOTONIC)
check_cxx_symbol_exists(mach_absolute_time "mach/mach.h;mach/mach_time.h" HAVE_MACH_ABSOLUTE_TIME)
check_cxx_symbol_exists(inotify_init "sys/inotify.h" HAVE_INOTIFY)
check_cxx_symbol_exists(kqueue "sys/types.h;sys/event.h" HAVE_KQUEUE)
check_cxx_symbol_exists(SO_NOSIGPIPE "sys/types.h;sys/socket.h" HAVE_NOSIGPIPE)
check_cxx_symbol_exists(MSG_NOSIGNAL "sys/types.h;sys/socket.h" HAVE_NOSIGNAL)
check_cxx_symbol_exists(SA_SIGINFO "signal.h" HAVE_SIGINFO)


include_directories(${CMAKE_CURRENT_LIST_DIR}/src ${CMAKE_CURRENT_BINARY_DIR}/include)
set(RCT_SOURCES
    src/Connection.cpp
    src/Config.cpp
    src/EventLoop.cpp
    src/EventReceiver.cpp
    src/SocketClient.cpp
    src/SocketServer.cpp
    src/Log.cpp
    src/MemoryMonitor.cpp
    src/Messages.cpp
    src/Path.cpp
    src/Process.cpp
    src/Rct.cpp
    src/ReadWriteLock.cpp
    src/SHA256.cpp
    src/Semaphore.cpp
    src/SharedMemory.cpp
    src/Thread.cpp
    src/ThreadPool.cpp
    src/Value.cpp)

if(HAVE_INOTIFY EQUAL 1)
  list(APPEND RCT_SOURCES src/FileSystemWatcher_inotify.cpp)
elseif(HAVE_FSEVENTS EQUAL 1)
  list(APPEND RCT_SOURCES src/FileSystemWatcher_fsevents.cpp)
elseif(HAVE_KQUEUE EQUAL 1)
  list(APPEND RCT_SOURCES src/FileSystemWatcher_kqueue.cpp)
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin)
configure_file(${CMAKE_CURRENT_LIST_DIR}/src/rct-config.h.in
               ${CMAKE_CURRENT_BINARY_DIR}/include/rct/rct-config.h)

