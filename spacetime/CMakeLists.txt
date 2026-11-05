cmake_minimum_required(VERSION 3.22)
project(SPACE_TIME VERSION 1.0.0)

# Плагин не требует установки Visual C++ Redistributable на целевом ПК
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# JUCE: если указан -DJUCE_DIR=... берём локальную копию, иначе скачиваем автоматически.
if(DEFINED JUCE_DIR AND EXISTS "${JUCE_DIR}/CMakeLists.txt")
    add_subdirectory("${JUCE_DIR}" JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.6
        GIT_SHALLOW ON)
    FetchContent_MakeAvailable(JUCE)
endif()

juce_add_plugin(SPACE_TIME
    COMPANY_NAME "SPACE TIME AUDIO"
    BUNDLE_ID "audio.spacetime.reverb"
    PLUGIN_MANUFACTURER_CODE SpTm
    PLUGIN_CODE SpR1
    FORMATS VST3 Standalone
    VST3_CATEGORIES Fx Reverb
    PRODUCT_NAME "SPACE-TIME"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE)

juce_generate_juce_header(SPACE_TIME)

target_sources(SPACE_TIME PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_features(SPACE_TIME PRIVATE cxx_std_17)

target_compile_definitions(SPACE_TIME PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(SPACE_TIME
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags)
