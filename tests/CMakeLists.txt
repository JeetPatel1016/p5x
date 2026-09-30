# P5X — Copyright (c) 2026 Jeet Patel. Licensed under GPL-3.0-or-later.
#
# Two test targets (docs/11-testing.md § Tooling):
#   p5x_tests         dsp/, midi/ and the logging core only, no plugin wrapper
#   p5x_plugin_tests  the processor, run headless, linked against the plugin's shared code

include(Catch)

set(TEST_DATA_DIR "${CMAKE_CURRENT_BINARY_DIR}/p5x-test-data")

set(SUPPORT_SOURCES
    support/Spectrum.h
    support/AllocationCounter.h
    support/AllocationCounter.cpp
    support/TestEnvironment.cpp)

# --------------------------------------------------------------------------------------------------
set(UNIT_TEST_SOURCES
    dsp/EnvelopeTests.cpp
    dsp/LadderFilterTests.cpp
    dsp/ModulationSourceTests.cpp
    dsp/OscillatorTests.cpp
    dsp/OutputStageTests.cpp
    dsp/RandomTests.cpp
    dsp/SmootherTests.cpp
    dsp/VoiceTests.cpp
    dsp/VoiceAllocatorTests.cpp
    midi/MidiHandlerTests.cpp
    midi/MidiLearnTests.cpp
    midi/MidiStressTests.cpp
    debug/DebugLogTests.cpp
    debug/TelemetryTests.cpp)

juce_add_console_app(p5x_tests PRODUCT_NAME "p5x_tests")
target_sources(p5x_tests PRIVATE ${SUPPORT_SOURCES} ${UNIT_TEST_SOURCES})
target_include_directories(p5x_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_definitions(p5x_tests PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 P5X_TEST_NO_GUI=1
                                             P5X_TEST_DATA_DIR="${TEST_DATA_DIR}")
target_link_libraries(p5x_tests PRIVATE
    p5x_dsp
    p5x_midi
    p5x_debug_core
    Catch2::Catch2WithMain
    juce::juce_recommended_config_flags)

p5x_strict_sources(${SUPPORT_SOURCES} ${UNIT_TEST_SOURCES} ${P5X_MIDI_SOURCES} ${P5X_DEBUG_CORE_SOURCES})
catch_discover_tests(p5x_tests PROPERTIES ENVIRONMENT "P5X_DATA_DIR=${TEST_DATA_DIR}")

# --------------------------------------------------------------------------------------------------
set(PLUGIN_TEST_SOURCES
    plugin/TestProcessor.h
    plugin/AliasingTests.cpp
    plugin/EngineTests.cpp
    plugin/GoldenTests.cpp
    plugin/ModulationTests.cpp
    plugin/IdentityTests.cpp
    plugin/ParameterTests.cpp
    plugin/PerformanceTests.cpp
    plugin/StateTests.cpp
    plugin/ProcessorTests.cpp
    plugin/RobustnessTests.cpp
    plugin/ComputerKeyboardTests.cpp
    plugin/EditorTests.cpp)

add_executable(p5x_plugin_tests ${SUPPORT_SOURCES} ${PLUGIN_TEST_SOURCES})
target_include_directories(p5x_plugin_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}" "${juce_SOURCE_DIR}/modules")
target_compile_definitions(p5x_plugin_tests PRIVATE P5X_TEST_NO_GUI=0 P5X_TEST_DATA_DIR="${TEST_DATA_DIR}"
                                                    P5X_TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(p5x_plugin_tests PRIVATE P5X Catch2::Catch2WithMain)

p5x_strict_sources(${PLUGIN_TEST_SOURCES})
catch_discover_tests(p5x_plugin_tests PROPERTIES ENVIRONMENT "P5X_DATA_DIR=${TEST_DATA_DIR}")
