cmake_minimum_required(VERSION 2.8)

project(rct_tests C CXX)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}")
include(FindCPPUNIT)
find_package(CPPUNIT REQUIRED)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CPPUNIT_CFLAGS}")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -Wall -Wextra -frtti")

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

link_directories(${CPPUNIT_LIBRARY_DIRS} ${PROJECT_BINARY_DIR})

set(BINARY_NAME "rct_tests")
add_executable(${BINARY_NAME} main.cpp DateTestSuite.cpp)
target_link_libraries(${BINARY_NAME} ${CPPUNIT_LIBRARIES} rct)

add_test("unittests" ${CMAKE_CURRENT_BINARY_DIR}/${BINARY_NAME})

add_executable("Path_tests"
  ../rct/Path.cpp ../rct/Rct.cpp ../rct/Log.cpp
  ../rct/WindowsUnicodeConversion.cpp main.cpp PathTestSuite.cpp)
target_link_libraries("Path_tests" ${CPPUNIT_LIBRARIES} Ws2_32)
