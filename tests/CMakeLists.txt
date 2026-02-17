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

# Add test target
add_test(NAME vis_codes COMMAND $<TARGET_FILE:test_vis_codes>)
add_test(NAME encode_smoke COMMAND $<TARGET_FILE:test_encode_smoke>)
add_test(NAME dsp_reference COMMAND $<TARGET_FILE:test_dsp_reference>)
add_test(NAME decoder_basic COMMAND $<TARGET_FILE:test_decoder_basic>)
add_test(NAME vis_decode COMMAND $<TARGET_FILE:test_vis_decode>)

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
