cmake_minimum_required(VERSION 3.22)

project(MetroGnome VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# Avoid copying plugins into system folders after build (requires admin)
set(JUCE_COPY_PLUGIN_AFTER_BUILD OFF CACHE BOOL "Disable copy-after-build for plugins" FORCE)

# Fetch JUCE using FetchContent (no global install required)
# Also ensure JUCE does not add its own install() rules to our install step
set(JUCE_ENABLE_INSTALL OFF CACHE BOOL "Disable JUCE self-install during our install step" FORCE)
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
# UI assets are embedded via an existing binary data target (MetroGnome_rc_lib)
# If this target is provided by another CMake file/preset, just link to it below.

# Plugin sources
target_sources(MetroGnome PRIVATE
    src/PluginProcessor.cpp
    src/PluginProcessor.h
    src/PluginEditor.cpp
    src/PluginEditor.h
    src/Timing.h
)

# Embed in-repo assets as a fallback (namespaced to avoid clashes)
juce_add_binary_data(MetroGnomeAssets
    HEADER_NAME MetroAssets.h
    NAMESPACE MetroAssets
    SOURCES
        assets/images/metrognome-a.png
        assets/images/metrognome-b.png
)

# Link embedded resources libraries
# External resource lib (if it exists in this build)
target_link_libraries(MetroGnome PRIVATE MetroGnome_rc_lib)
# Our in-repo assets fallback
target_link_libraries(MetroGnome PRIVATE MetroGnomeAssets)

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
    # Windows per-user VST3 location (no admin required)
    set(VST3_INSTALL_PREFIX "$ENV{LOCALAPPDATA}/Programs/Common/VST3")
    set(CPACK_GENERATOR "WIX;ZIP")
    # Stable GUID required by WiX to support upgrades — do not change after release
    set(CPACK_WIX_UPGRADE_GUID "8E6A2E3A-7B7B-4DBE-9D57-1D5F8B3D9C12")
elseif(APPLE)
    # macOS per-user VST3 location (no sudo required)
    set(VST3_INSTALL_PREFIX "$ENV{HOME}/Library/Audio/Plug-Ins/VST3")
    set(CPACK_GENERATOR "productbuild;DragNDrop")
else()
    # Linux per-user VST3 location
    set(VST3_INSTALL_PREFIX "$ENV{HOME}/.vst3")
    set(CPACK_GENERATOR "TGZ;ZIP")
endif()

# Default install prefix: if user didn't override, install straight to the VST3 folder
if (CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "${VST3_INSTALL_PREFIX}" CACHE PATH "Install path prefix" FORCE)
endif()

# Install rules: install the full VST3 bundle directory that JUCE generates
# JUCE writes the correctly structured bundle to the artefacts directory
# This ensures DAWs like Cubase see a .vst3 folder with Contents/... inside, not a flat file
install(DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/MetroGnome_artefacts/$<CONFIG>/VST3/MetroGnome.vst3" DESTINATION "." COMPONENT MetroGnomePlugin)
# Also install LICENSE so archives/installers include it
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/LICENSE DESTINATION "." COMPONENT MetroGnomePlugin)

# Convenience target: run installation from within IDEs (CLion)
add_custom_target(deploy
    COMMAND "${CMAKE_COMMAND}" --install "${CMAKE_BINARY_DIR}" --component MetroGnomePlugin
    COMMENT "Installing MetroGnome to ${CMAKE_INSTALL_PREFIX}"
)

# Basic package metadata
set(CPACK_PACKAGE_NAME "MetroGnome")
# Manufacturer shown in MSI (ARP): use company name
set(CPACK_PACKAGE_VENDOR "Otitis Media")
set(CPACK_PACKAGE_CONTACT "devnull@example.com")
set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "MetroGnome — JUCE VST3 Metronome Plugin")
set(CPACK_PACKAGE_LICENSE "MIT")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://example.com/metrognome")

# Ensure CPack installs into proper VST3 directory by default
set(CPACK_PACKAGING_INSTALL_PREFIX "${VST3_INSTALL_PREFIX}")

# macOS productbuild identifiers
if(APPLE)
    set(CPACK_PRODUCTBUILD_IDENTIFIER "com.otitismedia.metrognome.pkg")
endif()

include(CPack)