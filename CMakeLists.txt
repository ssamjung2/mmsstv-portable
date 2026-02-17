cmake_minimum_required(VERSION 3.10)
project(sstv_encoder VERSION 1.0.0 LANGUAGES C CXX)

# Place executables in a bin directory separate from the build tree
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bin)
file(MAKE_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})

# Build options
option(BUILD_EXAMPLES "Build example programs" ON)
option(BUILD_SHARED "Build shared library" ON)
option(BUILD_STATIC "Build static library" ON)
option(BUILD_TESTS "Build tests" OFF)
option(BUILD_RX "Build RX decoder library" ON)

# C++11 for internal implementation
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# C99 for public API
set(CMAKE_C_STANDARD 99)

# Shared/common sources
set(COMMON_SOURCES
    src/modes.cpp
    src/dsp_filters.cpp
)

add_library(sstv_common_obj OBJECT ${COMMON_SOURCES})
target_include_directories(sstv_common_obj PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_compile_options(sstv_common_obj PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra>
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
)

# Encoder library sources
set(ENCODER_SOURCES
    src/encoder.cpp
    src/vco.cpp
    src/vis.cpp
    $<TARGET_OBJECTS:sstv_common_obj>
)

# Build shared library
if(BUILD_SHARED)
    add_library(sstv_encoder SHARED ${ENCODER_SOURCES})
    target_include_directories(sstv_encoder PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    )
    set_target_properties(sstv_encoder PROPERTIES
        VERSION ${PROJECT_VERSION}
        SOVERSION 1
        PUBLIC_HEADER include/sstv_encoder.h
    )
    target_compile_options(sstv_encoder PRIVATE
        $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra>
        $<$<CXX_COMPILER_ID:MSVC>:/W4>
    )
endif()

# Build static library
if(BUILD_STATIC)
    add_library(sstv_encoder_static STATIC ${ENCODER_SOURCES})
    target_include_directories(sstv_encoder_static PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    )
    set_target_properties(sstv_encoder_static PROPERTIES
        OUTPUT_NAME sstv_encoder
        PUBLIC_HEADER include/sstv_encoder.h
    )
    target_compile_options(sstv_encoder_static PRIVATE
        $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra>
        $<$<CXX_COMPILER_ID:MSVC>:/W4>
    )
endif()

# RX decoder library
if(BUILD_RX)
    set(DECODER_SOURCES
        src/decoder.cpp
        $<TARGET_OBJECTS:sstv_common_obj>
    )

    if(BUILD_SHARED)
        add_library(sstv_decoder SHARED ${DECODER_SOURCES})
        target_include_directories(sstv_decoder PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>
        )
        set_target_properties(sstv_decoder PROPERTIES
            VERSION ${PROJECT_VERSION}
            SOVERSION 1
            PUBLIC_HEADER include/sstv_decoder.h
        )
        target_compile_options(sstv_decoder PRIVATE
            $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra>
            $<$<CXX_COMPILER_ID:MSVC>:/W4>
        )
    endif()

    if(BUILD_STATIC)
        add_library(sstv_decoder_static STATIC ${DECODER_SOURCES})
        target_include_directories(sstv_decoder_static PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>
        )
        set_target_properties(sstv_decoder_static PROPERTIES
            OUTPUT_NAME sstv_decoder
            PUBLIC_HEADER include/sstv_decoder.h
        )
        target_compile_options(sstv_decoder_static PRIVATE
            $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra>
            $<$<CXX_COMPILER_ID:MSVC>:/W4>
        )
    endif()
endif()

# Math library (needed on some platforms)
find_library(MATH_LIBRARY m)
if(MATH_LIBRARY)
    if(BUILD_SHARED)
        target_link_libraries(sstv_encoder PRIVATE ${MATH_LIBRARY})
    endif()
    if(BUILD_STATIC)
        target_link_libraries(sstv_encoder_static PRIVATE ${MATH_LIBRARY})
    endif()
    if(BUILD_RX AND BUILD_SHARED)
        target_link_libraries(sstv_decoder PRIVATE ${MATH_LIBRARY})
    endif()
    if(BUILD_RX AND BUILD_STATIC)
        target_link_libraries(sstv_decoder_static PRIVATE ${MATH_LIBRARY})
    endif()
endif()

# Examples
if(BUILD_EXAMPLES)
    add_executable(list_modes examples/list_modes.c)
    if(BUILD_SHARED)
        target_link_libraries(list_modes sstv_encoder)
    else()
        target_link_libraries(list_modes sstv_encoder_static)
    endif()

    add_executable(encode_wav examples/encode_wav.c)
    if(BUILD_SHARED)
        target_link_libraries(encode_wav sstv_encoder)
    else()
        target_link_libraries(encode_wav sstv_encoder_static)
    endif()

    add_executable(generate_all_modes examples/generate_all_modes.c)
    if(BUILD_SHARED)
        target_link_libraries(generate_all_modes sstv_encoder)
    else()
        target_link_libraries(generate_all_modes sstv_encoder_static)
    endif()
    
    # Real image test driver (requires stb_image)
    add_executable(test_real_images examples/test_real_images.c)
    target_include_directories(test_real_images PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    if(BUILD_SHARED)
        target_link_libraries(test_real_images sstv_encoder m)
    else()
        target_link_libraries(test_real_images sstv_encoder_static m)
    endif()

    if(BUILD_RX)
        add_executable(decode_wav examples/decode_wav.c)
        if(BUILD_SHARED)
            target_link_libraries(decode_wav sstv_decoder)
        else()
            target_link_libraries(decode_wav sstv_decoder_static)
        endif()
    endif()
endif()

# Tests
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Installation
include(GNUInstallDirs)

if(BUILD_SHARED)
    install(TARGETS sstv_encoder
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )
endif()

if(BUILD_STATIC)
    install(TARGETS sstv_encoder_static
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )
endif()

if(BUILD_RX AND BUILD_SHARED)
    install(TARGETS sstv_decoder
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )
endif()

if(BUILD_RX AND BUILD_STATIC)
    install(TARGETS sstv_decoder_static
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )
endif()

# pkg-config file
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/sstv_encoder.pc.in
    ${CMAKE_CURRENT_BINARY_DIR}/sstv_encoder.pc
    @ONLY
)
install(FILES ${CMAKE_BINARY_DIR}/sstv_encoder.pc
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig
)
