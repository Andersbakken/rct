cmake_minimum_required(VERSION 2.8)

project(rct_tests C CXX)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}")
include(FindCPPUNIT)
find_package(CPPUNIT REQUIRED)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CPPUNIT_CFLAGS}")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -Wall -Wextra -frtti -std=c++11")

include_directories(
    ${PROJECT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CPPUNIT_INCLUDE_DIRS}
)

if (CMAKE_SYSTEM_NAME MATCHES "Windows")
  add_definitions(-D_WIN32_WINNT=_WIN32_WINNT_VISTA)
endif ()

link_directories(${CPPUNIT_LIBRARY_DIRS} ${PROJECT_BINARY_DIR} ${RCT_BINARY_DIR})

set(BINARY_NAME "rct_tests")
add_executable(${BINARY_NAME} main.cpp DateTestSuite.cpp PathTestSuite.cpp)
target_link_libraries(${BINARY_NAME} ${CPPUNIT_LIBRARIES} rct)
add_test("unittests" ${CMAKE_CURRENT_BINARY_DIR}/${BINARY_NAME})


##################################################################

set(WIN_RCT_SOURCES Path.cpp Rct.cpp Log.cpp WindowsUnicodeConversion.cpp
  Buffer.cpp Config.cpp Value.cpp Date.cpp Connection.cpp EventLoop.cpp
  SocketClient.cpp Message.cpp String.cpp FileSystemWatcher_win32.cpp
  MemoryMonitor.cpp MessageQueue.cpp Plugin.cpp ReadWriteLock.cpp
  SHA256.cpp ScriptEngine.cpp Thread.cpp ThreadPool.cpp Timer.cpp
  Semaphore.cpp SharedMemory.cpp)

#missing: Process.cpp

foreach(l IN LISTS WIN_RCT_SOURCES)
  list(APPEND WIN_RCT_SOURCES_PREPENDED ../rct/${l})
endforeach()
set(WIN_RCT_SOURCES ${WIN_RCT_SOURCES_PREPENDED})

list(APPEND WIN_RCT_SOURCES ../cJSON/cJSON.c)

add_definitions(-D_CRTIMP= )

find_package(OpenSSL)
if (OPENSSL_FOUND)
    add_definitions(-DRCT_HAVE_OPENSSL)
    list(APPEND WIN_RCT_SOURCES ../rct/AES256CBC.cpp ../rct/SHA256.cpp)
    include_directories(${OPENSSL_INCLUDE_DIR})
else ()
    message("OPENSSL can't be found. Win test conf'd without openssl support")
endif ()

add_executable("Path_tests" main.cpp PathTestSuite.cpp ${WIN_RCT_SOURCES})
target_link_libraries("Path_tests" ${CPPUNIT_LIBRARIES}
  ${OPENSSL_LIBRARIES} Ws2_32 msvcr100)
