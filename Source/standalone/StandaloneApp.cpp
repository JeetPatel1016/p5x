// P5X — Copyright (c) 2026 Jeet Patel. Licensed under GPL-3.0-or-later.
//
// Custom Standalone app (JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP): the test harness from
// 10-debug-and-harness.md. Reuses JUCE's StandalonePluginHolder for the device and plugin plumbing.

#include "AppPaths.h"
#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "standalone/ComputerKeyboardInput.h"
#include "standalone/StandaloneServices.h"
#include "ui/LookAndFeelP5X.h"

// JUCE's standalone holder header isn't self-contained: the module headers must come first.
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>

namespace p5x::standalone
{
namespace
{
const juce::String aboutText()
{
    return "P5X " JucePlugin_VersionString + juce::String (juce::CharPointer_UTF8 (" \xc2\xb7 \xc2\xa9 2026 Jeet Patel"));
}
} // namespace

//==================================================================================================
class MainWindow final : public juce::DocumentWindow,
                         public juce::MenuBarModel,
                         public Services,
                         private juce::KeyListener
{
public:
    MainWindow (juce::StandalonePluginHolder& h, juce::PropertiesFile& props)
        : DocumentWindow ("P5X", ui::colours::panelBg, DocumentWindow::minimiseButton | DocumentWindow::closeButton),
          holder (h), properties (props)
    {
        setLookAndFeel (&lookAndFeel);
        setUsingNativeTitleBar (true);
        setMenuBar (this);

        computerKeyboardEnabled = properties.getBoolValue ("computerKeyboard", true);

        editor.reset (holder.processor->createEditorAndMakeActive());
        setContentNonOwned (editor.get(), true);
        setResizable (false, false);

        const auto state = properties.getValue ("windowState");

        if (state.isEmpty() || ! restoreWindowStateFromString (state))
            centreWithSize (getWidth(), getHeight());

        addKeyListener (this);
        setServices (this);
        setVisible (true);
    }

    ~MainWindow() override
    {
        setServices (nullptr);
        properties.setValue ("windowState", getWindowStateAsString());
        removeKeyListener (this);
        setMenuBar (nullptr);
        clearContentComponent();
        editor.reset();
        setLookAndFeel (nullptr);
    }

    void closeButtonPressed() override { juce::JUCEApplication::getInstance()->systemRequestedQuit(); }

    void activeWindowStatusChanged() override
    {
        // Focus lost: don't leave notes hanging.
        if (! isActiveWindow())
            releaseComputerKeys();
    }

    //==============================================================================================
    // MenuBarModel: settings (gear icon + app menu) and About (CLAUDE.md § Product identity).
    juce::StringArray getMenuBarNames() override { return { "P5X" }; }

    juce::PopupMenu getMenuForIndex (int, const juce::String&) override
    {
        juce::PopupMenu menu;
        menu.addItem (1, "Audio/MIDI Settings...");
        menu.addItem (2, "About P5X");
        menu.addSeparator();
        menu.addItem (3, "Quit");
        return menu;
    }

    void menuItemSelected (int itemId, int) override
    {
        switch (itemId)
        {
            case 1:
                if (auto* p5xEditor = dynamic_cast<P5XAudioProcessorEditor*> (editor.get()))
                    p5xEditor->showSettings();
                break;

            case 2:
                juce::AlertWindow::showAsync (juce::MessageBoxOptions()
                                                  .withIconType (juce::MessageBoxIconType::InfoIcon)
                                                  .withTitle ("About P5X")
                                                  .withMessage (aboutText())
                                                  .withButton ("OK")
                                                  .withAssociatedComponent (this),
                                              nullptr);
                break;

            case 3:
                juce::JUCEApplication::getInstance()->systemRequestedQuit();
                break;

            default:
                break;
        }
    }

    //==============================================================================================
    // Services
    juce::AudioDeviceManager& getDeviceManager() override { return holder.deviceManager; }

    void resetAudio() override
    {
        holder.deviceManager.closeAudioDevice();
        holder.deviceManager.restartLastAudioDevice();
        P5X_LOG (Info, Standalone, instanceId(), "Audio device restarted");
    }

    bool isComputerKeyboardEnabled() const override { return computerKeyboardEnabled; }

    void setComputerKeyboardEnabled (bool enabled) override
    {
        computerKeyboardEnabled = enabled;
        properties.setValue ("computerKeyboard", enabled);

        if (! enabled)
            releaseComputerKeys();
    }

private:
    P5XAudioProcessor& processor() const { return static_cast<P5XAudioProcessor&> (*holder.processor); }
    uint16_t instanceId() const { return processor().getInstanceId(); }

    // Active only when this window has focus and no text field is focused.
    bool computerKeyboardActive() const
    {
        return computerKeyboardEnabled && isActiveWindow()
            && dynamic_cast<juce::TextEditor*> (juce::Component::getCurrentlyFocusedComponent()) == nullptr;
    }

    bool keyPressed (const juce::KeyPress& key, juce::Component*) override
    {
        if (! computerKeyboardActive() || key.getModifiers().isCommandDown() || key.getModifiers().isAltDown())
            return false;

        auto character = key.getTextCharacter();

        if (character == 0)
            character = (juce::juce_wchar) key.getKeyCode();

        const auto lower = juce::CharacterFunctions::toLowerCase (character);
        const bool handled = lower == 'z' || lower == 'x' || ComputerKeyboardInput::offsetForKey (lower) >= 0;

        if (const auto event = keys.keyDown (lower))
            processor().injectNoteOn (event->note, ComputerKeyboardInput::kVelocity);

        return handled;
    }

    bool keyStateChanged (bool isKeyDown, juce::Component*) override
    {
        if (isKeyDown)
            return false;

        for (const auto key : keys.getHeldKeys())
            if (! juce::KeyPress::isKeyCurrentlyDown ((int) juce::CharacterFunctions::toUpperCase (key)))
                if (const auto event = keys.keyUp (key))
                    processor().injectNoteOff (event->note);

        return false;
    }

    void releaseComputerKeys()
    {
        for (const auto& event : keys.releaseAll())
            processor().injectNoteOff (event.note);
    }

    juce::StandalonePluginHolder& holder;
    juce::PropertiesFile& properties;
    ui::LookAndFeelP5X lookAndFeel;
    std::unique_ptr<juce::AudioProcessorEditor> editor;
    ComputerKeyboardInput keys;
    bool computerKeyboardEnabled = true;
};

//==================================================================================================
class App final : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override { return JucePlugin_Name; }
    const juce::String getApplicationVersion() override { return JucePlugin_VersionString; }
    bool moreThanOneInstanceAllowed() override { return true; }
    void anotherInstanceStarted (const juce::String&) override {}

    void initialise (const juce::String&) override
    {
        paths::appDataDir().createDirectory();

        juce::PropertiesFile::Options options;
        options.storageFormat = juce::PropertiesFile::storeAsXML;
        options.millisecondsBeforeSaving = 2000;
        properties = std::make_unique<juce::PropertiesFile> (paths::standaloneFile(), options);

        const auto savedSetup = properties->getXmlValue ("audioSetup");
        const auto savedDevice = savedSetup != nullptr ? savedSetup->getStringAttribute ("audioOutputDeviceName") : juce::String();

        juce::Array<juce::StandalonePluginHolder::PluginInOuts> channels;
        channels.add ({ 0, 2 }); // no inputs until "Route input into filter" (milestone 2), stereo out
        holder = std::make_unique<juce::StandalonePluginHolder> (properties.get(), false, juce::String(), nullptr,
                                                                 channels, false);

        auto& deviceManager = holder->deviceManager;
        const auto id = static_cast<P5XAudioProcessor&> (*holder->processor).getInstanceId();

        // Crash safety (10-debug-and-harness.md): if the saved device can't be opened or isn't running,
        // fall back to the default device and log WARN; never exit.
        auto isRunning = [&deviceManager]
        {
            auto* device = deviceManager.getCurrentAudioDevice();
            return device != nullptr && device->isOpen() && device->isPlaying();
        };

        if (! isRunning())
        {
            auto* failed = deviceManager.getCurrentAudioDevice();
            const auto name = failed != nullptr ? failed->getName() : savedDevice;
            const auto reason = failed != nullptr ? failed->getLastError() : juce::String();

            deviceManager.closeAudioDevice();
            deviceManager.initialiseWithDefaultDevices (0, 2);
            P5X_LOG (Warn, Standalone, id, "Audio device '%s' didn't start%s%s; trying the default device",
                     name.toRawUTF8(), reason.isNotEmpty() ? ": " : "", reason.toRawUTF8());
        }
        else if (savedDevice.isNotEmpty() && deviceManager.getCurrentAudioDevice()->getName() != savedDevice)
        {
            P5X_LOG (Warn, Standalone, id, "Saved audio device '%s' failed to open; using '%s'", savedDevice.toRawUTF8(),
                     deviceManager.getCurrentAudioDevice()->getName().toRawUTF8());
        }

        if (! isRunning())
            P5X_LOG (Error, Standalone, id, "No audio device is running; choose one in Settings");

        if (auto* device = deviceManager.getCurrentAudioDevice(); device != nullptr && isRunning())
            P5X_LOG (Info, Standalone, id, "Audio: %s / %s, %.0f Hz, %d samples", device->getTypeName().toRawUTF8(),
                     device->getName().toRawUTF8(), device->getCurrentSampleRate(), device->getCurrentBufferSizeSamples());

        window = std::make_unique<MainWindow> (*holder, *properties);
    }

    void shutdown() override
    {
        if (holder != nullptr)
            holder->savePluginState();

        window.reset();
        holder.reset(); // saves the audio device state
        properties->saveIfNeeded();
    }

    void systemRequestedQuit() override
    {
        if (juce::ModalComponentManager::getInstance()->cancelAllModalComponents())
        {
            juce::Timer::callAfterDelay (100, []
                                         {
                                             if (auto* app = juce::JUCEApplicationBase::getInstance())
                                                 app->systemRequestedQuit();
                                         });
            return;
        }

        quit();
    }

private:
    std::unique_ptr<juce::PropertiesFile> properties;
    std::unique_ptr<juce::StandalonePluginHolder> holder;
    std::unique_ptr<MainWindow> window;
};
} // namespace p5x::standalone

juce::JUCEApplicationBase* juce_CreateApplication();
juce::JUCEApplicationBase* juce_CreateApplication()
{
    return new p5x::standalone::App();
}
