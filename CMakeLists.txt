cmake_minimum_required(VERSION 3.28)
project(asciimmo VERSION 0.1 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable C++20 modules support when using Ninja generator (requires CMake 3.28+ and GCC 14+)
# Note: Unix Makefiles doesn't support modules; use Ninja for module builds
if(CMAKE_GENERATOR MATCHES "Ninja")
  set(CMAKE_CXX_SCAN_FOR_MODULES ON)
  set(CMAKE_EXPERIMENTAL_CXX_MODULE_DYNDEP 1)
  message(STATUS "C++ modules enabled (Ninja generator detected)")
else()
  message(STATUS "C++ modules disabled (requires Ninja generator, current: ${CMAKE_GENERATOR})")
endif()

# Set default build type to Release if not specified
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type (Debug, Release, RelWithDebInfo, MinSizeRel)" FORCE)
endif()

# Compiler flags for optimized release builds
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  # Release: aggressive optimizations, no debug info
  set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG -march=native -flto")
  # RelWithDebInfo: optimizations + debug symbols
  set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O2 -g -DNDEBUG")
  # Debug: no optimizations, full debug info
  set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g")
elseif(MSVC)
  set(CMAKE_CXX_FLAGS_RELEASE "/O2 /DNDEBUG /GL")
  set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "/O2 /Zi /DNDEBUG")
  set(CMAKE_CXX_FLAGS_DEBUG "/Od /Zi")
endif()

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

# --- ProtoBuf support ---
find_package(Protobuf REQUIRED)
file(GLOB PROTO_FILES "${CMAKE_SOURCE_DIR}/proto/*.proto")

# C++ output - generate per-file custom commands for proper dependency tracking
set(PROTO_CPP_OUT_DIR "${CMAKE_SOURCE_DIR}/src/proto")
set(PROTO_SRCS)
set(PROTO_HDRS)
foreach(PROTO_FILE ${PROTO_FILES})
  get_filename_component(PROTO_NAME ${PROTO_FILE} NAME_WE)
  set(PROTO_SRC "${PROTO_CPP_OUT_DIR}/${PROTO_NAME}.pb.cc")
  set(PROTO_HDR "${PROTO_CPP_OUT_DIR}/${PROTO_NAME}.pb.h")
  
  add_custom_command(
    OUTPUT ${PROTO_SRC} ${PROTO_HDR}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${PROTO_CPP_OUT_DIR}
    COMMAND ${Protobuf_PROTOC_EXECUTABLE} --cpp_out=${PROTO_CPP_OUT_DIR} -I ${CMAKE_SOURCE_DIR}/proto ${PROTO_FILE}
    DEPENDS ${PROTO_FILE}
    COMMENT "Compiling ${PROTO_NAME}.proto to C++"
  )
  
  list(APPEND PROTO_SRCS ${PROTO_SRC})
  list(APPEND PROTO_HDRS ${PROTO_HDR})
endforeach()
add_custom_target(proto_cpp ALL DEPENDS ${PROTO_SRCS} ${PROTO_HDRS})

# JS output (optional; requires protoc-gen-js plugin)
set(PROTO_JS_OUT_DIR "${CMAKE_SOURCE_DIR}/client/proto")
find_program(PROTOC_GEN_JS_EXECUTABLE NAMES protoc-gen-js)
if(PROTOC_GEN_JS_EXECUTABLE)
  set(PROTO_JS_FILES)
  foreach(PROTO_FILE ${PROTO_FILES})
    get_filename_component(PROTO_NAME ${PROTO_FILE} NAME_WE)
    set(PROTO_JS "${PROTO_JS_OUT_DIR}/${PROTO_NAME}_pb.js")
    
    add_custom_command(
      OUTPUT ${PROTO_JS}
      COMMAND ${CMAKE_COMMAND} -E make_directory ${PROTO_JS_OUT_DIR}
      COMMAND ${Protobuf_PROTOC_EXECUTABLE} --plugin=protoc-gen-js=${PROTOC_GEN_JS_EXECUTABLE} --js_out=import_style=commonjs,binary:${PROTO_JS_OUT_DIR} -I ${CMAKE_SOURCE_DIR}/proto ${PROTO_FILE}
      DEPENDS ${PROTO_FILE}
      COMMENT "Compiling ${PROTO_NAME}.proto to JS"
    )
    
    list(APPEND PROTO_JS_FILES ${PROTO_JS})
  endforeach()
  add_custom_target(proto_js ALL DEPENDS ${PROTO_JS_FILES})
else()
  message(WARNING "protoc-gen-js not found; skipping JS proto generation. Install via: npm install -g protoc-gen-js")
endif()

# --- Client build (npm) ---
find_program(NPM_EXECUTABLE NAMES npm)
if(NPM_EXECUTABLE)
  set(CLIENT_DIR "${CMAKE_SOURCE_DIR}/client")
  set(CLIENT_BUNDLE "${CLIENT_DIR}/bundle.js")
  file(GLOB_RECURSE CLIENT_SOURCES 
    "${CLIENT_DIR}/app.js"
    "${CLIENT_DIR}/index.html"
    "${CLIENT_DIR}/package.json"
  )
  # Add proto JS files as dependencies
  list(APPEND CLIENT_SOURCES ${PROTO_JS_FILES})
  
  add_custom_command(
    OUTPUT ${CLIENT_BUNDLE}
    COMMAND ${NPM_EXECUTABLE} install
    COMMAND ${NPM_EXECUTABLE} run build
    WORKING_DIRECTORY ${CLIENT_DIR}
    DEPENDS ${CLIENT_SOURCES}
    COMMENT "Building client bundle with npm"
  )
  add_custom_target(client_build ALL DEPENDS ${CLIENT_BUNDLE})
  
  # Make client depend on proto_js if available
  if(TARGET proto_js)
    add_dependencies(client_build proto_js)
  endif()
else()
  message(WARNING "npm not found; skipping client build. Install Node.js to enable client bundling.")
endif()
