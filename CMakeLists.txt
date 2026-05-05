cmake_minimum_required(VERSION 3.16)

# Sets project name
project(textoon LANGUAGES CXX C)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# Sets C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Specifies required Qt components
find_package(Qt6 REQUIRED COMPONENTS Core)
find_package(Qt6 REQUIRED COMPONENTS Concurrent)
find_package(Qt6 REQUIRED COMPONENTS Widgets)
find_package(Qt6 REQUIRED COMPONENTS Gui)

# Specifies .cpp and .h files to be passed to the compiler

file(GLOB LZSOURCES "lazybrush/lazybrush_components/*.cpp"
                    "lazybrush/lazybrush_include/grid_of_quadtrees_colorizer/*.cpp")

file(GLOB LZHEADERS "lazybrush/lazybrush_include/*.h"
                    "lazybrush/lazybrush_include/*.hpp"
                    "lazybrush/lazybrush_include/grid_of_quadtrees_colorizer/*.h"
                    "lazybrush/lazybrush_include/grid_of_quadtrees_colorizer/*.hpp")

file(GLOB PREPROCESS "lazybrush/lazybrush_include/preprocessing/*.h"
                     "lazybrush/lazybrush_include/preprocessing/*.hpp"
                     "lazybrush/lazybrush_include/preprocessing/*.cpp")

file(GLOB THIRD_PARTY "lazybrush/lazybrush_include/third_party/maxflow/*.h"
                      "lazybrush/lazybrush_include/third_party/maxflow/*.hpp"
                      "lazybrush/lazybrush_include/third_party/maxflow/*.cpp")

add_executable(${PROJECT_NAME}
    src/main.cpp
    src/mainwindow.cpp
    src/textoon.cpp

    src/scribblecontext.h
    src/scribblecontext.cpp
    src/mainwindow.h
    src/textoon.h

    src/depthmapgenerator.h
    src/depthmapgenerator.cpp

    src/postprocessing.h
    src/postprocessing.cpp

    util/obj_loader.h
    util/vector_utils.h

    ${LZHEADERS}
    ${LZSOURCES}
    ${PREPROCESS}
    ${THIRD_PARTY}
)

# Allows you to include files from within those directories, without prefixing their filepaths
target_include_directories(${PROJECT_NAME} PRIVATE src libs)

# Specifies libraries to be linked (Qt components, glew, etc)
target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt::Concurrent
    Qt::Core
    Qt::Gui
    Qt::Widgets
)

# This allows you to `#include "Eigen/..."`
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/Eigen
)

# Specifies other files
qt6_add_resources(${PROJECT_NAME} "Resources"
    PREFIX
        "/"
    FILES

)

# GLEW: this provides support for Windows (including 64-bit)
if (WIN32)
  add_compile_definitions(GLEW_STATIC)
  target_link_libraries(${PROJECT_NAME} PRIVATE
    opengl32
    glu32
  )
endif()

# Set this flag to silence warnings on Windows
if (MSVC OR MSYS OR MINGW)
  set(CMAKE_CXX_FLAGS "-Wno-volatile")
endif()

# Set this flag to silence warnings on MacOS
if (APPLE)
  set(CMAKE_CXX_FLAGS "-Wno-deprecated-volatile")
endif()
