cmake_minimum_required(VERSION 2.8)
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

if (WITH_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif ()

