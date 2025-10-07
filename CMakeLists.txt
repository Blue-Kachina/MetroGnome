cmake_minimum_required(VERSION 3.22)

project(MetroGnome VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# Avoid copying plugins into system folders after build (requires admin)
set(JUCE_COPY_PLUGIN_AFTER_BUILD OFF CACHE BOOL "Disable copy-after-build for plugins" FORCE)

# Fetch JUCE using FetchContent (no global install required)
FetchContent_Declare(
    juce
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.10
)

FetchContent_MakeAvailable(juce)

# Define the plugin target
juce_add_plugin(MetroGnome
    COMPANY_NAME "MetroGnome"
    BUNDLE_ID com.metronome.metrogome
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE MetG
    PLUGIN_CODE MtGn
    FORMATS VST3
    VST3_CATEGORIES Instrument
)

juce_generate_juce_header(MetroGnome)

# Add sources
target_sources(MetroGnome PRIVATE
    src/PluginProcessor.cpp
    src/PluginProcessor.h
    src/PluginEditor.cpp
    src/PluginEditor.h
)

# Recommended JUCE compile-time flags for smaller, RT-safe build
target_compile_definitions(MetroGnome PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_STRICT_REFCOUNTEDPOINTER=1
)

# Prefer static runtime on MSVC for simpler deployment later (can be revisited)
if (MSVC)
    foreach(flag_var CMAKE_C_FLAGS_RELEASE CMAKE_C_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELEASE CMAKE_CXX_FLAGS_DEBUG)
        if(${flag_var} MATCHES "/MD")
            string(REGEX REPLACE "/MD" "/MT" ${flag_var} "${${flag_var}}")
        endif()
    endforeach()
endif()

# Link JUCE modules
target_link_libraries(MetroGnome PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_audio_basics
    juce::juce_dsp
)

# Organize source tree nicely in IDEs
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES
    src/PluginProcessor.cpp
    src/PluginProcessor.h
    src/PluginEditor.cpp
    src/PluginEditor.h
)