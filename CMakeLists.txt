cmake_minimum_required(VERSION 3.20)
project(rt_embedded_mesh_chat C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Werror -pedantic -g")

include_directories(include)

# ─── Data Structures Shared Library ───────────────────
add_library(data_structures SHARED
        src/ds/HashMap.c
        src/ds/vector.c
        src/ds/genqueue.c
        src/ds/gen_dlist.c
)
target_include_directories(data_structures PUBLIC include)

set_target_properties(data_structures PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/lib
)

# ─── Shared Sources ────────────────────────────────────
set(SHARED_SOURCES src/protocol.c)

# ─── Server ────────────────────────────────────────────
add_executable(server_app
        src/server_main.c
        src/server_mng.c
        src/server_net.c
        ${SHARED_SOURCES}
)
target_link_libraries(server_app data_structures)

# ─── Client ────────────────────────────────────────────
add_executable(client_app
        src/client_main.c
        src/client_mng.c
        src/client_net.c
        src/client_ui.c
        ${SHARED_SOURCES}
)
target_link_libraries(client_app data_structures)

# ─── Multicast Apps ────────────────────────────────────
add_executable(sender.out src/sender.c)
add_executable(receiver.out src/receiver.c)