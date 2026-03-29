cmake_minimum_required(VERSION 3.15)
project(P2PChat VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)
set(CMAKE_TLS_VERIFY OFF)
FetchContent_Declare(
    glfw
    URL https://github.com/glfw/glfw/releases/download/3.4/glfw-3.4.zip
    DOWNLOAD_EXTRACT_TIMESTAMP true
)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glfw)

FetchContent_Declare(
    stb
    URL https://github.com/nothings/stb/archive/refs/heads/master.zip
    DOWNLOAD_EXTRACT_TIMESTAMP true
)
FetchContent_MakeAvailable(stb)
# --------------------------------------

include_directories(include)
include_directories(lib/imgui) 

include_directories(lib/json)
include_directories(lib/asio)         
include_directories(lib/sodium/include)
include_directories(${stb_SOURCE_DIR})

add_definitions(-DASIO_STANDALONE)

# -----------------------------------------------------

set(SOURCES
    src/main.cpp
    src/NetworkManager.cpp
    src/EncryptionEngine.cpp
    src/StorageManager.cpp
    src/Interface.cpp
)

file(GLOB IMGUI_SOURCES "lib/imgui/*.cpp")

add_executable(${PROJECT_NAME} ${SOURCES} ${IMGUI_SOURCES})

if(WIN32)
    # ================= WINDOWS (MinGW64) =================
    target_link_libraries(${PROJECT_NAME} PRIVATE 
        glfw 
        opengl32 
        "${CMAKE_CURRENT_SOURCE_DIR}/lib/sodium/libsodium.a"
        ws2_32      # Sockets
        mswsock     # Extensiones de Sockets para Asio
        advapi32    # Requerido por libsodium para entropía
        crypt32     # Requerido por libsodium
        imm32       # Requerido por ImGui en algunos casos
        gdi32                                                                               
    )
elseif(UNIX AND NOT APPLE)
    # ================= LINUX =================
    target_link_libraries(${PROJECT_NAME} PRIVATE 
        glfw 
        GL       
        sodium   
        pthread  
    )
endif()

