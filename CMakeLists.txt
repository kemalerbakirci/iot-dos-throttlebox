cmake_minimum_required(VERSION 3.15)
project(iot-dos-throttlebox VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add compiler flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -O2")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0 -DDEBUG")

# Include directories
include_directories(include)

# Find required packages
find_package(Threads REQUIRED)

# Create library for throttlebox components
add_library(throttlebox_lib
    src/throttlebox.cpp
    src/rate_limiter.cpp
    src/config.cpp
    src/metrics.cpp
)

target_include_directories(throttlebox_lib PUBLIC include)
target_link_libraries(throttlebox_lib Threads::Threads)

# Create main executable
add_executable(throttlebox
    src/main.cpp
)

target_link_libraries(throttlebox throttlebox_lib)

# Optional: Enable testing
option(BUILD_TESTS "Build tests" OFF)

if(BUILD_TESTS)
    enable_testing()
    
    # Find testing framework (you might want to use Catch2 or GoogleTest)
    # For now, we'll create simple test executables
    
    add_executable(test_rate_limiter tests/test_rate_limiter.cpp)
    target_link_libraries(test_rate_limiter throttlebox_lib)
    add_test(NAME test_rate_limiter COMMAND test_rate_limiter)
    
    add_executable(test_config tests/test_config.cpp)
    target_link_libraries(test_config throttlebox_lib)
    add_test(NAME test_config COMMAND test_config)
    
    add_executable(test_throttlebox tests/test_throttlebox.cpp)
    target_link_libraries(test_throttlebox throttlebox_lib)
    add_test(NAME test_throttlebox COMMAND test_throttlebox)
endif()

# Installation
install(TARGETS throttlebox
    RUNTIME DESTINATION bin
)

install(DIRECTORY include/throttlebox
    DESTINATION include
)

# Package configuration
set(CPACK_PACKAGE_NAME "iot-dos-throttlebox")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "MQTT DoS Protection Reverse Proxy")
set(CPACK_PACKAGE_VENDOR "IoT Security Tools")

include(CPack)