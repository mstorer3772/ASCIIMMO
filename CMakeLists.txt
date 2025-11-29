cmake_minimum_required(VERSION 3.20)
project(asciimmo VERSION 0.1 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Fetch cpp-httplib (single-header) for HTTP endpoints
include(FetchContent)
FetchContent_Declare(
  cpp-httplib
  GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
  GIT_TAG        v0.11.0
)
FetchContent_MakeAvailable(cpp-httplib)

# Shared worldgen library
add_library(worldgen STATIC src/worldgen.cpp)
target_include_directories(worldgen PUBLIC include)

# Microservice binaries
add_executable(world-service src/world_service.cpp)
target_include_directories(world-service PRIVATE include ${cpp-httplib_SOURCE_DIR})
target_link_libraries(world-service PRIVATE worldgen)

add_executable(auth-service src/auth_service.cpp)
target_include_directories(auth-service PRIVATE include ${cpp-httplib_SOURCE_DIR})

add_executable(session-service src/session_service.cpp)
target_include_directories(session-service PRIVATE include ${cpp-httplib_SOURCE_DIR})

add_executable(social-service src/social_service.cpp)
target_include_directories(social-service PRIVATE include ${cpp-httplib_SOURCE_DIR})

# Legacy monolithic binary (optional; can be removed later)
add_executable(asciimmo-server
  src/main.cpp
  src/worldgen.cpp
  src/server_http.cpp
)
target_include_directories(asciimmo-server PRIVATE include ${cpp-httplib_SOURCE_DIR})

# Install rules
install(TARGETS world-service auth-service session-service social-service asciimmo-server RUNTIME DESTINATION bin)
