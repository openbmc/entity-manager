cmake_minimum_required (VERSION 3.1 FATAL_ERROR)
set (BUILD_SHARED_LIBRARIES OFF)
include (ExternalProject)
set (CMAKE_CXX_STANDARD 17)
set (CMAKE_CXX_STANDARD_REQUIRED ON)
set (CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake ${CMAKE_MODULE_PATH})
set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti -Werror -Wall")

# todo: fix these warnings
set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-strict-aliasing -Wno-catch-value")

set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fno-rtti")

option (YOCTO "Enable Building in Yocto" OFF)
option (USE_OVERLAYS "Enable Overlay Usage" ON)

if (NOT YOCTO)
    externalproject_add (
        Boost URL
        https://dl.bintray.com/boostorg/release/1.66.0/source/boost_1_66_0.tar.gz
        URL_MD5 d275cd85b00022313c171f602db59fc5 SOURCE_DIR
        "${CMAKE_BINARY_DIR}/boost-src" BINARY_DIR
        "${CMAKE_BINARY_DIR}/boost-build" CONFIGURE_COMMAND "" BUILD_COMMAND ""
        INSTALL_COMMAND mkdir -p "${CMAKE_BINARY_DIR}/prefix/include/" && cp -R
        ${CMAKE_BINARY_DIR}/boost-src/boost ${CMAKE_BINARY_DIR}/prefix/include
    )

    externalproject_add (
        nlohmann-json GIT_REPOSITORY "https://github.com/nlohmann/json.git"
        GIT_TAG d2dd27dc3b8472dbaa7d66f83619b3ebcd9185fe SOURCE_DIR
        "${CMAKE_BINARY_DIR}/nlohmann-json-src" BINARY_DIR
        "${CMAKE_BINARY_DIR}/nlohmann-json-build" CONFIGURE_COMMAND ""
        BUILD_COMMAND "" INSTALL_COMMAND mkdir -p
        "${CMAKE_BINARY_DIR}/nlohmann/include/nlohmann" && cp -r
        "${CMAKE_BINARY_DIR}/nlohmann-json-src/single_include/nlohmann"
        "${CMAKE_BINARY_DIR}/nlohmann/include"
    )

    externalproject_add (valijson GIT_REPOSITORY
                         "https://github.com/tristanpenman/valijson.git"
                         GIT_TAG c2f22fddf599d04dc33fcd7ed257c698a05345d9
                         SOURCE_DIR "${CMAKE_BINARY_DIR}/valijson-src"
                         BINARY_DIR "${CMAKE_BINARY_DIR}/valijson-build"
                         CONFIGURE_COMMAND "" BUILD_COMMAND "" INSTALL_COMMAND
                         mkdir -p
                         "${CMAKE_BINARY_DIR}/valijson/include/vaijson" && cp
                         -r "${CMAKE_BINARY_DIR}/valijson-src/include"
                         "${CMAKE_BINARY_DIR}/valijson")

    # requires apt install autoconf-archive and autoconf
    externalproject_add (sdbusplus-project PREFIX
                         ${CMAKE_BINARY_DIR}/sdbusplus-project GIT_REPOSITORY
                         https://github.com/openbmc/sdbusplus.git GIT_TAG
                         bed15f0cee4784acdf151cca14efdfb98cb9d397 SOURCE_DIR
                         ${CMAKE_BINARY_DIR}/sdbusplus-src BINARY_DIR
                         ${CMAKE_BINARY_DIR}/sdbusplus-build CONFIGURE_COMMAND
                         "" BUILD_COMMAND cd ${CMAKE_BINARY_DIR}/sdbusplus-src
                         && ./bootstrap.sh && ./configure --enable-transaction
                         && make -j libsdbusplus.la INSTALL_COMMAND ""
                         LOG_DOWNLOAD ON)

    include_directories (${CMAKE_BINARY_DIR}/sdbusplus-src)
    include_directories (${CMAKE_BINARY_DIR}/nlohmann/include)
    include_directories (${CMAKE_BINARY_DIR}/nlohmann/include/nlohmann)
    include_directories (${CMAKE_BINARY_DIR}/valijson/include)
    include_directories (${CMAKE_BINARY_DIR}/phosphor-dbus-interfaces/include)
    link_directories (${CMAKE_BINARY_DIR}/sdbusplus-src/.libs)

    include_directories (${CMAKE_BINARY_DIR}/boost-src)
    set (CMAKE_PREFIX_PATH ${CMAKE_BINARY_DIR}/boost-src ${CMAKE_PREFIX_PATH})
    option (ENABLE_TEST "Enable Google Test" OFF)
    if (ENABLE_TEST)
        hunter_add_package (GTest)
        find_package (GTest CONFIG REQUIRED)
        enable_testing ()
    endif ()
endif ()

add_definitions (-DBOOST_ERROR_CODE_HEADER_ONLY)
add_definitions (-DBOOST_SYSTEM_NO_DEPRECATED)
add_definitions (-DBOOST_ALL_NO_LIB)
add_definitions (-DBOOST_NO_RTTI)
add_definitions (-DBOOST_NO_TYPEID)

include_directories (${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories (${Boost_INCLUDE_DIRS})
include_directories (${CMAKE_CURRENT_SOURCE_DIR}/include)

add_executable (fru-device src/FruDevice.cpp src/Utils.cpp)

target_link_libraries (fru-device pthread)
target_link_libraries (fru-device stdc++fs)
target_link_libraries (fru-device i2c)
target_link_libraries (fru-device ${Boost_LIBRARIES})
target_link_libraries (fru-device -lsystemd)
target_link_libraries (fru-device sdbusplus)

add_executable (entity-manager src/EntityManager.cpp src/Overlay.cpp
                src/Utils.cpp)

target_link_libraries (entity-manager -lsystemd)
target_link_libraries (entity-manager stdc++fs)
target_link_libraries (entity-manager ${Boost_LIBRARIES})
target_link_libraries (entity-manager sdbusplus)
if (USE_OVERLAYS) # overlays can be disabled because they require a kernel patch
                  # as of today
    target_compile_definitions (entity-manager PRIVATE OVERLAYS=1)
endif ()

if (NOT YOCTO)
    add_dependencies (entity-manager nlohmann-json)
    add_dependencies (entity-manager sdbusplus-project)
    add_dependencies (entity-manager valijson)
    add_dependencies (fru-device valijson)
    add_dependencies (fru-device sdbusplus-project)
    add_dependencies (valijson nlohmann-json)
endif ()

set (
    SERVICE_FILES
    ${PROJECT_SOURCE_DIR}/service_files/xyz.openbmc_project.EntityManager.service
    ${PROJECT_SOURCE_DIR}/service_files/xyz.openbmc_project.FruDevice.service
)

set (PACKAGE_DIR /usr/share/entity-manager/)
target_compile_definitions (entity-manager PRIVATE PACKAGE_DIR="${PACKAGE_DIR}"
                            -DBOOST_ASIO_DISABLE_THREADS)
install (TARGETS fru-device entity-manager DESTINATION bin)
install (DIRECTORY configurations DESTINATION ${PACKAGE_DIR})
install (DIRECTORY overlay_templates DESTINATION ${PACKAGE_DIR})
install (DIRECTORY schemas DESTINATION ${PACKAGE_DIR}/configurations)
install (FILES ${SERVICE_FILES} DESTINATION /lib/systemd/system/)
