cmake_minimum_required(VERSION 3.12)
project(ctrack VERSION 1.0.2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Option to disable parallel processing
option(DISABLE_PAR "Disable parallel processing" OFF)

# Option to disable building examples
option(DISABLE_EXAMPLES "Disable building examples" OFF)

# Option to build benchmark (default OFF)
option(BUILD_BENCHMARK "Build benchmark" OFF)

option(ENABLE_WARNINGS "Enable warnings" OFF)

# Check for TBB
if(NOT MSVC AND NOT DISABLE_PAR)
    find_package(TBB QUIET)
    if(TBB_FOUND)
        message(STATUS "TBB found. Enabling parallel execution.")
    else()
        message(STATUS "TBB not found. Disabling parallel execution.")
        set(DISABLE_PAR ON)
    endif()
elseif(DISABLE_PAR)
    message(STATUS "DISABLE_PAR set. Disabling parallel execution.")
endif()

# Create the ctrack library
add_library(ctrack INTERFACE)
target_include_directories(ctrack INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Configure ctrack based on TBB availability
if(DISABLE_PAR)
    target_compile_definitions(ctrack INTERFACE CTRACK_DISABLE_EXECUTION_POLICY)
elseif(NOT MSVC AND TBB_FOUND)
    target_link_libraries(ctrack INTERFACE TBB::tbb)
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(ENABLE_WARNINGS)
    if (NOT MSVC)
        include(cmake/add_warning.cmake)
        include(cmake/warnings.cmake)
    endif()
endif()

# Add the examples subdirectory if not disabled
if(NOT DISABLE_EXAMPLES)
    add_subdirectory(examples)
else()
    message(STATUS "Building examples disabled.")
endif()

# Add the benchmark subdirectory if enabled
if(BUILD_BENCHMARK)
    add_subdirectory(benchmark)
    message(STATUS "Building benchmark enabled.")
else()
    message(STATUS "Building benchmark disabled.")
endif()

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