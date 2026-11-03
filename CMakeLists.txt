cmake_minimum_required(VERSION 3.24)
project(mooncake CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(USERVER_FEATURE_CHAOTIC_OPENAPI OFF)
set(USERVER_FEATURE_JEMALLOC OFF)
set(USERVER_FEATURE_TESTSUITE OFF)
set(USERVER_CHECK_PACKAGE_VERSIONS OFF)

include(FetchContent)
FetchContent_Declare(userver
  GIT_REPOSITORY https://github.com/userver-framework/userver.git
  GIT_TAG v3.2
  GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(userver)
userver_setup_environment()

add_library(mooncake_objs OBJECT
  src/block_store.cpp
  src/block_handler.cpp)
target_include_directories(mooncake_objs PUBLIC src)
target_link_libraries(mooncake_objs PUBLIC userver::core)

add_executable(cache_node src/main.cpp)
target_link_libraries(cache_node PRIVATE mooncake_objs)

add_executable(block_store_test tests/block_store_test.cpp)
target_link_libraries(block_store_test PRIVATE mooncake_objs userver::utest)
add_google_tests(block_store_test)
