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
    COMPANY_NAME "Otitis Media"
    BUNDLE_ID com.otitismedia.metrognome
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE OMed
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
    src/Timing.h
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

# ---------------- Tests (Phase 2b) ----------------
# Lightweight console tests for Timing utilities
add_executable(MetroGnome_Tests
    src/TimingTests.cpp
    src/Timing.h
)

# No JUCE dependency needed; pure C++17
set_target_properties(MetroGnome_Tests PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED YES)

include(CTest)
enable_testing()
add_test(NAME TimingTests COMMAND MetroGnome_Tests)

# ---------------- Install & Packaging (Phase 9) ----------------
# Install the VST3 bundle into the standard platform-specific location.
# We set the packaging install prefix so CPack installers deploy directly to host-discoverable paths.
if (WIN32)
    # Windows standard VST3 location
    # Use per-user VST3 location to avoid UAC/admin requirements
    set(VST3_INSTALL_PREFIX "$ENV{LOCALAPPDATA}/Programs/Common/VST3")
    set(CPACK_GENERATOR "WIX;ZIP")
    # Stable GUID required by WiX to support upgrades — do not change after release
    set(CPACK_WIX_UPGRADE_GUID "8E6A2E3A-7B7B-4DBE-9D57-1D5F8B3D9C12")
elseif(APPLE)
    # macOS standard VST3 location
    set(VST3_INSTALL_PREFIX "/Library/Audio/Plug-Ins/VST3")
    set(CPACK_GENERATOR "productbuild;DragNDrop")
else()
    # Linux: prefer user scope to avoid root requirements
    set(VST3_INSTALL_PREFIX "$ENV{HOME}/.vst3")
    set(CPACK_GENERATOR "TGZ;ZIP")
endif()

# Install rules: place the plugin bundle at the root of the install prefix
install(TARGETS MetroGnome BUNDLE DESTINATION ".")

# Basic package metadata
set(CPACK_PACKAGE_NAME "MetroGnome")
set(CPACK_PACKAGE_VENDOR "MetroGnome")
set(CPACK_PACKAGE_CONTACT "devnull@example.com")
set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "MetroGnome — JUCE VST3 Metronome Plugin")

# Ensure CPack installs into proper VST3 directory by default
set(CPACK_PACKAGING_INSTALL_PREFIX "${VST3_INSTALL_PREFIX}")

# macOS productbuild identifiers
if(APPLE)
    set(CPACK_PRODUCTBUILD_IDENTIFIER "com.otitismedia.metrognome.pkg")
endif()

include(CPack)