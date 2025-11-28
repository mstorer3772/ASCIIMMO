cmake_minimum_required(VERSION 3.10)
project(asciimmo-server VERSION 0.1 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(asciimmo-server
  src/main.cpp
  src/worldgen.cpp
)

target_include_directories(asciimmo-server PRIVATE include)

# Install rule (optional)
install(TARGETS asciimmo-server RUNTIME DESTINATION bin)
