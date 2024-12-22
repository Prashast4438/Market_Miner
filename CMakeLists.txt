cmake_minimum_required(VERSION 3.10)
project(project)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(CURL REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(spdlog REQUIRED)
find_package(Boost REQUIRED COMPONENTS system)
find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)

# Create logs directory
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/logs)

# Add executable
add_executable(project 
    main.cpp
)

# Link libraries
target_link_libraries(project PRIVATE
    CURL::libcurl
    nlohmann_json::nlohmann_json
    spdlog::spdlog
    Boost::system
    OpenSSL::SSL
    OpenSSL::Crypto
    Threads::Threads
)

# Add compiler optimizations for release build
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_options(project PRIVATE -O3 -march=native)
endif()

# Add include directories for Boost
target_include_directories(project PRIVATE ${Boost_INCLUDE_DIRS})
