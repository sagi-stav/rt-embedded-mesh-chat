cmake_minimum_required(VERSION 3.20)
project(rt_embedded_mesh_chat C)

# Set strict C standard and compiler warnings (Crucial for Embedded systems)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Werror -pedantic -g")

# Define headers directory
include_directories(include)

# Direct the Linker to the directory containing our shared library
link_directories(${CMAKE_SOURCE_DIR}/lib)

# Set RPATH to burn the library path into the executable
# This allows Linux to find the .so file in the lib directory without LD_LIBRARY_PATH
set(CMAKE_BUILD_RPATH "${CMAKE_SOURCE_DIR}/lib")

# Shared source files
set(SHARED_SOURCES
        src/protocol.c
)

# ---------------------------------------------------------
# 1. Server Executable
# ---------------------------------------------------------
add_executable(server_app
        src/server_main.c
        src/server_mng.c
        src/server_net.c
        ${SHARED_SOURCES}
)
# Link the dynamic data structures library to the server
target_link_libraries(server_app data_structures)

# ---------------------------------------------------------
# 2. Client Executable
# ---------------------------------------------------------
add_executable(client_app
        src/client_main.c
        src/client_mng.c
        src/client_net.c
        src/client_ui.c
        ${SHARED_SOURCES}
)
# Link the dynamic data structures library to the client
target_link_libraries(client_app data_structures)

# ---------------------------------------------------------
# 3. Multicast Chat Applications
# ---------------------------------------------------------
add_executable(sender.out src/sender.c)
add_executable(receive.out src/receiver.c)