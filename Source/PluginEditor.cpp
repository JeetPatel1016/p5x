// P5X — Copyright (c) 2026 Jeet Patel. Licensed under GPL-3.0-or-later.
#include "PluginEditor.h"
#include "debug/DebugConsole.h"
#include "standalone/SettingsDialog.h"

using namespace p5x;

namespace
{
constexpr int kWidth = 1280;
constexpr int kTopBarHeight = 44;
constexpr int kKeyboardHeight = 120;
} // namespace

//==================================================================================================
class P5XAudioProcessorEditor::MidiLed : public juce::Component
{
public:
    void setLit (bool shouldBeLit)
    {
        if (lit != shouldBeLit)
        {
            lit = shouldBeLit;
            repaint();
        }
    }

    void paint (juce::Graphics& g) override
    {
        const auto dot = getLocalBounds().toFloat().withSizeKeepingCentre (10.0f, 10.0f);
        g.setColour (lit ? ui::colours::accent : ui::colours::ledOff);
        g.fillEllipse (dot);
    }

private:
    bool lit = false;
};

// A plain top-level window for the Debug console and Settings dialog (flat, code-drawn: 08-ui.md).
class P5XAudioProcessorEditor::ToolWindow : public juce::DocumentWindow
{
public:
    ToolWindow (const juce::String& title, juce::LookAndFeel& lnf, std::function<void()> onCloseToUse)
        : DocumentWindow (title, ui::colours::panelBg, DocumentWindow::closeButton), onClose (std::move (onCloseToUse))
    {
        setLookAndFeel (&lnf);
        setUsingNativeTitleBar (true);
    }

    ~ToolWindow() override { setLookAndFeel (nullptr); }

    void closeButtonPressed() override
    {
        if (onClose)
            onClose();
    }

private:
    std::function<void()> onClose;
};

//==================================================================================================
P5XAudioProcessorEditor::P5XAudioProcessorEditor (P5XAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p), panel (p), keyboard (p)
{
    setLookAndFeel (&lookAndFeel);
    tooltips.setLookAndFeel (&lookAndFeel);

    debugButton.setClickingTogglesState (true);
    debugButton.setTooltip ("Open the debug console");
    debugButton.onClick = [this] { setDebugConsoleVisible (debugButton.getToggleState()); };
    addAndMakeVisible (debugButton);

    settingsButton.setTooltip ("Audio and MIDI settings");
    settingsButton.onClick = [this] { showSettings(); };
    addAndMakeVisible (settingsButton);

    midiLed = std::make_unique<MidiLed>();
    midiLed->setTitle ("MIDI activity");
    addAndMakeVisible (*midiLed);

    for (auto* label : { &cpuLabel, &learnLabel })
    {
        label->setFont (ui::LookAndFeelP5X::font (13.0f));
        label->setColour (juce::Label::textColourId, ui::colours::label);
        addAndMakeVisible (*label);
    }

    learnLabel.setColour (juce::Label::textColourId, ui::colours::accent);

    addAndMakeVisible (panel);
    addAndMakeVisible (keyboard);

    processor.onLearnCommitted = [this] (const midi::MidiLearn::LearnCommit& commit)
    {
        const auto name = processor.getParameterByIndex (commit.paramIndex)->getName (64);
        learnedText = name + juce::String (juce::CharPointer_UTF8 (" \xe2\x86\x90 CC ")) + juce::String (commit.cc);
        learnedUntilMs = juce::Time::getMillisecondCounterHiRes() + 2000.0;
    };

    setWantsKeyboardFocus (true);
    setSize (kWidth, kTopBarHeight + ui::PlaceholderPanel::kPreferredHeight + kKeyboardHeight);
    startTimerHz (30);
}

P5XAudioProcessorEditor::~P5XAudioProcessorEditor()
{
    stopTimer();
    processor.onLearnCommitted = nullptr;
    consoleWindow.reset();
    settingsWindow.reset();
    tooltips.setLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
}

void P5XAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (ui::colours::panelBg);

    auto top = getLocalBounds().removeFromTop (kTopBarHeight);
    g.setColour (ui::colours::topBar);
    g.fillRect (top);

    g.setColour (ui::colours::text);
    g.setFont (ui::LookAndFeelP5X::font (20.0f, true));
    g.drawText ("P5X", top.removeFromLeft (80).withTrimmedLeft (14), juce::Justification::centredLeft);
}

void P5XAudioProcessorEditor::resized()
{
    auto area = getLocalBounds();
    auto top = area.removeFromTop (kTopBarHeight).reduced (8, 8);

    top.removeFromLeft (80);
    settingsButton.setBounds (top.removeFromRight (90));
    top.removeFromRight (6);
    debugButton.setBounds (top.removeFromRight (80));
    top.removeFromRight (12);
    cpuLabel.setBounds (top.removeFromRight (110));
    midiLed->setBounds (top.removeFromRight (24));
    top.removeFromRight (8);
    learnLabel.setBounds (top);

    keyboard.setBounds (area.removeFromBottom (kKeyboardHeight));
    panel.setBounds (area);
}

bool P5XAudioProcessorEditor::keyPressed (const juce::KeyPress& key)
{
    // Esc cancels a waiting MIDI Learn (07-midi.md § Flow).
    if (key == juce::KeyPress::escapeKey && processor.getMidiLearn().getWaitingParam() >= 0)
    {
        processor.getMidiLearn().cancelLearn();
        P5X_LOG (Info, Learn, processor.getInstanceId(), "Learn cancelled");
        return true;
    }

    return false;
}

void P5XAudioProcessorEditor::timerCallback()
{
    const double now = juce::Time::getMillisecondCounterHiRes();

    // MIDI LED: 80 ms flash on any incoming MIDI after the channel filter (08-ui.md).
    if (const auto activity = processor.getMidiActivityCount(); activity != lastMidiActivity)
    {
        lastMidiActivity = activity;
        ledOffAtMs = now + 80.0;
    }

    midiLed->setLit (now < ledOffAtMs);

    const auto telemetry = processor.getTelemetry();
    cpuLabel.setText ("CPU " + juce::String (telemetry.cpuPercent, 1) + "%", juce::dontSendNotification);

    const int waiting = processor.getMidiLearn().getWaitingParam();
    juce::String status;

    if (waiting >= 0)
        status = juce::String (juce::CharPointer_UTF8 ("Waiting for CC\xe2\x80\xa6 \xc2\xb7 "))
               + processor.getParameterByIndex (waiting)->getName (64)
               + juce::String (juce::CharPointer_UTF8 (" \xc2\xb7 Esc to cancel"));
    else if (now < learnedUntilMs)
        status = learnedText;

    learnLabel.setText (status, juce::dontSendNotification);
    panel.tick();

    if (consoleWindow == nullptr && debugButton.getToggleState())
        debugButton.setToggleState (false, juce::dontSendNotification);
}

void P5XAudioProcessorEditor::setDebugConsoleVisible (bool visible)
{
    if (! visible)
    {
        consoleWindow.reset();
        debugButton.setToggleState (false, juce::dontSendNotification);
        return;
    }

    if (consoleWindow == nullptr)
    {
        consoleWindow = std::make_unique<ToolWindow> ("P5X Debug", lookAndFeel,
                                                      [this] { juce::MessageManager::callAsync ([safe = SafePointer<P5XAudioProcessorEditor> (this)]
                                                                                              {
                                                                                                  if (safe != nullptr)
                                                                                                      safe->setDebugConsoleVisible (false);
                                                                                              }); });
        consoleWindow->setContentOwned (new debug::DebugConsole (processor), true);
        consoleWindow->setResizable (true, false);
        consoleWindow->centreWithSize (consoleWindow->getWidth(), consoleWindow->getHeight());
    }

    consoleWindow->setVisible (true);
    consoleWindow->toFront (true);
    debugButton.setToggleState (true, juce::dontSendNotification);
}

void P5XAudioProcessorEditor::showSettings()
{
    if (settingsWindow == nullptr)
    {
        settingsWindow = std::make_unique<ToolWindow> ("P5X Settings", lookAndFeel,
                                                       [this] { juce::MessageManager::callAsync ([safe = SafePointer<P5XAudioProcessorEditor> (this)]
                                                                                               {
                                                                                                   if (safe != nullptr)
                                                                                                       safe->settingsWindow.reset();
                                                                                               }); });
        auto dialog = std::make_unique<standalone::SettingsDialog> (processor);
        dialog->onClose = [this]
        {
            juce::MessageManager::callAsync ([safe = SafePointer<P5XAudioProcessorEditor> (this)]
                                             {
                                                 if (safe != nullptr)
                                                     safe->settingsWindow.reset();
                                             });
        };
        settingsWindow->setContentOwned (dialog.release(), true);
        settingsWindow->centreWithSize (settingsWindow->getWidth(), settingsWindow->getHeight());
    }

    settingsWindow->setVisible (true);
    settingsWindow->toFront (true);
}
