cmake_minimum_required(VERSION 3.22)
project(G3XQ10 VERSION 0.1.0 LANGUAGES C CXX)

option(G3X_BUILD_TESTS "Build DSP tests" ON)
option(G3X_BUILD_PLUGIN "Build JUCE plugin targets" ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(g3x_q10_dsp STATIC
  src/dsp/BiquadFilter.cpp
  src/dsp/Q10Processor.cpp)
target_include_directories(g3x_q10_dsp PUBLIC src)
if(MSVC)
  target_compile_options(g3x_q10_dsp PRIVATE /W4 /permissive-)
else()
  target_compile_options(g3x_q10_dsp PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(G3X_BUILD_PLUGIN)
  include(FetchContent)
  FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 9.0.1 GIT_SHALLOW TRUE)
  FetchContent_MakeAvailable(JUCE)
  juce_add_plugin(G3XQ10 COMPANY_NAME G3X BUNDLE_ID com.g3x.q10
    PLUGIN_MANUFACTURER_CODE G3xx PLUGIN_CODE Gq10 FORMATS VST3 Standalone
    PRODUCT_NAME "G3X Q10" IS_SYNTH FALSE NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE IS_MIDI_EFFECT FALSE COPY_PLUGIN_AFTER_BUILD FALSE)
  target_sources(G3XQ10 PRIVATE src/plugin/PluginProcessor.cpp src/plugin/PluginProcessor.hpp)
  target_include_directories(G3XQ10 PRIVATE src)
  target_compile_definitions(G3XQ10 PUBLIC
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
  target_link_libraries(G3XQ10 PRIVATE g3x_q10_dsp juce::juce_audio_utils
    juce::juce_recommended_config_flags juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)
endif()

if(G3X_BUILD_TESTS)
  enable_testing()
  add_executable(g3x_q10_tests tests/BiquadFilterTests.cpp)
  target_link_libraries(g3x_q10_tests PRIVATE g3x_q10_dsp)
  add_test(NAME g3x_q10_tests COMMAND g3x_q10_tests)
endif()
