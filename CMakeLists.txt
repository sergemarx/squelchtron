cmake_minimum_required(VERSION 3.22)
project(Squelchtron VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.6
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(Squelchtron
    COMPANY_NAME "Wasteland Audio"
    PLUGIN_MANUFACTURER_CODE Wlau
    PLUGIN_CODE Sqtr
    FORMATS VST3 Standalone
    PRODUCT_NAME "Squelchtron"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(Squelchtron PRIVATE
    PluginProcessor.cpp
    PluginEditor.cpp)

target_compile_definitions(Squelchtron PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(Squelchtron
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_processors
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
