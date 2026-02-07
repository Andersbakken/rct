cmake_minimum_required(VERSION 3.8.2)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig)

if (POLICY CMP0025)
  cmake_policy(SET CMP0025 NEW)
endif ()

if (POLICY CMP0042)
  cmake_policy(SET CMP0042 NEW)
endif ()

if (POLICY CMP0067)
  cmake_policy(SET CMP0067 NEW)
endif ()

if (NOT RCT_NO_LIBRARY)
    project(rct)
endif ()

set(CMAKE_OLD_MODULE_PATH ${CMAKE_MODULE_PATH})
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_LIST_DIR}/cmake/")

set(RCT_LIBRARIES pthread)

include(CheckSymbolExists)
include(CheckCXXSymbolExists)
include(CheckCXXSourceCompiles)
include(CheckCXXSourceRuns)
include(GNUInstallDirs)

include(${CMAKE_CURRENT_LIST_DIR}/compiler.cmake)
set(CMAKE_MODULE_PATH ${CMAKE_OLD_MODULE_PATH})

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
  message("-- Using win32 FileSystemWatcher")
  set(HAVE_CHANGENOTIFICATION 1)
  set(HAVE_CYGWIN 1)
endif ()

if (CMAKE_SYSTEM_NAME MATCHES "Darwin")
  execute_process(COMMAND sw_vers -productVersion OUTPUT_VARIABLE OSX_VERSION OUTPUT_STRIP_TRAILING_WHITESPACE)
  message("OS X version ${OSX_VERSION}")
  string(REGEX REPLACE "^([0-9]+)\\.[0-9]+\\..*$" "\\1" OSX_MAJOR ${OSX_VERSION})
  string(REGEX REPLACE "^[0-9]+\\.([0-9]+)\\..*$" "\\1" OSX_MINOR ${OSX_VERSION})
  if (${OSX_MAJOR} EQUAL 10 AND ${OSX_MINOR} GREATER 6)
    set(HAVE_FSEVENTS 1)
    message("-- Using FSEvents")
  elseif (${OSX_MAJOR} GREATER 10)
    set(HAVE_FSEVENTS 1)
    message("-- Using FSEvents")
  else ()
    message("-- Using kqueue")
  endif ()
endif ()

if (CMAKE_SYSTEM_NAME MATCHES "Windows")
  add_definitions(-D_WIN32_WINNT=_WIN32_WINNT_VISTA)
  set(HAVE_CHANGENOTIFICATION 1)
  set(HAVE_SELECT 1)
  list (APPEND RCT_LIBRARIES Ws2_32)
endif ()

check_cxx_source_compiles("
  #include <sys/types.h>
  #include <sys/stat.h>
  int main(int, char**) {
      struct stat st;
      return st.st_mtim.tv_sec;
  }" HAVE_STATMTIM)

if (NOT DEFINED RCT_INCLUDE_DIR)
  set(RCT_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/include")
endif ()

set(RCT_DEFINITIONS "")
set(RCT_INCLUDE_DIRS "")
set(RCT_SYSTEM_INCLUDE_DIRS "")
find_package(ZLIB)
if (NOT ZLIB_FOUND AND PKGCONFIG_FOUND)
    pkg_search_module(ZLIB zlib)
    if (ZLIB_FOUND)
        set(ZLIB_LIBRARY ${ZLIB_LIBRARIES})
    endif ()
endif()

if (ZLIB_FOUND)
    set(RCT_DEFINITIONS ${RCT_DEFINITIONS} -DRCT_HAVE_ZLIB)
    list(APPEND RCT_SYSTEM_INCLUDE_DIRS ${ZLIB_INCLUDE_DIRS})
else ()
    message("ZLIB Can't be found. Rct configured without zlib support")
endif ()
find_package(OpenSSL)
if (NOT OPENSSL_FOUND AND PKGCONFIG_FOUND)
    pkg_search_module(OPENSSL openssl)
endif()

if (OPENSSL_FOUND)
    set(RCT_DEFINITIONS ${RCT_DEFINITIONS} -DRCT_HAVE_OPENSSL)
    list(APPEND RCT_SOURCES ${CMAKE_CURRENT_LIST_DIR}/rct/AES256CBC.cpp ${CMAKE_CURRENT_LIST_DIR}/rct/SHA256.cpp)
    if (OPENSSL_INCLUDE_DIR)
        list(APPEND RCT_SYSTEM_INCLUDE_DIRS ${OPENSSL_INCLUDE_DIR})
    endif ()
    if (OPENSSL_INCLUDE_DIRS)
        list(APPEND RCT_SYSTEM_INCLUDE_DIRS ${OPENSSL_INCLUDE_DIRS})
    endif ()
else ()
    message("OPENSSL Can't be found. Rct configured without openssl support")
endif ()

list(APPEND RCT_INCLUDE_DIRS ${CMAKE_CURRENT_LIST_DIR} ${RCT_INCLUDE_DIR})

set(RCT_SOURCES
  ${RCT_SOURCES}
  ${CMAKE_CURRENT_LIST_DIR}/cJSON/cJSON.c
  ${CMAKE_CURRENT_LIST_DIR}/rct/Buffer.cpp
  ${CMAKE_CURRENT_LIST_DIR}/rct/Config.cpp
  ${CMAKE_CURRENT_LIST_DIR}/rct/Connection.cpp
  ${CMAKE_CURRENT_LIST_DIR}/rct/CpuUsage.cpp
  ${CMAKE_CURRENT_LIST_DIR}/rct/Date.cpp
  ${CMAKE_CURRENT_LIST_DIR}/rct/EventLoop.cpp
  ${CMAKE_CURRENT_LIST_DIR}/rct/FileSystemWatcher.cpp
  ${CMAKE_CURRENT_LIST_DIR}/rct/Log.cpp
  ${CMAKE_CURRENT_LIST_DIR}/rct/MemoryMappedFile.cpp
  ${CMAKE_CURRENT_LIST_DIR}/rct/MemoryMonitor.cpp
  ${CMAKE_CURRENT_LIST_DIR}/rct/Message.cpp
  ${CMAKE_CURRENT_LIST_DIR}/rct/MessageQueue.cpp
  ${CMAKE_CURRENT_LIST_DIR}/rct/Path.cpp
  ${CMAKE_CURRENT_LIST_DIR}/rct/Plugin.cpp
  ${CMAKE_CURRENT_LIST_DIR}/rct/Rct.cpp
  ${CMAKE_CURRENT_LIST_DIR}/rct/ReadWriteLock.cpp
  ${CMAKE_CURRENT_LIST_DIR}/rct/Semaphore.cpp
  ${CMAKE_CURRENT_LIST_DIR}/rct/SharedMemory.cpp
  ${CMAKE_CURRENT_LIST_DIR}/rct/SocketClient.cpp
  ${CMAKE_CURRENT_LIST_DIR}/rct/SocketServer.cpp
  ${CMAKE_CURRENT_LIST_DIR}/rct/String.cpp
  ${CMAKE_CURRENT_LIST_DIR}/rct/Thread.cpp
  ${CMAKE_CURRENT_LIST_DIR}/rct/ThreadPool.cpp
  ${CMAKE_CURRENT_LIST_DIR}/rct/Timer.cpp
  ${CMAKE_CURRENT_LIST_DIR}/rct/Value.cpp
  ${CMAKE_CURRENT_LIST_DIR}/rct/demangle.cpp)

if (HAVE_INOTIFY EQUAL 1)
  list(APPEND RCT_SOURCES ${CMAKE_CURRENT_LIST_DIR}/rct/FileSystemWatcher_inotify.cpp)
elseif (HAVE_FSEVENTS EQUAL 1)
  list(APPEND RCT_SOURCES ${CMAKE_CURRENT_LIST_DIR}/rct/FileSystemWatcher_fsevents.cpp)
elseif (HAVE_KQUEUE EQUAL 1)
  list(APPEND RCT_SOURCES ${CMAKE_CURRENT_LIST_DIR}/rct/FileSystemWatcher_kqueue.cpp)
elseif (HAVE_CHANGENOTIFICATION EQUAL 1)
  list(APPEND RCT_SOURCES ${CMAKE_CURRENT_LIST_DIR}/rct/FileSystemWatcher_win32.cpp)
endif ()

if (CMAKE_SYSTEM_NAME MATCHES "Windows")
    list(APPEND RCT_SOURCES
        ${CMAKE_CURRENT_LIST_DIR}/rct/Process_Windows.cpp
        ${CMAKE_CURRENT_LIST_DIR}/rct/WindowsUnicodeConversion.cpp)
else ()
     list(APPEND RCT_SOURCES ${CMAKE_CURRENT_LIST_DIR}/rct/Process.cpp)
endif ()


configure_file(${CMAKE_CURRENT_LIST_DIR}/rct/rct-config.h.in ${RCT_INCLUDE_DIR}/rct/rct-config.h.gen)
if (EXISTS "${RCT_INCLUDE_DIR}/rct/rct-config.h")
  file(READ "${RCT_INCLUDE_DIR}/rct/rct-config.h" cfif_output)
else()
  set(cfif_output "")
endif ()
file(READ "${RCT_INCLUDE_DIR}/rct/rct-config.h.gen" cfif_output_gen)
if (NOT "${cfif_output}" STREQUAL "${cfif_output_gen}")
  file (WRITE "${RCT_INCLUDE_DIR}/rct/rct-config.h" "${cfif_output_gen}")
endif ()
file(REMOVE "${RCT_INCLUDE_DIR}/rct/rct-config.h.gen")

set(RCT_DEFINITIONS ${RCT_DEFINITIONS} -DOS_${CMAKE_SYSTEM_NAME})
if (RCT_SERIALIZER_VERIFY_PRIMITIVE_SIZE)
  set(RCT_DEFINITIONS ${RCT_DEFINITIONS} -DRCT_SERIALIZER_VERIFY_PRIMITIVE_SIZE=1)
endif ()
add_definitions(${RCT_DEFINITIONS})
if (NOT RCT_NO_LIBRARY)
    include_directories(${RCT_INCLUDE_DIRS})
    include_directories(SYSTEM ${RCT_SYSTEM_INCLUDE_DIRS})
    if (RCT_STATIC)
        add_library(rct STATIC ${RCT_SOURCES})
    else()
        add_library(rct SHARED ${RCT_SOURCES})
    endif ()
endif ()

if (RCT_EVENTLOOP_CALLBACK_TIME_THRESHOLD)
  set(RCT_DEFINITIONS ${RCT_DEFINITIONS} "-DRCT_EVENTLOOP_CALLBACK_TIME_THRESHOLD=${RCT_EVENTLOOP_CALLBACK_TIME_THRESHOLD}")
endif ()

if (CMAKE_SYSTEM_NAME MATCHES "Darwin")
  find_library(CORESERVICES_LIBRARY CoreServices)
  find_path(CORESERVICES_INCLUDE "CoreServices/CoreServices.h")
  find_library(COREFOUNDATION_LIBRARY CoreFoundation)
  find_path(COREFOUNDATION_INCLUDE "CoreFoundation/CoreFoundation.h")
endif ()

if(ZLIB_FOUND)
    list(APPEND RCT_LIBRARIES ${ZLIB_LIBRARY})
endif()
if(OPENSSL_FOUND)
    list(APPEND RCT_LIBRARIES ${OPENSSL_CRYPTO_LIBRARY})
endif()
if (CMAKE_SYSTEM_NAME MATCHES "Linux")
  list(APPEND RCT_LIBRARIES dl rt)
endif ()
if (CMAKE_SYSTEM_NAME MATCHES "Darwin")
  list(APPEND RCT_LIBRARIES ${CORESERVICES_LIBRARY} ${COREFOUNDATION_LIBRARY})
endif ()
if (${V8_LIBRARIES})
    list(APPEND RCT_LIBRARIES ${V8_LIBRARIES})
endif ()

if (NOT RCT_NO_LIBRARY)
    target_link_libraries(rct ${RCT_LIBRARIES})
    set_target_properties(rct PROPERTIES INTERFACE_LINK_LIBRARIES "${RCT_LIBRARIES}")
    set_target_properties(rct PROPERTIES LINK_INTERFACE_LIBRARIES "${RCT_LIBRARIES}")
endif ()

if (NOT RCT_NO_INSTALL)
  install(CODE "message(\"Installing rct...\")")
  install(TARGETS rct EXPORT rct LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
endif ()

if (RCT_USE_LIBCXX)
  set(CMAKE_REQUIRED_LIBRARIES "${CMAKE_EXE_LINKER_FLAGS}")
endif ()

if (ASAN)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=address")
endif ()

check_cxx_source_compiles("
  #include <functional>
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
  #include <string>

  int main(int, char **)
  {
      std::string str = \"foobar testing\";
      std::string::iterator it = str.erase(str.begin(), str.end());
      return 0;
  }" HAVE_STRING_ITERATOR_ERASE)

unset(CMAKE_REQUIRED_FLAGS)
unset(CMAKE_REQUIRED_LIBRARIES)

if (RCT_WITH_TESTS)
    enable_testing()
    add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/tests)

    if (${CMAKE_CXX_COMPILER} MATCHES "clang")
        find_program(LLVM_COV_EXECUTABLE
            NAMES
            llvm-cov
            llvm-cov35
            llvm-cov36
            llvm-cov37
            )

        add_custom_target(
            gen_llvm_cov_wrapper_script
            COMMAND echo ${LLVM_COV_EXECUTABLE} gcov $@ | tee llvm-cov.sh
            COMMAND chmod a+x llvm-cov.sh
            VERBATIM
        )

        set(GCOV_TOOL ./llvm-cov.sh)
    else ()
        find_program(GCOV_EXECUTABLE
            NAMES
            gcov47
            gcov48
        )

        add_custom_target(
            gen_llvm_cov_wrapper_script
            COMMAND true
            VERBATIM
        )

        set(GCOV_TOOL ${GCOV_EXECUTABLE})
    endif ()

    find_program(LCOV_EXECUTABLE NAMES lcov)
    find_program(GENHTML_EXECUTABLE NAMES genhtml)

    if (GCOV_TOOL AND LCOV_EXECUTABLE AND GENHTML_EXECUTABLE)
        add_custom_target(
            coverage
            COMMAND ${LCOV_EXECUTABLE} --directory . --base-directory . --gcov-tool ${GCOV_TOOL} -capture -o cov.info
            COMMAND ${GENHTML_EXECUTABLE} cov.info -o output
            DEPENDS gen_llvm_cov_wrapper_script
            VERBATIM
        )
    endif ()

endif ()

if (NOT RCT_NO_INSTALL)
  install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/include/rct/rct-config.h
    rct/AES256CBC.h
    rct/Apply.h
    rct/Buffer.h
    rct/Config.h
    rct/Connection.h
    rct/EventLoop.h
    rct/FileSystemWatcher.h
    rct/List.h
    rct/Log.h
    rct/Map.h
    rct/MemoryMonitor.h
    rct/Message.h
    rct/MessageQueue.h
    rct/Path.h
    rct/Plugin.h
    rct/Point.h
    rct/Process.h
    rct/Rct.h
    rct/ReadLocker.h
    rct/ReadWriteLock.h
    rct/Rect.h
    rct/ResponseMessage.h
    rct/SHA256.h
    rct/Semaphore.h
    rct/Serializer.h
    rct/Set.h
    rct/SharedMemory.h
    rct/SignalSlot.h
    rct/Size.h
    rct/SocketClient.h
    rct/SocketServer.h
    rct/StopWatch.h
    rct/String.h
    rct/StringTokenizer.h
    rct/Thread.h
    rct/ThreadLocal.h
    rct/ThreadPool.h
    rct/Timer.h
    rct/Value.h
    rct/WriteLocker.h
    DESTINATION include/rct)

  install(EXPORT "rct" DESTINATION lib/cmake)
endif ()
