cmake_minimum_required(VERSION 3.10)
project(arc_unpacker)

# 使用 C++14
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 设置 CMake 模块路径
set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/src/cmake")

# ------- Version -------
execute_process(COMMAND git describe --tags OUTPUT_VARIABLE VERSION_SHORT OUTPUT_STRIP_TRAILING_WHITESPACE)
execute_process(COMMAND git describe --always --dirty --long --tags OUTPUT_VARIABLE VERSION_LONG OUTPUT_STRIP_TRAILING_WHITESPACE)
if("${VERSION_SHORT}" STREQUAL "")
    set(VERSION_SHORT "0.0")
    set(VERSION_LONG "?")
endif()
message("Version: ${VERSION_SHORT}")
message("Version long: ${VERSION_LONG}")

# ------------
# Dependencies
# ------------
cmake_policy(SET CMP0167 NEW)
find_package(Boost REQUIRED COMPONENTS filesystem system)
find_package(ZLIB REQUIRED)
find_package(BZip2 REQUIRED)
find_package(PNG REQUIRED)
find_package(JPEG REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(WebP REQUIRED)

# 设置 include 目录
include_directories(
    ${Boost_INCLUDE_DIRS}
    ${ZLIB_INCLUDE_DIRS}
    ${BZIP2_INCLUDE_DIRS}
    ${PNG_INCLUDE_DIRS}
    ${JPEG_INCLUDE_DIRS}
    ${OPENSSL_INCLUDE_DIRS}
    ${WEBP_INCLUDE_DIRS}
)

# 设置库目录
link_directories(
    ${Boost_LIBRARY_DIRS}
    ${ZLIB_LIBRARY_DIRS}
    ${BZIP2_LIBRARY_DIRS}
    ${PNG_LIBRARY_DIRS}
    ${JPEG_LIBRARY_DIRS}
    ${OPENSSL_LIBRARY_DIRS}
    ${WEBP_LIBRARY_DIRS}
)

# ------------
# Global build options
# ------------
if(CMAKE_COMPILER_IS_GNUCXX)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pedantic -Wwrite-strings -Wno-unused-variable -Wno-unused-parameter -Wold-style-cast -pthread")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -s")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -ggdb")
endif()

# 64-bit off_t
add_definitions(-D_FILE_OFFSET_BITS=64)

# ------------
# Source files
# ------------
configure_file("${CMAKE_SOURCE_DIR}/src/version.h.in" "${CMAKE_BINARY_DIR}/generated/version.h" @ONLY)
include_directories(${CMAKE_BINARY_DIR}/generated)

file(GLOB_RECURSE au_sources "${CMAKE_SOURCE_DIR}/src/*.cc")
file(GLOB_RECURSE au_headers "${CMAKE_SOURCE_DIR}/src/*.h")
list(REMOVE_ITEM au_sources "${CMAKE_SOURCE_DIR}/src/main.cc")

option(micro "Micro" OFF)
function(filter_sources sources)
    foreach(item ${${sources}})
        if(${item} MATCHES "dec/.*/")
            if(NOT ${item} MATCHES "french_bread|/(jpeg|png|bmp|wav)_(audio|image)")
                list(REMOVE_ITEM ${sources} ${item})
            endif()
        endif()
    endforeach()
    set(${sources} ${${sources}} PARENT_SCOPE)
endfunction()

if(micro)
    message("Enabling micro build")
    filter_sources(au_sources)
    filter_sources(au_headers)
endif()

# Platform-specific source exclusions
if(WIN32)
    list(REMOVE_ITEM au_sources "${CMAKE_SOURCE_DIR}/src/logger_ansi.cc")
    list(REMOVE_ITEM au_sources "${CMAKE_SOURCE_DIR}/src/logger_dummy.cc")
elseif(UNIX)
    list(REMOVE_ITEM au_sources "${CMAKE_SOURCE_DIR}/src/logger_win.cc")
    list(REMOVE_ITEM au_sources "${CMAKE_SOURCE_DIR}/src/logger_dummy.cc")
else()
    list(REMOVE_ITEM au_sources "${CMAKE_SOURCE_DIR}/src/logger_win.cc")
    list(REMOVE_ITEM au_sources "${CMAKE_SOURCE_DIR}/src/logger_ansi.cc")
endif()

macro(group_source_files PARENT_DIR FILES)
    foreach(FILE ${FILES})
        file(RELATIVE_PATH GROUP "${PARENT_DIR}" "${FILE}")
        get_filename_component(GROUP "${GROUP}" PATH)
        string(REPLACE "/" "\\" GROUP "${GROUP}")

        if("${FILE}" MATCHES ".*\\.cc")
            set(GROUP "Source Files\\${GROUP}")
        elseif("${FILE}" MATCHES ".*\\.h")
            set(GROUP "Header Files\\${GROUP}")
        endif()

        source_group("${GROUP}" FILES "${FILE}")
    endforeach()
endmacro()

group_source_files("${CMAKE_SOURCE_DIR}/src" "${au_sources};${au_headers}")

# -------------------
# 3rd party libraries
# -------------------

# Catch2 for testing
set(CATCH_PATH "${CMAKE_SOURCE_DIR}/tests/test_support/catch.h")
if(NOT EXISTS "${CATCH_PATH}")
    message("Downloading Catch2...")
    file(DOWNLOAD "https://raw.githubusercontent.com/catchorg/Catch2/master/single_include/catch2/catch.hpp" "${CATCH_PATH}")
endif()

# -------------------
# Linking definitions
# -------------------
if(CMAKE_COMPILER_IS_GNUCXX)
    set(iconv "")
    set(unicode "")
elseif(CMAKE_SYSTEM_NAME MATCHES "Darwin")
    set(iconv iconv)
else()
    set(iconv ${ICONV_LIBRARY})
    set(unicode "")
    include_directories(${ICONV_INCLUDE_DIR})
endif()

add_library(libau OBJECT ${au_sources} ${au_headers})

add_executable(arc_unpacker "${CMAKE_SOURCE_DIR}/src/main.cc" $<TARGET_OBJECTS:libau>)
target_link_libraries(arc_unpacker ${unicode} ${iconv} ${Boost_LIBRARIES} ${ZLIB_LIBRARIES} ${BZIP2_LIBRARIES} ${PNG_LIBRARIES} ${JPEG_LIBRARIES} ${OPENSSL_LIBRARIES})
if(WEBP_FOUND)
    target_link_libraries(arc_unpacker ${WEBP_LIBRARIES})
endif()

target_include_directories(libau BEFORE PUBLIC "${CMAKE_SOURCE_DIR}/src")
target_include_directories(libau BEFORE PUBLIC "${CMAKE_BINARY_DIR}/generated")
target_include_directories(arc_unpacker BEFORE PUBLIC "${CMAKE_SOURCE_DIR}/src")
target_include_directories(arc_unpacker BEFORE PUBLIC "${CMAKE_BINARY_DIR}/generated")
