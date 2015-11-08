cmake_minimum_required(VERSION 2.8)

project(rct_tests C CXX)

include(FindPkgConfig)
pkg_search_module(CPPUNIT REQUIRED cppunit)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CPPUNIT_CFLAGS}")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -Wall -Wextra -Weffc++ -frtti")

add_definitions(
    ${CPPUNIT_CFLAGS_OTHER}
)

include_directories(
    ${PROJECT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CPPUNIT_INCLUDEDIR}
)

file(GLOB UNIT_TESTS_SRCS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

link_directories(${CPPUNIT_LIBRARY_DIRS} ${PROJECT_BINARY_DIR})

set(BINARY_NAME "rct_tests")
add_executable(${BINARY_NAME} ${UNIT_TESTS_SRCS})
target_link_libraries(${BINARY_NAME} ${CPPUNIT_LIBRARIES} rct)

add_test("unittests" ${CMAKE_CURRENT_BINARY_DIR}/${BINARY_NAME})
