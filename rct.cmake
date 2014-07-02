cmake_minimum_required(VERSION 2.8)
project(rct)
add_definitions(-DOS_${CMAKE_SYSTEM_NAME})

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_LIST_DIR}/cmake/")

include(CheckSymbolExists)
include(CheckCXXSymbolExists)
include(CheckCXXSourceCompiles)
include(CheckCXXSourceRuns)

include(${CMAKE_CURRENT_LIST_DIR}/compiler.cmake)

check_cxx_symbol_exists(backtrace "execinfo.h" HAVE_BACKTRACE)
check_cxx_symbol_exists(CLOCK_MONOTONIC_RAW "time.h" HAVE_CLOCK_MONOTONIC_RAW)
check_cxx_symbol_exists(CLOCK_MONOTONIC "time.h" HAVE_CLOCK_MONOTONIC)
check_cxx_symbol_exists(mach_absolute_time "mach/mach.h;mach/mach_time.h" HAVE_MACH_ABSOLUTE_TIME)
check_cxx_symbol_exists(inotify_init "sys/inotify.h" HAVE_INOTIFY)
check_cxx_symbol_exists(kqueue "sys/types.h;sys/event.h" HAVE_KQUEUE)
check_cxx_symbol_exists(epoll_wait "sys/epoll.h" HAVE_EPOLL)
check_cxx_symbol_exists(select "sys/select.h" HAVE_SELECT)
check_cxx_symbol_exists(FD_CLOEXEC "fcntl.h" HAVE_CLOEXEC)
check_cxx_symbol_exists(SO_NOSIGPIPE "sys/types.h;sys/socket.h" HAVE_NOSIGPIPE)
check_cxx_symbol_exists(MSG_NOSIGNAL "sys/types.h;sys/socket.h" HAVE_NOSIGNAL)
check_cxx_symbol_exists(GetLogicalProcessorInformation "windows.h" HAVE_PROCESSORINFORMATION)
check_cxx_symbol_exists(SCHED_IDLE "pthread.h" HAVE_SCHEDIDLE)
check_cxx_symbol_exists(SHM_DEST "sys/types.h;sys/ipc.h;sys/shm.h" HAVE_SHMDEST)

if (CYGWIN)
  message("Using win32 FileSystemWatcher")
  set(HAVE_CHANGENOTIFICATION 1)
  set(HAVE_CYGWIN 1)
endif ()

if (CMAKE_SYSTEM_NAME MATCHES "Darwin")
  exec_program(sw_vers ARGS -productVersion OUTPUT_VARIABLE OSX_VERSION)
  message("OS X version ${OSX_VERSION}")
  string(REGEX REPLACE "^([0-9]+)\\.[0-9]+\\..*$" "\\1" OSX_MAJOR ${OSX_VERSION})
  string(REGEX REPLACE "^[0-9]+\\.([0-9]+)\\..*$" "\\1" OSX_MINOR ${OSX_VERSION})
  if (${OSX_MAJOR} EQUAL 10 AND ${OSX_MINOR} GREATER 6)
    set(HAVE_FSEVENTS 1)
    message("Using FSEvents")
  elseif (${OSX_MAJOR} GREATER 10)
    set(HAVE_FSEVENTS 1)
    message("Using FSEvents")
  else ()
    message("Using kqueue")
  endif ()
endif ()

check_cxx_source_compiles("
  #include <sys/types.h>
  #include <sys/stat.h>
  int main(int, char**) {
      struct stat st;
      return st.st_mtim.tv_sec;
  }" HAVE_STATMTIM)

if (NOT DEFINED RCT_INCLUDE_DIR)
   set(RCT_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/include/rct")
endif ()

find_package(ZLIB REQUIRED)

include_directories(${CMAKE_CURRENT_LIST_DIR} ${RCT_INCLUDE_DIR} ${RCT_INCLUDE_DIR}/.. ${ZLIB_INCLUDE_DIRS})
set(RCT_SOURCES
  ${CMAKE_CURRENT_LIST_DIR}/rct/AES256CBC.cpp
  ${CMAKE_CURRENT_LIST_DIR}/rct/Buffer.cpp
  ${CMAKE_CURRENT_LIST_DIR}/rct/Config.cpp
  ${CMAKE_CURRENT_LIST_DIR}/rct/Connection.cpp
  ${CMAKE_CURRENT_LIST_DIR}/rct/EventLoop.cpp
  ${CMAKE_CURRENT_LIST_DIR}/rct/Log.cpp
  ${CMAKE_CURRENT_LIST_DIR}/rct/MemoryMonitor.cpp
  ${CMAKE_CURRENT_LIST_DIR}/rct/MessageQueue.cpp
  ${CMAKE_CURRENT_LIST_DIR}/rct/Messages.cpp
  ${CMAKE_CURRENT_LIST_DIR}/rct/Path.cpp
  ${CMAKE_CURRENT_LIST_DIR}/rct/Plugin.cpp
  ${CMAKE_CURRENT_LIST_DIR}/rct/Process.cpp
  ${CMAKE_CURRENT_LIST_DIR}/rct/Rct.cpp
  ${CMAKE_CURRENT_LIST_DIR}/rct/ReadWriteLock.cpp
  ${CMAKE_CURRENT_LIST_DIR}/rct/SHA256.cpp
  ${CMAKE_CURRENT_LIST_DIR}/rct/Semaphore.cpp
  ${CMAKE_CURRENT_LIST_DIR}/rct/SharedMemory.cpp
  ${CMAKE_CURRENT_LIST_DIR}/rct/SocketClient.cpp
  ${CMAKE_CURRENT_LIST_DIR}/rct/SocketServer.cpp
  ${CMAKE_CURRENT_LIST_DIR}/rct/String.cpp
  ${CMAKE_CURRENT_LIST_DIR}/rct/Thread.cpp
  ${CMAKE_CURRENT_LIST_DIR}/rct/ThreadPool.cpp
  ${CMAKE_CURRENT_LIST_DIR}/rct/Timer.cpp
  ${CMAKE_CURRENT_LIST_DIR}/rct/Value.cpp
  ${CMAKE_CURRENT_LIST_DIR}/cJSON/cJSON.c)

if (HAVE_INOTIFY EQUAL 1)
  list(APPEND RCT_SOURCES ${CMAKE_CURRENT_LIST_DIR}/rct/FileSystemWatcher_inotify.cpp)
elseif (HAVE_FSEVENTS EQUAL 1)
  list(APPEND RCT_SOURCES ${CMAKE_CURRENT_LIST_DIR}/rct/FileSystemWatcher_fsevents.cpp)
elseif (HAVE_KQUEUE EQUAL 1)
  list(APPEND RCT_SOURCES ${CMAKE_CURRENT_LIST_DIR}/rct/FileSystemWatcher_kqueue.cpp)
elseif (HAVE_CHANGENOTIFICATION EQUAL 1)
  list(APPEND RCT_SOURCES ${CMAKE_CURRENT_LIST_DIR}/rct/FileSystemWatcher_win32.cpp)
endif ()


if (RCT_BUILD_SCRIPTENGINE)
  find_package(V8)
  if (V8_FOUND EQUAL 1)
    set(HAVE_SCRIPTENGINE 1)
    message("Building ScriptEngine")
    set(RCT_SOURCES ${RCT_SOURCES} ${CMAKE_CURRENT_LIST_DIR}/rct/ScriptEngine.cpp)
    include_directories(${V8_INCLUDE})
  endif()
endif()

configure_file(${CMAKE_CURRENT_LIST_DIR}/rct/rct-config.h.in ${RCT_INCLUDE_DIR}/rct-config.h.gen)
if (EXISTS "${RCT_INCLUDE_DIR}/rct-config.h")
  file(READ "${RCT_INCLUDE_DIR}/rct-config.h" cfif_output)
else()
  set(cfif_output "")
endif ()
file(READ "${RCT_INCLUDE_DIR}/rct-config.h.gen" cfif_output_gen)
if (NOT "${cfif_output}" STREQUAL "${cfif_output_gen}")
  file (WRITE "${RCT_INCLUDE_DIR}/rct-config.h" "${cfif_output_gen}")
endif()
file(REMOVE "${RCT_INCLUDE_DIR}/rct-config.h.gen")

add_definitions(-DOS_${CMAKE_SYSTEM_NAME})
add_library(rct SHARED ${RCT_SOURCES})
if (RCT_EVENTLOOP_CALLBACK_TIME_THRESHOLD)
  add_definitions("-DRCT_EVENTLOOP_CALLBACK_TIME_THRESHOLD=${RCT_EVENTLOOP_CALLBACK_TIME_THRESHOLD}")
endif ()

if (CMAKE_SYSTEM_NAME MATCHES "Darwin")
  find_library(CORESERVICES_LIBRARY CoreServices)
  find_path(CORESERVICES_INCLUDE "CoreServices/CoreServices.h")
  find_library(COREFOUNDATION_LIBRARY CoreFoundation)
  find_path(COREFOUNDATION_INCLUDE "CoreFoundation/CoreFoundation.h")
endif ()

if (CMAKE_SYSTEM_NAME MATCHES "Linux")
  set(RCT_SYSTEM_LIBRARIES dl pthread rt)
endif ()
if (NOT CMAKE_SYSTEM_NAME MATCHES "Darwin")
  list(APPEND RCT_SYSTEM_LIBRARIES crypto pthread)
endif()

target_link_libraries(rct ${CORESERVICES_LIBRARY} ${COREFOUNDATION_LIBRARY} ${RCT_SYSTEM_LIBRARIES} ${ZLIB_LIBRARIES} ${V8_LIBS})

install(CODE "message(\"Installing rct...\")")
install(TARGETS rct DESTINATION lib COMPONENT rct EXPORT rct)

check_cxx_source_compiles("
  #include <memory>
  #include <mutex>
  #include <tuple>
  #include <string>
  #include <${CMAKE_CURRENT_LIST_DIR}/rct/Apply.h>

  void callTest(int, std::string) { }

  int main(int, char**) {
      std::shared_ptr<int> ptr;
      std::mutex mtx;
      std::unique_lock<std::mutex> lock(mtx);
      std::tuple<int, std::string> tpl(5, std::string(\"foo\"));
      applyMove(std::bind(callTest, std::placeholders::_1, std::placeholders::_2), tpl);
      return 0;
  }" HAVE_CXX11)

if (NOT HAVE_CXX11)
  message(FATAL_ERROR "C++11 support not detected. rct requires a modern compiler, GCC >= 4.8 or Clang >= 3.2 should suffice")
endif ()


check_cxx_source_runs("
  #include <unordered_map>

  int main(int, char **)
  {
      std::unordered_map<int, int> a;
      a.emplace(1, 1);
      std::unordered_map<int, int> b = std::move(a);
      a.emplace(1, 1);
      return 0;
  }" HAVE_UNORDERDED_MAP_WORKING_MOVE_CONSTRUCTOR)

install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/include/rct/rct-config.h
        rct/AES256CBC.h
        rct/Apply.h
        rct/Buffer.h
        rct/Connection.h
        rct/Config.h
        rct/EventLoop.h
        rct/FileSystemWatcher.h
        rct/List.h
        rct/Log.h
        rct/Map.h
        rct/SocketClient.h
        rct/SocketServer.h
        rct/MemoryMonitor.h
        rct/Message.h
        rct/MessageQueue.h
        rct/Messages.h
        rct/Path.h
        rct/Plugin.h
        rct/Point.h
        rct/Process.h
        rct/Rct.h
        rct/ReadLocker.h
        rct/ReadWriteLock.h
        rct/Rect.h
        rct/RegExp.h
        rct/ResponseMessage.h
        rct/Semaphore.h
        rct/Serializer.h
        rct/Set.h
        rct/SHA256.h
        rct/SharedMemory.h
        rct/SignalSlot.h
        rct/Size.h
        rct/StopWatch.h
        rct/String.h
        rct/Thread.h
        rct/ThreadLocal.h
        rct/ThreadPool.h
        rct/Timer.h
        rct/Value.h
        rct/WriteLocker.h
        DESTINATION include/rct)

install(EXPORT "rct" DESTINATION lib/cmake)
