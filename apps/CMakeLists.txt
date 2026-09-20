# SPDX-License-Identifier: Apache-2.0

add_executable(pocketsstvd pocketsstvd/main.cpp)
target_link_libraries(pocketsstvd PRIVATE sstv_station)

add_executable(pocketsstv pocketsstv-cli/main.cpp)
target_link_libraries(pocketsstv PRIVATE sstv_station)

foreach(target pocketsstvd pocketsstv)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra)
    endif()
endforeach()
