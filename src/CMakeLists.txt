cmake_minimum_required(VERSION 3.20)

project(XPandProjects LANGUAGES C CXX)

set(XPAND_APPLICATION_ROOT_PATH "/srv/public/bbrence/AmiraAvizo3D-20222" 
    CACHE PATH "Path to the root directory containing Avizo/Amira/PerGeos software."
)
set(XPAND_CMAKE_ROOT_PATH "${XPAND_APPLICATION_ROOT_PATH}/cmake" 
    CACHE INTERNAL "Path to the directory containing Avizo/Amira/PerGeos CMake modules and configuration files."
)

# Set path to modules inside Avizo/Amira/PerGeos software install directory.
set(CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH};${XPAND_APPLICATION_ROOT_PATH}/cmake/modules")

# Set the C++ version used throughout the project
set(CMAKE_CXX_STANDARD 11)
# Set the C++ standard as required for the compiler
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Disable C++ compiler extensions to ensure portable code
set(CMAKE_CXX_EXTENSIONS OFF)
# Set Release as the default build type for single-config generators (such as Make)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Specifies the build type on single-configuration generators." FORCE)
endif()

# Setup everything for XPand
include(XPandUtils)
xpand_setup(${XPAND_APPLICATION_ROOT_PATH})

# Find third party libraries
find_package(AvizoAppsBoost REQUIRED)
find_package(AvizoAppsGTest REQUIRED)
find_package(AvizoAppsQt5 REQUIRED)
find_package(AvizoAppsOIV REQUIRED)
find_package(AvizoAppsTbb REQUIRED)
find_package(AvizoAppsTcl REQUIRED)
find_package(AvizoAppsULM REQUIRED)
find_package(AvizoAppsUnifiedUI REQUIRED)
find_package(OpenMP REQUIRED)
find_package(OpenGL REQUIRED)

# Find application libraries
find_package(AmiraAvizo3D REQUIRED)

# Generate documentation
xpand_doc(${XPAND_APPLICATION_ROOT_PATH} AmiraSkin)

add_subdirectory(hxfilo)
