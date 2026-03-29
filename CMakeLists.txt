cmake_minimum_required(VERSION 3.22)

project(Wired VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ──────────────────────────────────────────────
#  JUCE — auto-fetched via FetchContent
# ──────────────────────────────────────────────
include(FetchContent)

FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        7.0.9
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(JUCE)

# ──────────────────────────────────────────────
#  Embedded binary resources (GIF)
# ──────────────────────────────────────────────
juce_add_binary_data(WiredAssets
    SOURCES
        resources/lain.gif
)

# ──────────────────────────────────────────────
#  Plugin target
# ──────────────────────────────────────────────
juce_add_plugin(Wired
    COMPANY_NAME                "WiredDev"
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     FALSE
    PLUGIN_MANUFACTURER_CODE    WRED
    PLUGIN_CODE                 Wrd1
    FORMATS                     VST3 AU Standalone
    PRODUCT_NAME                "Wired"
    VERSION                     "1.0.0"
)

target_sources(Wired
    PRIVATE
        PluginProcessor.cpp
        PluginProcessor.h
        PluginEditor.cpp
        PluginEditor.h
)

target_compile_definitions(Wired
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(Wired
    PRIVATE
        WiredAssets
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_gui_basics
        juce::juce_graphics
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
