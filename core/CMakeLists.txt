# SPDX-License-Identifier: Apache-2.0
#
# libsstv_station - everything the station does that is not pixels on a screen
# (ADR-0001). Day one holds the API dispatch and its JSON; session, audio,
# rig, pictures and library modules arrive in later slices.

add_library(sstv_station STATIC
    src/json.cpp
    src/api.cpp
    src/audio.cpp
    src/session.cpp
)

target_include_directories(sstv_station PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include          # sstv_encoder.h, for version reporting
)

# The core is C++17 (ADR/engineering practices); the signal libraries stay at
# C++11 and are not disturbed by this.
target_compile_features(sstv_station PUBLIC cxx_std_17)

# The session drives the decoder; the encoder header supplies mode names.
target_link_libraries(sstv_station PUBLIC sstv_decoder_static)

target_compile_definitions(sstv_station PUBLIC
    POCKETSSTV_VERSION="${POCKETSSTV_VERSION}"
)

if(MSVC)
    target_compile_options(sstv_station PRIVATE /W4)
else()
    target_compile_options(sstv_station PRIVATE -Wall -Wextra)
endif()
