// P5X — Copyright (c) 2026 Jeet Patel. Licensed under GPL-3.0-or-later.
//
// Test run setup: global files go to a scratch folder (never the user's %APPDATA%/P5X), and the
// plugin tests get a JUCE GUI environment for timers, parameters and the editor.

#include <catch2/reporters/catch_reporter_event_listener.hpp>
#include <catch2/reporters/catch_reporter_registrars.hpp>

#include <juce_core/juce_core.h>

#if ! P5X_TEST_NO_GUI
    #include <juce_gui_basics/juce_gui_basics.h>
#endif

#include <memory>
#include <stdlib.h>

namespace
{
class TestEnvironment final : public Catch::EventListenerBase
{
public:
    using Catch::EventListenerBase::EventListenerBase;

    void testRunStarting (const Catch::TestRunInfo&) override
    {
        // Also set by ctest; this covers running the executable by hand.
        const juce::File dataDir (P5X_TEST_DATA_DIR);
        dataDir.createDirectory();
        _putenv_s ("P5X_DATA_DIR", dataDir.getFullPathName().toRawUTF8());
        checkDataDirVisible (dataDir);

#if ! P5X_TEST_NO_GUI
        gui = std::make_unique<juce::ScopedJuceInitialiser_GUI>();
#endif
    }

    void testRunEnded (const Catch::TestRunStats&) override
    {
#if ! P5X_TEST_NO_GUI
        gui.reset();
#endif
    }

private:
    // juce::SystemStats reads the Win32 environment block, which _putenv_s updates too; verify it.
    static void checkDataDirVisible (const juce::File& dataDir)
    {
        jassertquiet (juce::SystemStats::getEnvironmentVariable ("P5X_DATA_DIR", {}) == dataDir.getFullPathName());
    }

#if ! P5X_TEST_NO_GUI
    std::unique_ptr<juce::ScopedJuceInitialiser_GUI> gui;
#endif
};
} // namespace

CATCH_REGISTER_LISTENER (TestEnvironment)
