cmake_minimum_required(VERSION 3.20) # FetchContent is available in 3.11+
project(TankBusters CXX)

set(CMAKE_CXX_STANDARD 17)

# Generate compile_commands.json
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)


if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Debug")
endif()

find_package(Threads)
link_libraries(Threads::Threads)

# Dependencies
set(RAYLIB_VERSION 5.5)
find_package(raylib ${RAYLIB_VERSION} QUIET) # QUIET or REQUIRED
if (NOT raylib_FOUND) # If there's none, fetch and build raylib
  include(FetchContent)
  FetchContent_Declare(
    raylib
    DOWNLOAD_EXTRACT_TIMESTAMP OFF
    URL https://github.com/raysan5/raylib/archive/refs/tags/${RAYLIB_VERSION}.tar.gz
  )
  FetchContent_GetProperties(raylib)
  if (NOT raylib_POPULATED) # Have we downloaded raylib yet?
    set(FETCHCONTENT_QUIET NO)
    FetchContent_MakeAvailable(raylib)
    set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE) # don't build the supplied examples
  endif()
endif()

set(JSON_VERSION 3.11.3)
find_package(nlohmann_json ${JSON_VERSION} QUIET)
if (NOT nlohmann_json_FOUND) # If there's none, fetch and build nlohmann_json
  include(FetchContent)
  FetchContent_Declare(
    json
    DOWNLOAD_EXTRACT_TIMESTAMP OFF
    URL https://github.com/nlohmann/json/releases/download/v${JSON_VERSION}/json.tar.xz
  )
  FetchContent_GetProperties(json)
  if (NOT nlohmann_json_POPULATED) # Have we downloaded nlohmann_json yet?
    set(FETCHCONTENT_QUIET NO)
    FetchContent_MakeAvailable(json)
    set(JSON_BuildTests OFF CACHE INTERNAL "")
  endif()
endif()

# Project definition

include ( CheckCXXCompilerFlag )
foreach ( FLAG -Wall -Wextra -pedantic -Wno-missing-field-initializers -Wno-vla )
  check_cxx_compiler_flag ( "${FLAG}" COMPILER_SUPPORTS_${FLAG} )
  if ( COMPILER_SUPPORTS_${FLAG} )
    add_compile_options ( ${FLAG} )
  endif ( )
endforeach ( )

# Define PROJECT_SOURCES as a list of all source files
file(GLOB_RECURSE PROJECT_COMMON_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_LIST_DIR}/src/common/*.cpp")
file(GLOB_RECURSE PROJECT_CLIENT_ONLY_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_LIST_DIR}/src/client/*.cpp")
file(GLOB_RECURSE PROJECT_SERVER_ONLY_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_LIST_DIR}/src/server/*.cpp")

# Define PROJECT_INCLUDE to be the path to the include directory of the project
set(PROJECT_COMMON_INCLUDE "${CMAKE_CURRENT_LIST_DIR}/src/common/")
set(PROJECT_CLIENT_ONLY_INCLUDE "${CMAKE_CURRENT_LIST_DIR}/src/client/")
set(PROJECT_SERVER_ONLY_INCLUDE "${CMAKE_CURRENT_LIST_DIR}/src/server/")

# Define Client and Server executables
set(CLIENT "${PROJECT_NAME}Client")
set(SERVER "${PROJECT_NAME}Server")

# Client config
add_executable(${CLIENT})
target_sources(${CLIENT} PRIVATE ${PROJECT_COMMON_SOURCES})
target_sources(${CLIENT} PRIVATE ${PROJECT_CLIENT_ONLY_SOURCES})
target_include_directories(${CLIENT} PRIVATE ${PROJECT_COMMON_INCLUDE})
target_include_directories(${CLIENT} PRIVATE ${PROJECT_CLIENT_ONLY_INCLUDE})
target_link_libraries(${CLIENT} raylib)
target_link_libraries(${CLIENT} Threads::Threads)
target_link_libraries(${CLIENT} nlohmann_json::nlohmann_json)

# Server config
add_executable(${SERVER})
target_sources(${SERVER} PRIVATE ${PROJECT_COMMON_SOURCES})
target_sources(${SERVER} PRIVATE ${PROJECT_SERVER_ONLY_SOURCES})
target_include_directories(${SERVER} PRIVATE ${PROJECT_COMMON_INCLUDE})
target_include_directories(${SERVER} PRIVATE ${PROJECT_SERVER_ONLY_INCLUDE})
target_link_libraries(${SERVER} raylib)
target_link_libraries(${SERVER} Threads::Threads)
target_link_libraries(${SERVER} nlohmann_json::nlohmann_json)

