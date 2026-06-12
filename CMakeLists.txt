cmake_minimum_required(VERSION 3.20)
project(validator VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

#add_library(validator_lib
#)

#target_include_directories(validator_lib
#    PUBLIC
#        ${PROJECT_SOURCE_DIR}/include
#)

add_executable(validator_app
    src/main.cpp
)

target_link_libraries(validator_app
    PRIVATE
        #validator_lib
)

include(CTest)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
