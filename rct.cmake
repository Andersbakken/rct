cmake_minimum_required(VERSION 2.8)
add_definitions(-DOS_${CMAKE_SYSTEM_NAME})

include(CheckSymbolExists)
include(CheckCXXSymbolExists)

include(CheckCXXCompilerFlag)
if (NOT CMAKE_SYSTEM_NAME MATCHES "Darwin")
  check_cxx_compiler_flag("-std=c++0x" COMPILER_SUPPORTS_CXX_0X)
  if (COMPILER_SUPPORTS_CXX_0X)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x")
  endif ()
else ()
  add_definitions(-D_DARWIN_UNLIMITED_SELECT)
endif ()

check_cxx_symbol_exists(backtrace "execinfo.h" HAVE_BACKTRACE)
check_cxx_symbol_exists(CLOCK_MONOTONIC_RAW "time.h" HAVE_CLOCK_MONOTONIC_RAW)
check_cxx_symbol_exists(CLOCK_MONOTONIC "time.h" HAVE_CLOCK_MONOTONIC)
check_cxx_symbol_exists(mach_absolute_time "mach/mach.h;mach/mach_time.h" HAVE_MACH_ABSOLUTE_TIME)
check_cxx_symbol_exists(inotify_init "sys/inotify.h" HAVE_INOTIFY)
check_cxx_symbol_exists(kqueue "sys/types.h;sys/event.h" HAVE_KQUEUE)
check_cxx_symbol_exists(SO_NOSIGPIPE "sys/types.h;sys/socket.h" HAVE_NOSIGPIPE)
check_cxx_symbol_exists(MSG_NOSIGNAL "sys/types.h;sys/socket.h" HAVE_NOSIGNAL)
check_cxx_symbol_exists(SA_SIGINFO "signal.h" HAVE_SIGINFO)

if (NOT DEFINED RCT_INCLUDE_DIR)
   set(RCT_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/include/rct")
endif ()

include_directories(${CMAKE_CURRENT_LIST_DIR} ${RCT_INCLUDE_DIR})
set(RCT_SOURCES
  ${CMAKE_CURRENT_LIST_DIR}/rct/Connection.cpp
  ${CMAKE_CURRENT_LIST_DIR}/rct/Config.cpp
  ${CMAKE_CURRENT_LIST_DIR}/rct/EventLoop.cpp
  ${CMAKE_CURRENT_LIST_DIR}/rct/EventReceiver.cpp
  ${CMAKE_CURRENT_LIST_DIR}/rct/SocketClient.cpp
  ${CMAKE_CURRENT_LIST_DIR}/rct/SocketServer.cpp
  ${CMAKE_CURRENT_LIST_DIR}/rct/Log.cpp
  ${CMAKE_CURRENT_LIST_DIR}/rct/MemoryMonitor.cpp
  ${CMAKE_CURRENT_LIST_DIR}/rct/Messages.cpp
  ${CMAKE_CURRENT_LIST_DIR}/rct/Path.cpp
  ${CMAKE_CURRENT_LIST_DIR}/rct/Process.cpp
  ${CMAKE_CURRENT_LIST_DIR}/rct/Rct.cpp
  ${CMAKE_CURRENT_LIST_DIR}/rct/ReadWriteLock.cpp
  ${CMAKE_CURRENT_LIST_DIR}/rct/SHA256.cpp
  ${CMAKE_CURRENT_LIST_DIR}/rct/Semaphore.cpp
  ${CMAKE_CURRENT_LIST_DIR}/rct/SharedMemory.cpp
  ${CMAKE_CURRENT_LIST_DIR}/rct/Thread.cpp
  ${CMAKE_CURRENT_LIST_DIR}/rct/ThreadPool.cpp
  ${CMAKE_CURRENT_LIST_DIR}/rct/Value.cpp)

if (HAVE_INOTIFY EQUAL 1)
  list(APPEND RCT_SOURCES ${CMAKE_CURRENT_LIST_DIR}/rct/FileSystemWatcher_inotify.cpp)
elseif (HAVE_FSEVENTS EQUAL 1)
  list(APPEND RCT_SOURCES ${CMAKE_CURRENT_LIST_DIR}/rct/FileSystemWatcher_fsevents.cpp)
elseif (HAVE_KQUEUE EQUAL 1)
  list(APPEND RCT_SOURCES ${CMAKE_CURRENT_LIST_DIR}/rct/FileSystemWatcher_kqueue.cpp)
endif ()

configure_file(${CMAKE_CURRENT_LIST_DIR}/rct/rct-config.h.in
  ${RCT_INCLUDE_DIR}/rct-config.h)
