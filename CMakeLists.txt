cmake_minimum_required(VERSION 3.28)
project(asciimmo VERSION 0.1 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
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
  set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG -march=native -flto=auto")
  # RelWithDebInfo: optimizations + debug symbols
  set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O2 -g -DNDEBUG")
  # Debug: no optimizations, full debug info
  set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g")
elseif(MSVC)
  set(CMAKE_CXX_FLAGS_RELEASE "/O2 /DNDEBUG /GL")
  set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "/O2 /Zi /DNDEBUG")
  set(CMAKE_CXX_FLAGS_DEBUG "/Od /Zi")
endif()

# Find Boost for Beast HTTP server and Asio async I/O
find_package(Boost REQUIRED COMPONENTS system)

# Find OpenSSL for HTTPS support
find_package(OpenSSL REQUIRED)

# Find yaml-cpp for YAML configuration parsing
find_package(yaml-cpp REQUIRED)

# Find PostgreSQL and bring in a modern libpqxx (built from source)
include(FetchContent)
find_package(PostgreSQL REQUIRED)

# Use FetchContent to pull libpqxx 7.10.1 to match C++20 features
FetchContent_Declare(
  libpqxx
  GIT_REPOSITORY https://github.com/jtv/libpqxx.git
  GIT_TAG        7.10.1
)
FetchContent_MakeAvailable(libpqxx)

# Database utilities library
add_library(db_utils STATIC src/db_pool.cpp src/db_config.cpp)
target_include_directories(db_utils PUBLIC include ${PostgreSQL_INCLUDE_DIRS})
target_link_libraries(db_utils PUBLIC libpqxx::pqxx PostgreSQL::PostgreSQL)

# Shared HTTP server library using Boost.Beast with HTTPS support
add_library(http_server STATIC src/shared/http_server.cpp)
target_include_directories(http_server PUBLIC include ${Boost_INCLUDE_DIRS})
target_link_libraries(http_server PUBLIC Boost::system OpenSSL::SSL OpenSSL::Crypto)

# Shared worldgen library
add_library(worldgen STATIC src/worldgen.cpp)
target_include_directories(worldgen PUBLIC include)

# Microservice binaries
add_executable(world-service src/world_service.cpp)
target_include_directories(world-service PRIVATE include)
target_link_libraries(world-service PRIVATE worldgen http_server Boost::system yaml-cpp)

add_executable(auth-service src/auth_service.cpp)
target_include_directories(auth-service PRIVATE include)
target_link_libraries(auth-service PRIVATE http_server db_utils pqxx Boost::system OpenSSL::SSL OpenSSL::Crypto yaml-cpp)

add_executable(session-service src/session_service.cpp)
target_include_directories(session-service PRIVATE include)
target_link_libraries(session-service PRIVATE http_server Boost::system yaml-cpp)

add_executable(social-service src/social_service.cpp)
target_include_directories(social-service PRIVATE include)
target_link_libraries(social-service PRIVATE http_server Boost::system yaml-cpp)

# Install rules
install(TARGETS world-service auth-service session-service social-service RUNTIME DESTINATION bin)

# --- Testing with Google Test ---
enable_testing()
find_package(GTest REQUIRED)
include(GoogleTest)

# WorldGen tests
add_executable(worldgen_test tests/worldgen_test.cpp)
target_link_libraries(worldgen_test PRIVATE worldgen GTest::gtest GTest::gtest_main)
target_include_directories(worldgen_test PRIVATE include)
gtest_discover_tests(worldgen_test)

# TokenCache tests
add_executable(token_cache_test tests/token_cache_test.cpp)
target_link_libraries(token_cache_test PRIVATE GTest::gtest GTest::gtest_main)
target_include_directories(token_cache_test PRIVATE include)
gtest_discover_tests(token_cache_test)

# World service tests
add_executable(world_service_test tests/world_service_test.cpp)
target_link_libraries(world_service_test PRIVATE worldgen GTest::gtest GTest::gtest_main)
target_include_directories(world_service_test PRIVATE include)
gtest_discover_tests(world_service_test)

# Auth service tests
add_executable(auth_service_test tests/auth_service_test.cpp)
target_include_directories(auth_service_test PRIVATE include)
target_link_libraries(auth_service_test PRIVATE GTest::gtest GTest::gtest_main db_utils pqxx OpenSSL::SSL OpenSSL::Crypto)
gtest_discover_tests(auth_service_test)

# Session service tests
add_executable(session_service_test tests/session_service_test.cpp)
target_link_libraries(session_service_test PRIVATE GTest::gtest GTest::gtest_main)
gtest_discover_tests(session_service_test)

# Social service tests
add_executable(social_service_test tests/social_service_test.cpp)
target_link_libraries(social_service_test PRIVATE GTest::gtest GTest::gtest_main)
gtest_discover_tests(social_service_test)

# Database tests
add_executable(database_test tests/database_test.cpp)
target_link_libraries(database_test PRIVATE db_utils GTest::gtest GTest::gtest_main)
target_include_directories(database_test PRIVATE include)
# Ensure pqxx headers don't require source_location in this target

gtest_discover_tests(database_test)

add_executable(auth_database_test tests/auth_database_test.cpp)
target_link_libraries(auth_database_test PRIVATE db_utils GTest::gtest GTest::gtest_main)
target_include_directories(auth_database_test PRIVATE include)
# Ensure pqxx headers don't require source_location in this target

gtest_discover_tests(auth_database_test)

add_executable(session_database_test tests/session_database_test.cpp)
target_link_libraries(session_database_test PRIVATE db_utils GTest::gtest GTest::gtest_main)
target_include_directories(session_database_test PRIVATE include)
# Ensure pqxx headers don't require source_location in this target

gtest_discover_tests(session_database_test)

add_executable(social_database_test tests/social_database_test.cpp)
target_link_libraries(social_database_test PRIVATE db_utils GTest::gtest GTest::gtest_main)
target_include_directories(social_database_test PRIVATE include)
# Ensure pqxx headers don't require source_location in this target

gtest_discover_tests(social_database_test)

# --- ProtoBuf support ---
find_package(Protobuf REQUIRED)
file(GLOB PROTO_FILES "${CMAKE_SOURCE_DIR}/proto/*.proto")

# C++ output - generate per-file custom commands for proper dependency tracking
set(PROTO_CPP_OUT_DIR "${CMAKE_SOURCE_DIR}/src/shared/proto")
set(PROTO_HDR_OUT_DIR "${CMAKE_SOURCE_DIR}/include/shared/proto")
set(PROTO_SRCS)
set(PROTO_HDRS)
foreach(PROTO_FILE ${PROTO_FILES})
  get_filename_component(PROTO_NAME ${PROTO_FILE} NAME_WE)
  set(PROTO_SRC "${PROTO_CPP_OUT_DIR}/${PROTO_NAME}.pb.cc")
  set(PROTO_HDR "${PROTO_HDR_OUT_DIR}/${PROTO_NAME}.pb.h")
  
  add_custom_command(
    OUTPUT ${PROTO_SRC} ${PROTO_HDR}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${PROTO_CPP_OUT_DIR}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${PROTO_HDR_OUT_DIR}
    COMMAND ${Protobuf_PROTOC_EXECUTABLE} --cpp_out=${PROTO_CPP_OUT_DIR} -I ${CMAKE_SOURCE_DIR}/proto ${PROTO_FILE}
    COMMAND ${CMAKE_COMMAND} -E copy ${PROTO_CPP_OUT_DIR}/${PROTO_NAME}.pb.h ${PROTO_HDR_OUT_DIR}/${PROTO_NAME}.pb.h
    COMMAND ${CMAKE_COMMAND} -E remove ${PROTO_CPP_OUT_DIR}/${PROTO_NAME}.pb.h
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
