cmake_minimum_required(VERSION 3.10)
project(KTerm C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Compile options
add_compile_options(-Wall -Wextra -O2)

# Find Math library
find_library(M_LIB m)

# Find Situation library (Assuming it's installed in system paths)
# If Situation provides a CMake config, find_package(Situation) would be better,
# but for now we look for the library file.
find_library(SITUATION_LIB situation)

if (NOT SITUATION_LIB)
    message(WARNING "Situation library not found. Assuming mock/headless build.")
    set(SITUATION_LIB "")
endif()

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

# --- Examples ---

# Telnet Client
add_executable(telnet_client telnet_client.c)
target_link_libraries(telnet_client ${SITUATION_LIB} ${M_LIB})

# SSH Client (Reference Implementation)
add_executable(ssh_client ssh_client.c)
target_link_libraries(ssh_client ${SITUATION_LIB} ${M_LIB})

# Net Server
add_executable(net_server example/net_server.c)
target_link_libraries(net_server ${SITUATION_LIB} ${M_LIB})

# SSH Sodium (Optional - Requires libsodium)
find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
    pkg_check_modules(SODIUM libsodium)
    if(SODIUM_FOUND)
        add_executable(ssh_sodium example/ssh_sodium.c)
        target_include_directories(ssh_sodium PRIVATE ${SODIUM_INCLUDE_DIRS})
        target_link_libraries(ssh_sodium ${SITUATION_LIB} ${M_LIB} ${SODIUM_LIBRARIES})
        message(STATUS "Building ssh_sodium example (libsodium found)")
    else()
        message(STATUS "Skipping ssh_sodium example (libsodium not found)")
    endif()
else()
    # Manual check
    find_library(SODIUM_LIB sodium)
    if(SODIUM_LIB)
        add_executable(ssh_sodium example/ssh_sodium.c)
        target_link_libraries(ssh_sodium ${SITUATION_LIB} ${M_LIB} ${SODIUM_LIB})
        message(STATUS "Building ssh_sodium example (libsodium found)")
    endif()
endif()

# Speedtest Client
add_executable(speedtest_client example/speedtest_client.c)
target_link_libraries(speedtest_client ${SITUATION_LIB} ${M_LIB})
