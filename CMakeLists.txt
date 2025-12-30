cmake_minimum_required(VERSION 3.20)
project(netprobe VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler flags
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(
        -Wall -Wextra -Wpedantic
        -O3 -march=native
        -flto
    )
    add_link_options(-flto -static-libgcc -static-libstdc++)
endif()

# Source files
set(SOURCES
    src/main.cpp
    src/ansi.cpp
    src/argparse.cpp
    src/stats.cpp
    src/socket.cpp
    src/async_io.cpp
    src/commands/ping.cpp
    src/commands/trace.cpp
    src/commands/scan.cpp
    src/commands/bench.cpp
    src/commands/sniff.cpp
    src/commands/iperf.cpp
)

# Main executable
add_executable(netprobe ${SOURCES})

# Link pthread for threading support
target_link_libraries(netprobe PRIVATE pthread)

# Include directories
target_include_directories(netprobe PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

# Strip binary in release mode
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    add_custom_command(TARGET netprobe POST_BUILD
        COMMAND ${CMAKE_STRIP} $<TARGET_FILE:netprobe>
        COMMENT "Stripping binary for minimal size"
    )
endif()

# Install targets
install(TARGETS netprobe DESTINATION bin)
install(FILES man/netprobe.1 DESTINATION share/man/man1)
