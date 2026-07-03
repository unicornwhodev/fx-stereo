cmake_minimum_required(VERSION 3.22)

project(MusiqueStereo VERSION 1.1.0 LANGUAGES C CXX)

enable_testing()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if (NOT TARGET juce::juce_audio_utils)
    set(_musique_stereo_juce_source "")
    foreach(_candidate
        "${CMAKE_CURRENT_SOURCE_DIR}/JUCE"
        "${CMAKE_CURRENT_SOURCE_DIR}/../JUCE"
        "${CMAKE_CURRENT_SOURCE_DIR}/../fx-creative/JUCE"
        "${CMAKE_CURRENT_SOURCE_DIR}/../fx-pitchtime/build-codex-nmake/_deps/juce-src"
        "${CMAKE_BINARY_DIR}/_deps/juce-src"
        "${CMAKE_CURRENT_SOURCE_DIR}/../build/_deps/juce-src"
        "${CMAKE_CURRENT_SOURCE_DIR}/build/_deps/juce-src")
        if (EXISTS "${_candidate}/CMakeLists.txt")
            set(_musique_stereo_juce_source "${_candidate}")
            break()
        endif()
    endforeach()

    if (_musique_stereo_juce_source)
        add_subdirectory("${_musique_stereo_juce_source}" JUCE)
    else()
        include(FetchContent)
        FetchContent_Declare(
            JUCE
            GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
            GIT_TAG        8.0.4
            GIT_SHALLOW    TRUE
        )
        FetchContent_MakeAvailable(JUCE)
    endif()
endif()

if (NOT TARGET FXShared)
    add_library(FXShared INTERFACE)
    target_include_directories(FXShared INTERFACE
        "${CMAKE_CURRENT_SOURCE_DIR}/../FXShared"
    )
endif()

juce_add_plugin(MusiqueStereo
    COMPANY_NAME "Musique"
    BUNDLE_ID "com.musique.stereo"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE MQFX
    PLUGIN_CODE MST1
    FORMATS Standalone VST3
    PRODUCT_NAME "Musique Stereo"
    ICON_BIG   "${CMAKE_CURRENT_SOURCE_DIR}/Source/Assets/icon_big.png"
    ICON_SMALL "${CMAKE_CURRENT_SOURCE_DIR}/Source/Assets/icon_small.png"
)

juce_generate_juce_header(MusiqueStereo)

target_sources(MusiqueStereo PRIVATE
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/StereoEngines.h
)

target_compile_definitions(MusiqueStereo PRIVATE
    MUSIQUE_STEREO_DSP_TESTS=0
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0
)

juce_add_binary_data(MusiqueStereoAssets
    SOURCES
        Source/Assets/icon_big.png
        Source/Assets/icon_small.png
        Source/Assets/favicon.png
        Source/Assets/logo.png
)

target_link_libraries(MusiqueStereo PRIVATE
    MusiqueStereoAssets
    FXShared
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
    juce::juce_recommended_lto_flags
)

juce_add_console_app(MusiqueStereoDSPTests
    PRODUCT_NAME "Musique Stereo DSP Tests"
)

juce_generate_juce_header(MusiqueStereoDSPTests)

target_sources(MusiqueStereoDSPTests PRIVATE
    Tests/StereoProcessorTests.cpp
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/StereoEngines.h
)

target_compile_definitions(MusiqueStereoDSPTests PRIVATE
    MUSIQUE_STEREO_DSP_TESTS=1
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0
)

target_include_directories(MusiqueStereoDSPTests PRIVATE
    Source
)

target_link_libraries(MusiqueStereoDSPTests PRIVATE
    FXShared
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
)

add_test(NAME MusiqueStereoDSPTests COMMAND MusiqueStereoDSPTests)
