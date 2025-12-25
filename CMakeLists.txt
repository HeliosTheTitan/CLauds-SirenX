cmake_minimum_required(VERSION 3.22)

project(SirenX VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find JUCE (adjust path as needed)
# Option 1: If JUCE is installed system-wide or via CPM
# find_package(JUCE CONFIG REQUIRED)

# Option 2: Add JUCE as subdirectory (recommended)
# add_subdirectory(JUCE)

# Option 3: Use FetchContent
include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 7.0.9
)
FetchContent_MakeAvailable(JUCE)

# Plugin target
juce_add_plugin(SirenX
    COMPANY_NAME "Solar Productions"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    PLUGIN_MANUFACTURER_CODE Solr
    PLUGIN_CODE Sirn
    BUNDLE_ID "com.SolarProductions.SirenX"
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "SirenX"
)

# Source files
target_sources(SirenX
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginProcessor.h
        Source/PluginEditor.cpp
        Source/PluginEditor.h
        Source/CustomLookAndFeel.h
        Source/CustomLookAndFeel.cpp
        Source/ReverbEngine.h
)

# Preprocessor definitions
target_compile_definitions(SirenX
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
)

# Generate JuceHeader.h
juce_generate_juce_header(SirenX)

# Link JUCE modules
target_link_libraries(SirenX
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_gui_basics
        juce::juce_graphics
        juce::juce_core
        juce::juce_data_structures
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# Compiler warnings
if(MSVC)
    target_compile_options(SirenX PRIVATE /W4)
else()
    target_compile_options(SirenX PRIVATE -Wall -Wextra -Wpedantic)
endif()
