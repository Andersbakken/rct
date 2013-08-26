cmake_minimum_required(VERSION 2.8)
project(rct)
include(rct.cmake)
add_definitions(-DOS_${CMAKE_SYSTEM_NAME})
add_library(rct STATIC ${RCT_SOURCES})
#add_library(rct SHARED ${RCT_SOURCES})

if ( HAVE_FAM EQUAL 1 )
  message( STATUS "Using FAM via Gamin Library..." )
  target_link_libraries(rct
    ${GAMIN_LDFLAGS}
    )
endif()

include_directories( 
  /usr/local/include
)

target_link_libraries(rct
  -L/usr/local/lib
  -levent_pthreads
  -levent
)

install(CODE "message(\"Installing rct...\")")
install(TARGETS rct DESTINATION lib COMPONENT rct EXPORT rct)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pedantic -O0 -g -Wformat -std=gnu++0x")
if (CMAKE_SYSTEM_NAME MATCHES "Darwin")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
endif ()

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
  }" HAVE_CXX11)

if (NOT HAVE_CXX11)
  message(FATAL_ERROR "C++11 support not detected. rct requires a modern compiler, GCC >= 4.7 or Clang >= 3.2 should suffice")
endif ()

install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/include/rct/rct-config.h
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
