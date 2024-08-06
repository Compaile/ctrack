cmake_minimum_required(VERSION 3.12)
project(ctrack VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Create the ctrack library
add_library(ctrack INTERFACE)
target_include_directories(ctrack INTERFACE 
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Add the examples subdirectory
add_subdirectory(examples)

# Installation
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

install(TARGETS ctrack
    EXPORT ctrackTargets
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

install(EXPORT ctrackTargets
    FILE ctrackTargets.cmake
    NAMESPACE ctrack::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ctrack
)

configure_package_config_file(${CMAKE_CURRENT_SOURCE_DIR}/Config.cmake.in
    "${CMAKE_CURRENT_BINARY_DIR}/ctrackConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ctrack
)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/ctrackConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/ctrackConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/ctrackConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ctrack
)