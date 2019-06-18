cmake_minimum_required(VERSION 2.8)

project(rct_tests C CXX)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}")
find_package(CPPUNIT REQUIRED)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CPPUNIT_CFLAGS} -g -Wall -Wextra -frtti -std=c++11")

include_directories(
    ${PROJECT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CPPUNIT_INCLUDE_DIRS}
    ${RCT_INCLUDE_DIRS}
    ${RCT_BINARY_DIR}/include
    )

if (CMAKE_SYSTEM_NAME MATCHES "Windows")
    add_definitions(-D_WIN32_WINNT=_WIN32_WINNT_VISTA)
endif ()

link_directories(${CPPUNIT_LIBRARY_DIRS} ${PROJECT_BINARY_DIR} ${RCT_BINARY_DIR})

set(RCT_TEST_SRCS main.cpp PathTestSuite.cpp MemoryMappedFileTestSuite.cpp StringTokenizerTestSuite.cpp)
if (OPENSSL_FOUND)
    list(APPEND RCT_TEST_SRCS SHA256TestSuite.cpp)
endif ()

if (NOT CMAKE_SYSTEM_NAME MATCHES "Windows")
    list(APPEND RCT_TEST_SRCS DateTestSuite.cpp)
endif()

if (NOT CMAKE_SYSTEM_NAME MATCHES "Darwin")
    list(APPEND RCT_TEST_SRCS ProcessTestSuite.cpp)
endif()

if (RCT_NO_LIBRARY)
    list(APPEND RCT_TEST_SRCS ${RCT_SOURCES})
endif ()

set(BINARY_NAME "rct_tests")
add_executable(${BINARY_NAME} ${RCT_TEST_SRCS})

if (NOT RCT_NO_LIBRARY)
    target_link_libraries(${BINARY_NAME} ${CPPUNIT_LIBRARIES} rct pthread)
else ()
    target_link_libraries(${BINARY_NAME} ${CPPUNIT_LIBRARIES} pthread ${RCT_LIBRARIES})
endif ()

add_test("unittests" ${CMAKE_CURRENT_BINARY_DIR}/${BINARY_NAME})

add_executable("ChildProcess" ChildProcess.cpp)
target_link_libraries("ChildProcess" pthread)

if (CMAKE_SYSTEM_NAME MATCHES "Windows")
    target_link_libraries("ChildProcess" Ws2_32)
    target_link_libraries(${BINARY_NAME} Ws2_32)
endif()

# Create this one for unicode test: See if the Process class can start
# a program that his special characters in it.
add_custom_command(
    TARGET "ChildProcess" POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
    ${CMAKE_CURRENT_BINARY_DIR}/ChildProcess${CMAKE_EXECUTABLE_SUFFIX}
    ${CMAKE_CURRENT_BINARY_DIR}/ChildProcess_Äßéמש最終${CMAKE_EXECUTABLE_SUFFIX})

# Create a file with special chars for the MemoryMappedFile tests.
if (NOT APPLE)
    add_custom_command(
        TARGET "ChildProcess" POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_CURRENT_SOURCE_DIR}/testfile_Äßéמש最終.txt
        ${CMAKE_CURRENT_BINARY_DIR})
endif ()
