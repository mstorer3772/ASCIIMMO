cmake_minimum_required(VERSION 3.20)
project(asciimmo-server VERSION 0.1 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(asciimmo-server
  src/main.cpp
  src/worldgen.cpp
  src/server_http.cpp
)

target_include_directories(asciimmo-server PRIVATE include)

# Fetch cpp-httplib (single-header) for a simple HTTP endpoint
include(FetchContent)
FetchContent_Declare(
  cpp-httplib
  GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
  GIT_TAG        v0.11.0
)
FetchContent_MakeAvailable(cpp-httplib)

# Ensure include path to fetched cpp-httplib root (contains httplib.h)
FetchContent_GetProperties(cpp-httplib)
if (cpp-httplib_SOURCE_DIR)
  target_include_directories(asciimmo-server PRIVATE ${cpp-httplib_SOURCE_DIR})
endif()

# Install rule (optional)
install(TARGETS asciimmo-server RUNTIME DESTINATION bin)
