include(FetchContent)

find_path(BOOST_TEST_INCLUDE_DIR boost/test/included/unit_test.hpp)

if(NOT BOOST_TEST_INCLUDE_DIR)
    message(STATUS "Boost.Test headers not found locally; fetching Boost source...")

    FetchContent_Declare(
        boost_source
        URL https://archives.boost.io/release/1.86.0/source/boost_1_86_0.tar.gz
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_MakeAvailable(boost_source)

    # Full Boost source has top-level "boost/" headers directly under source root.
    set(BOOST_TEST_INCLUDE_DIR "${boost_source_SOURCE_DIR}")
endif()

add_executable(validator_tests
    validator_test.cpp
)

target_link_libraries(validator_tests
    PRIVATE
        #validator_lib
)

target_include_directories(validator_tests
    PRIVATE
        ${BOOST_TEST_INCLUDE_DIR}
    ${PROJECT_SOURCE_DIR}/include
)

add_test(NAME validator_tests COMMAND validator_tests)
