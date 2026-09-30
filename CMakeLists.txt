# P5X — Copyright (c) 2026 Jeet Patel. Licensed under GPL-3.0-or-later.

cmake_minimum_required(VERSION 3.25)

project(P5X VERSION 0.2.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

option(P5X_BUILD_TESTS "Build the Catch2 test targets" ON)
option(P5X_ENABLE_ASIO "Build with ASIO support (Steinberg ASIO SDK, GPLv3 option)" ON)
option(P5X_VERBOSE "Keep DEBUG-level logging in Release builds" OFF)

if(NOT MSVC)
    message(FATAL_ERROR "P5X builds with MSVC only (docs/12-conventions.md)")
endif()

include(cmake/Warnings.cmake)
include(cmake/Dependencies.cmake)

# Build info for Dump state and the About box (docs/10-debug-and-harness.md)
set(P5X_GIT_HASH "unknown")
find_package(Git QUIET)
if(GIT_FOUND)
    execute_process(COMMAND "${GIT_EXECUTABLE}" rev-parse --short HEAD
                    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
                    OUTPUT_VARIABLE P5X_GIT_HASH OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
endif()

set(SRC "${CMAKE_CURRENT_SOURCE_DIR}/Source")

# --------------------------------------------------------------------------------------------------
# p5x_dsp: standard library only (docs/00-architecture.md § Dependency rules)
add_library(p5x_dsp STATIC
    ${SRC}/dsp/Envelope.h
    ${SRC}/dsp/Envelope.cpp
    ${SRC}/dsp/LadderFilterCEM.h
    ${SRC}/dsp/LadderFilterCEM.cpp
    ${SRC}/dsp/Oscillator.h
    ${SRC}/dsp/Oscillator.cpp
    ${SRC}/dsp/OutputStage.h
    ${SRC}/dsp/Random.h
    ${SRC}/dsp/Smoother.h
    ${SRC}/dsp/Voice.h
    ${SRC}/dsp/Voice.cpp
    ${SRC}/dsp/VoiceAllocator.h)
target_include_directories(p5x_dsp PUBLIC "${SRC}")
target_compile_options(p5x_dsp PRIVATE ${P5X_STRICT_FLAGS})

# p5x_midi and p5x_debug_core are INTERFACE libraries: JUCE modules compile into every binary that
# links them, so each consumer compiles these sources itself (docs/DECISIONS.md).
set(P5X_MIDI_SOURCES
    ${SRC}/midi/MidiHandler.h
    ${SRC}/midi/MidiHandler.cpp
    ${SRC}/midi/MidiLearn.h
    ${SRC}/midi/MidiLearn.cpp
    ${SRC}/midi/MidiMonitor.h)
add_library(p5x_midi INTERFACE)
target_sources(p5x_midi INTERFACE ${P5X_MIDI_SOURCES})
target_include_directories(p5x_midi INTERFACE "${SRC}")
target_link_libraries(p5x_midi INTERFACE juce::juce_audio_basics)

set(P5X_DEBUG_CORE_SOURCES
    ${SRC}/debug/DebugLog.h
    ${SRC}/debug/DebugLog.cpp
    ${SRC}/debug/MpscRing.h
    ${SRC}/debug/LogFileSink.h
    ${SRC}/debug/LogFileSink.cpp
    ${SRC}/debug/Telemetry.h)
add_library(p5x_debug_core INTERFACE)
target_sources(p5x_debug_core INTERFACE ${P5X_DEBUG_CORE_SOURCES})
target_include_directories(p5x_debug_core INTERFACE "${SRC}")
target_link_libraries(p5x_debug_core INTERFACE juce::juce_core)
target_compile_definitions(p5x_debug_core INTERFACE P5X_VERBOSE=$<BOOL:${P5X_VERBOSE}>)

# --------------------------------------------------------------------------------------------------
juce_add_binary_data(p5x_binary
    NAMESPACE P5XBinary
    HEADER_NAME P5XBinaryData.h
    SOURCES
        Resources/fonts/KodeMono/KodeMono-Regular.ttf
        Resources/fonts/KodeMono/KodeMono-Medium.ttf
        Resources/fonts/KodeMono/KodeMono-SemiBold.ttf
        Resources/fonts/KodeMono/KodeMono-Bold.ttf
        Resources/fonts/KodeMono/OFL.txt)

# Identity: exactly the table in CLAUDE.md. MANUFACTURER_CODE, PLUGIN_CODE and BUNDLE_ID never change.
# COMPANY_WEBSITE is intentionally left empty for now.
juce_add_plugin(P5X
    PRODUCT_NAME "P5X"
    COMPANY_NAME "Jeet Patel"
    COMPANY_COPYRIGHT "Copyright (c) 2026 Jeet Patel"
    BUNDLE_ID com.jeetpatel.p5x
    PLUGIN_MANUFACTURER_CODE JtPl
    PLUGIN_CODE P5xS
    VERSION ${PROJECT_VERSION}
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    VST3_CATEGORIES Instrument Synth
    FORMATS VST3 Standalone
    COPY_PLUGIN_AFTER_BUILD FALSE)

# JUCE only generates the Windows resource file (file properties: version, company, copyright) once;
# its build rule doesn't depend on the version. Delete a stale one so the build regenerates it.
set(P5X_RC_FILE "${CMAKE_CURRENT_BINARY_DIR}/P5X_artefacts/JuceLibraryCode/P5X_resources.rc")
if(EXISTS "${P5X_RC_FILE}")
    file(READ "${P5X_RC_FILE}" P5X_RC_CONTENT)
    string(REGEX MATCH "\"FileVersion\", *\"${PROJECT_VERSION}[^0-9]" P5X_RC_HAS_VERSION "${P5X_RC_CONTENT}")
    if(NOT P5X_RC_HAS_VERSION)
        message(STATUS "P5X: version changed to ${PROJECT_VERSION}; regenerating P5X_resources.rc")
        file(REMOVE "${P5X_RC_FILE}")
    endif()
endif()

set(P5X_PLUGIN_SOURCES
    ${SRC}/AppPaths.h
    ${SRC}/AppPaths.cpp
    ${SRC}/PluginProcessor.h
    ${SRC}/PluginProcessor.cpp
    ${SRC}/PluginEditor.h
    ${SRC}/PluginEditor.cpp
    ${SRC}/params/ParameterIDs.h
    ${SRC}/params/ParameterLayout.h
    ${SRC}/params/ParameterLayout.cpp
    ${SRC}/debug/LogService.h
    ${SRC}/debug/LogService.cpp
    ${SRC}/debug/DebugConsole.h
    ${SRC}/debug/DebugConsole.cpp
    ${SRC}/ui/LookAndFeelP5X.h
    ${SRC}/ui/LookAndFeelP5X.cpp
    ${SRC}/ui/PlaceholderPanel.h
    ${SRC}/ui/PlaceholderPanel.cpp
    ${SRC}/ui/OnScreenKeyboard.h
    ${SRC}/ui/OnScreenKeyboard.cpp
    ${SRC}/standalone/StandaloneServices.h
    ${SRC}/standalone/SettingsDialog.h
    ${SRC}/standalone/SettingsDialog.cpp
    ${SRC}/standalone/ComputerKeyboardInput.h
    ${SRC}/standalone/ComputerKeyboardInput.cpp
    ${SRC}/standalone/StandaloneApp.cpp)
target_sources(P5X PRIVATE ${P5X_PLUGIN_SOURCES})
target_include_directories(P5X PUBLIC "${SRC}")

# JUCE configuration must be PUBLIC so the VST3 and Standalone wrapper targets see the same flags.
target_compile_definitions(P5X PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP=1
    JUCE_DISPLAY_SPLASH_SCREEN=0
    P5X_GIT_HASH="${P5X_GIT_HASH}")

if(P5X_ENABLE_ASIO)
    target_compile_definitions(P5X PUBLIC JUCE_ASIO=1)
    target_include_directories(P5X PRIVATE "${asiosdk_SOURCE_DIR}/common")
endif()

target_link_libraries(P5X
    PRIVATE
        p5x_binary
        p5x_dsp
        p5x_midi
        p5x_debug_core
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags)

p5x_strict_sources(${P5X_PLUGIN_SOURCES} ${P5X_MIDI_SOURCES} ${P5X_DEBUG_CORE_SOURCES})

# --------------------------------------------------------------------------------------------------
if(P5X_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
