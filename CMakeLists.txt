cmake_minimum_required(VERSION 3.1 FATAL_ERROR)
SET(BUILD_SHARED_LIBRARIES OFF)
include(ExternalProject)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -lstdc++fs")
set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake ${CMAKE_MODULE_PATH})

option(HUNTER_ENABLED "Enable hunter package pulling" OFF)
include("cmake/HunterGate.cmake")

HunterGate(
    URL "https://github.com/ruslo/hunter/archive/v0.18.64.tar.gz"
    SHA1 "baf9c8cc4f65306f0e442b5419967b4c4c04589a"
)

project(enitity-manager)

hunter_add_package(Boost)
find_package(Boost REQUIRED)
include_directories(${Boost_INCLUDE_DIRS})

add_definitions(-DBOOST_ERROR_CODE_HEADER_ONLY)
add_definitions(-DBOOST_SYSTEM_NO_DEPRECATED)
add_definitions(-DBOOST_ALL_NO_LIB)
add_definitions(-DBOOST_NO_RTTI)
add_definitions(-DBOOST_NO_TYPEID)

hunter_add_package(dbus)
find_package(dbus REQUIRED) # Include functions provided by PkgConfig module.
include_directories(${DBUS_INCLUDE_DIRS})

find_package(Threads REQUIRED)

option(YOCTO "Enable Building in Yocto" OFF)

if(NOT YOCTO)
    ExternalProject_Add(boost-dbus
        PREFIX ${CMAKE_CURRENT_BINARY_DIR}/boost-dbus
        GIT_REPOSITORY ssh://git-amr-2.devtools.intel.com:29418/openbmc-boost-dbus
        GIT_TAG 216455021500e013a8d95a2d412ade31177353b7
        CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${EXTERNAL_INSTALL_LOCATION} -DBOOST_ROOT=${BOOST_ROOT}
        CONFIGURE_COMMAND ""
        BUILD_COMMAND ""
        INSTALL_COMMAND ""
        LOG_DOWNLOAD ON
    )
    ExternalProject_Get_Property(boost-dbus install_dir)
    include_directories(${install_dir}/src/boost-dbus/include)

    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include/non-yocto)

    ExternalProject_Add(nlohmann-json
        PREFIX ${CMAKE_CURRENT_BINARY_DIR}/nlohmann-json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG afebb6a3bbc8751e834a5472e5c53cb2b5bbd750
        CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${EXTERNAL_INSTALL_LOCATION}
        CONFIGURE_COMMAND ""
        BUILD_COMMAND mkdir -p
            ${CMAKE_CURRENT_BINARY_DIR}/nlohmann-json/src/nlohmann-json/src/nlohmann &&
                cp ${CMAKE_CURRENT_BINARY_DIR}/nlohmann-json/src/nlohmann-json/src/json.hpp
                ${CMAKE_CURRENT_BINARY_DIR}/nlohmann-json/src/nlohmann-json/src/nlohmann/json.hpp
        INSTALL_COMMAND ""
        LOG_DOWNLOAD ON
    )

    ExternalProject_Get_Property(nlohmann-json install_dir)
    include_directories(${install_dir}/src/nlohmann-json/src)

    ExternalProject_Add(sdbusplus
        PREFIX ${CMAKE_CURRENT_BINARY_DIR}/sdbusplus
        GIT_REPOSITORY ssh://git-amr-2.devtools.intel.com:29418/sdbusplus
        CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${EXTERNAL_INSTALL_LOCATION} -DBOOST_ROOT=${BOOST_ROOT}
        CONFIGURE_COMMAND ""
        BUILD_COMMAND ""
        INSTALL_COMMAND ""
        LOG_DOWNLOAD ON
    )
    set(WANT_TRANSACTION 0)
    configure_file(${CMAKE_BINARY_DIR}/sdbusplus-src/sdbusplus/server.hpp.in
                  ${CMAKE_BINARY_DIR}/prefix/include/sdbusplus/server.hpp @ONLY)
    configure_file(${CMAKE_BINARY_DIR}/sdbusplus-src/sdbusplus/bus.hpp.in
                  ${CMAKE_BINARY_DIR}/prefix/include/sdbusplus/bus.hpp @ONLY)

    ExternalProject_Get_Property(sdbusplus install_dir)
    include_directories(${install_dir}/src/sdbusplus/include)

    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include/non-yocto)

    option(ENABLE_TEST "Enable Google Test" OFF)
    if(ENABLE_TEST)
        hunter_add_package(GTest)
        find_package(GTest CONFIG REQUIRED)
        enable_testing()
    endif()
endif()
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

link_directories(${EXTERNAL_INSTALL_LOCATION}/lib)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

add_executable(fru-device src/FruDevice.cpp src/Utils.cpp)

target_link_libraries(fru-device pthread)
target_link_libraries(fru-device stdc++fs)
target_link_libraries(fru-device ${Boost_LIBRARIES})
target_link_libraries(fru-device -lsystemd)
target_link_libraries(fru-device phosphor_dbus)

add_executable(entity-manager src/EntityManager.cpp
                              src/Overlay.cpp
                              src/Utils.cpp)

target_link_libraries(entity-manager  ${DBUS_LIBRARIES})
target_link_libraries(entity-manager pthread)
target_link_libraries(entity-manager stdc++fs)
target_link_libraries(entity-manager ${Boost_LIBRARIES})

if(NOT YOCTO)
    add_dependencies(entity-manager nlohmann-json)
    add_dependencies(entity-manager boost-dbus)
    add_dependencies(fru-device sdbusplus)
endif()

install (TARGETS fru-device entity-manager DESTINATION bin)
