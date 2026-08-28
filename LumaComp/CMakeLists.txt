cmake_minimum_required(VERSION 3.15)

project(LumaComp VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

if(NOT DEFINED JUCE_ROOT)
    set(JUCE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/JUCE-6.1.6")
endif()
if(NOT DEFINED VST2_SDK_ROOT)
    set(VST2_SDK_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/vst2sdk")
endif()
if(NOT DEFINED VST3_SDK_ROOT)
    set(VST3_SDK_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/vst3sdk/vst3sdk-3.7.8_build_34")
endif()

add_subdirectory(${JUCE_ROOT} juce_build)

juce_set_vst2_sdk_path(${VST2_SDK_ROOT})
juce_set_vst3_sdk_path(${VST3_SDK_ROOT})

juce_add_plugin(LumaComp
    VERSION ${PROJECT_VERSION}
    COMPANY_NAME "Luma Audio"
    COMPANY_WEBSITE "https://lumaaudio.example.com"
    COMPANY_EMAIL "luma@example.com"
    COMPANY_COPYRIGHT "Copyright 2026 Luma Audio"
    PRODUCT_NAME "LumaComp"
    DESCRIPTION "3-band compressor with harmonic brightening"
    FORMATS VST3 VST
    NEEDS_MIDI_INPUT false
    NEEDS_MIDI_OUTPUT false
    IS_SYNTH false
    EDITOR_NAME LumaCompAudioProcessorEditor
)

target_sources(LumaComp PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
)

target_link_libraries(LumaComp PRIVATE
    juce::juce_audio_processors
    juce::juce_dsp
    juce::juce_gui_basics
    juce::juce_graphics
)

target_compile_definitions(LumaComp PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_USE_XATTR=0
    JUCE_DISABLE_JUCE_VERSION_PRINTING=1
)

# Statically link the MinGW runtime so the plugins have no external
# dependencies on libgcc/libstdc++/winpthread DLLs.
foreach(plugin_target IN ITEMS LumaComp_VST LumaComp_VST3)
    target_link_options(${plugin_target} PRIVATE -static -static-libgcc -static-libstdc++)
endforeach()

# ---------------------------------------------------------------------------
# Optional processor-side meter test (built only when BUILD_TEST=ON)
# ---------------------------------------------------------------------------
option(BUILD_TEST "Build the processor meter test" OFF)
if(BUILD_TEST)
    add_executable(LumaCompMeterTest tests/ProcessorMeterTest.cpp)
    target_link_libraries(LumaCompMeterTest PRIVATE LumaComp)
    target_include_directories(LumaCompMeterTest PRIVATE
        Source
        "${JUCE_ROOT}/modules")
    target_compile_definitions(LumaCompMeterTest PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_DISABLE_JUCE_VERSION_PRINTING=1)
endif()
