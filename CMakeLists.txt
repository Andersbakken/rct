cmake_minimum_required(VERSION 2.8)

if (RCT_WITH_TESTS)
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O0 -Wall -fprofile-arcs -ftest-coverage")
    SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -O0 -Wall -W -fprofile-arcs -ftest-coverage")
    SET(CMAKE_EXE_LINKER_FLAGS "-fprofile-arcs -ftest-coverage")
endif ()

# If building with tests, set flags to generate profile/coverage data
# for the compilation of the main code.
include(rct.cmake)
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti")
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fstack-protector-all")
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wstack-protector")
# set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall")
# set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fstack-protector-all")
# set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wstack-protector")

# add_executable(dbtest rct/dbtest.cpp)
# target_link_libraries(dbtest rct)


