cmake_minimum_required(VERSION 3.10)

# VIS Code Test Suite
add_executable(test_vis_codes test_vis_codes.c)

add_executable(test_encode_smoke test_encode_smoke.c)
target_include_directories(test_encode_smoke PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(test_encode_smoke PRIVATE sstv_encoder_static)

add_executable(test_dsp_reference test_dsp_reference.cpp)
target_include_directories(test_dsp_reference PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_dsp_reference PRIVATE sstv_encoder_static)

add_executable(test_decoder_basic test_decoder_basic.c)
target_include_directories(test_decoder_basic PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(test_decoder_basic PRIVATE sstv_decoder_static m)

add_executable(test_vis_decode test_vis_decode.c)
target_include_directories(test_vis_decode PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(test_vis_decode PRIVATE sstv_decoder_static m)

add_executable(test_vis_decode_wav test_vis_decode_wav.c)
target_include_directories(test_vis_decode_wav PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(test_vis_decode_wav PRIVATE sstv_decoder_static m)

add_executable(test_hf_impairments test_hf_impairments.cpp ../src/SpectralSubtractionDNR.cpp)
target_include_directories(test_hf_impairments PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_hf_impairments PRIVATE sstv_encoder_static)

add_executable(test_decode_images test_decode_images.cpp)
target_include_directories(test_decode_images PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/external)
target_link_libraries(test_decode_images PRIVATE sstv_decoder_static m)

add_executable(test_decode_modes test_decode_modes.cpp)
target_include_directories(test_decode_modes PRIVATE
    ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(test_decode_modes PRIVATE sstv_decoder_static m)

add_executable(test_roundtrip test_roundtrip.cpp)
target_include_directories(test_roundtrip PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(test_roundtrip PRIVATE sstv_encoder_static sstv_decoder_static m)

add_executable(test_timing_correction test_timing_correction.cpp)
target_include_directories(test_timing_correction PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/external)
target_link_libraries(test_timing_correction PRIVATE sstv_decoder_static m)

# Station application tests (PocketSSTV core)
if(BUILD_STATION)
    add_executable(test_station_api test_station_api.cpp)
    target_link_libraries(test_station_api PRIVATE sstv_station)

    add_executable(fuzz_station_api fuzz_station_api.cpp)
    target_link_libraries(fuzz_station_api PRIVATE sstv_station)
endif()

# Add test target
add_test(NAME vis_codes COMMAND $<TARGET_FILE:test_vis_codes>)
add_test(NAME encode_smoke COMMAND $<TARGET_FILE:test_encode_smoke>)
add_test(NAME dsp_reference COMMAND $<TARGET_FILE:test_dsp_reference>)
add_test(NAME decoder_basic COMMAND $<TARGET_FILE:test_decoder_basic>)
add_test(NAME vis_decode COMMAND $<TARGET_FILE:test_vis_decode>)
if(BUILD_STATION)
    add_test(NAME station_api COMMAND $<TARGET_FILE:test_station_api>)
    # Short seeded run for every push; the nightly job runs it far longer
    # under sanitizers.
    add_test(NAME station_fuzz COMMAND $<TARGET_FILE:fuzz_station_api> 1 20000)
    set_tests_properties(station_fuzz PROPERTIES TIMEOUT 120)
endif()
add_test(NAME roundtrip COMMAND $<TARGET_FILE:test_roundtrip>)
set_tests_properties(roundtrip PROPERTIES TIMEOUT 300)
add_test(NAME decode_images
    COMMAND $<TARGET_FILE:test_decode_images>
            ${CMAKE_CURRENT_SOURCE_DIR}/audio
            --mae-limit 60)
set_tests_properties(decode_images PROPERTIES TIMEOUT 600)

add_test(NAME decode_modes
    COMMAND $<TARGET_FILE:test_decode_modes>
            ${CMAKE_CURRENT_SOURCE_DIR}/test_modes)
set_tests_properties(decode_modes PROPERTIES TIMEOUT 3600)

add_test(NAME timing_correction
    COMMAND $<TARGET_FILE:test_timing_correction>
            ${CMAKE_CURRENT_SOURCE_DIR}/audio)
set_tests_properties(timing_correction PROPERTIES TIMEOUT 1800)

# Optional: JSON test fixture validation
if(NOT WIN32)
  # Create custom target to validate JSON fixture syntax
  add_custom_target(validate_json_fixture
    COMMAND python3 -m json.tool vis_codes.json > /dev/null
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Validating VIS JSON fixture..."
  )
  add_dependencies(test_vis_codes validate_json_fixture)
endif()
