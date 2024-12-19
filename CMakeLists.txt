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

# Server config
add_executable(${SERVER})
target_sources(${SERVER} PRIVATE ${PROJECT_COMMON_SOURCES})
target_sources(${SERVER} PRIVATE ${PROJECT_SERVER_ONLY_SOURCES})
target_include_directories(${SERVER} PRIVATE ${PROJECT_COMMON_INCLUDE})
target_include_directories(${SERVER} PRIVATE ${PROJECT_SERVER_ONLY_INCLUDE})
target_link_libraries(${SERVER} raylib)
target_link_libraries(${SERVER} Threads::Threads)


# Web Configurations
if (${PLATFORM} STREQUAL "Web")
  set_target_properties(${CLIENT} PROPERTIES SUFFIX ".html") # Tell Emscripten to build an example.html file.
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -s USE_GLFW=3 -s ASSERTIONS=1 -s WASM=1 -s ASYNCIFY -s GL_ENABLE_GET_PROC_ADDRESS=1")
endif()

# Checks if OSX and links appropriate frameworks (Only required on MacOS)
if (APPLE)
  target_link_libraries(${CLIENT} "-framework IOKit")
  target_link_libraries(${CLIENT} "-framework Cocoa")
  target_link_libraries(${CLIENT} "-framework OpenGL")
endif()
