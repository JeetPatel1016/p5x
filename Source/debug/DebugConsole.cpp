// P5X — Copyright (c) 2026 Jeet Patel. Licensed under GPL-3.0-or-later.
#include "debug/DebugConsole.h"
#include "PluginProcessor.h"
#include "debug/LogFileSink.h"
#include "ui/LookAndFeelP5X.h"

namespace p5x::debug
{
namespace
{
constexpr size_t kMaxEntries = LogService::kHistorySize;

juce::Colour levelColour (uint8_t level)
{
    switch ((Level) level)
    {
        case Level::Error: return ui::colours::logError;
        case Level::Warn:  return ui::colours::accent;
        case Level::Info:  return ui::colours::logInfo;
        case Level::Debug:
        default:           return ui::colours::labelDim.withAlpha (0.7f);
    }
}

juce::String timeOfDay (uint64_t timeMs)
{
    const juce::Time t ((juce::int64) timeMs);
    return t.formatted ("%H:%M:%S.") + juce::String (t.getMilliseconds()).paddedLeft ('0', 3);
}

void drawRowBackground (juce::Graphics& g, int rowNumber, bool selected, int width, int height)
{
    if (selected)
        g.fillAll (ui::colours::accent.withAlpha (0.15f));
    else if (rowNumber % 2 == 1)
        g.fillAll (ui::colours::panelBg.withAlpha (0.35f));

    juce::ignoreUnused (width, height);
}
} // namespace

//==================================================================================================
class DebugConsole::LogModel : public juce::ListBoxModel
{
public:
    explicit LogModel (DebugConsole& o) : owner (o) {}

    int getNumRows() override { return (int) owner.visible.size(); }

    void paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool selected) override
    {
        if (row < 0 || row >= getNumRows())
            return;

        drawRowBackground (g, row, selected, width, height);
        const auto& e = *owner.visible[(size_t) row];
        auto area = juce::Rectangle<int> (width, height).reduced (6, 0);

        g.setFont (ui::LookAndFeelP5X::font (12.0f));
        g.setColour (ui::colours::labelDim);
        g.drawText (timeOfDay (e.timeMs), area.removeFromLeft (96), juce::Justification::centredLeft);
        g.setColour (levelColour (e.level));
        g.drawText (levelName ((Level) e.level), area.removeFromLeft (52), juce::Justification::centredLeft);
        g.setColour (ui::colours::labelDim);
        g.drawText (juce::String (sourceName ((Source) e.source)) + (e.instanceId == 0 ? "" : " #" + juce::String (e.instanceId)),
                    area.removeFromLeft (110), juce::Justification::centredLeft);
        g.setColour (e.level == (uint8_t) Level::Debug ? ui::colours::labelDim : ui::colours::text);
        g.drawText (juce::String::fromUTF8 (e.text), area, juce::Justification::centredLeft);
    }

private:
    DebugConsole& owner;
};

class DebugConsole::MonitorModel : public juce::ListBoxModel
{
public:
    explicit MonitorModel (P5XAudioProcessor& p) : processor (p) {}

    void refresh() { rows.assign (processor.getMonitorHistory().begin(), processor.getMonitorHistory().end()); }

    int getNumRows() override { return (int) rows.size(); }

    void paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool selected) override
    {
        if (row < 0 || row >= getNumRows())
            return;

        drawRowBackground (g, row, selected, width, height);
        const auto& e = rows[(size_t) row];
        auto area = juce::Rectangle<int> (width, height).reduced (6, 0);

        const int status = e.size > 0 ? e.bytes[0] : 0;
        const bool channelMessage = status >= 0x80 && status < 0xF0;
        juce::String data;

        for (int i = 1; i < e.size; ++i)
            data << (int) e.bytes[i] << " ";

        g.setFont (ui::LookAndFeelP5X::font (12.0f));
        g.setColour (e.dimmed ? ui::colours::labelDim.withAlpha (0.45f) : ui::colours::text);
        g.drawText (juce::String (e.timeMs * 0.001, 3), area.removeFromLeft (90), juce::Justification::centredLeft);
        g.drawText (channelMessage ? "ch " + juce::String ((status & 0x0F) + 1) : "-", area.removeFromLeft (56),
                    juce::Justification::centredLeft);
        g.drawText (midi::describeType (e), area.removeFromLeft (110), juce::Justification::centredLeft);
        g.drawText (data.trim() + (e.dimmed ? "  (filtered)" : ""), area, juce::Justification::centredLeft);
    }

private:
    P5XAudioProcessor& processor;
    std::vector<midi::MonitorEvent> rows;
};

class DebugConsole::MapModel : public juce::ListBoxModel
{
public:
    explicit MapModel (P5XAudioProcessor& p) : processor (p) {}

    bool refresh()
    {
        auto map = processor.getMidiLearn().getMap();

        if (map == current)
            return false;

        current = std::move (map);
        return true;
    }

    int getNumRows() override { return juce::jmax (1, (int) current.size()); }

    void paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool selected) override
    {
        drawRowBackground (g, row, selected, width, height);
        g.setFont (ui::LookAndFeelP5X::font (12.0f));
        auto area = juce::Rectangle<int> (width, height).reduced (6, 0);

        if (current.empty())
        {
            g.setColour (ui::colours::labelDim);
            g.drawText ("No MIDI mappings. Right-click a control and choose Learn MIDI CC.", area,
                        juce::Justification::centredLeft);
            return;
        }

        if (row < 0 || row >= (int) current.size())
            return;

        auto it = std::next (current.begin(), row);
        const int index = processor.getParameterIndex (it->second);
        const auto name = index >= 0 ? processor.getParameterByIndex (index)->getName (64) : it->second;

        g.setColour (ui::colours::accent);
        g.drawText ("CC " + juce::String (it->first), area.removeFromLeft (70), juce::Justification::centredLeft);
        g.setColour (ui::colours::text);
        g.drawText (name + "  (" + it->second + ")", area, juce::Justification::centredLeft);
    }

private:
    P5XAudioProcessor& processor;
    std::map<int, juce::String> current;
};

class DebugConsole::VoicesModel : public juce::ListBoxModel
{
public:
    void update (const TelemetrySnapshot& s) { snapshot = s; }

    int getNumRows() override { return snapshot.voiceCount; }

    void paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool selected) override
    {
        if (row < 0 || row >= TelemetrySnapshot::kMaxVoices)
            return;

        drawRowBackground (g, row, selected, width, height);
        const auto& v = snapshot.voices[(size_t) row];
        auto area = juce::Rectangle<int> (width, height).reduced (6, 0);
        g.setFont (ui::LookAndFeelP5X::font (12.0f));

        static const char* states[] = { "idle", "on", "release", "sustained" };
        static const char* stages[] = { "Idle", "Attack", "Decay", "Sustain", "Release" };
        const bool idle = v.state == VoiceTelemetry::State::Idle;

        g.setColour (idle ? ui::colours::labelDim.withAlpha (0.5f) : ui::colours::text);
        g.drawText ("#" + juce::String (row + 1), area.removeFromLeft (40), juce::Justification::centredLeft);
        g.drawText (states[(int) v.state], area.removeFromLeft (80), juce::Justification::centredLeft);

        if (idle)
            return;

        g.drawText (juce::MidiMessage::getMidiNoteName (v.note, true, true, 4) + " (" + juce::String (v.note) + ")",
                    area.removeFromLeft (90), juce::Justification::centredLeft);
        g.drawText ("vel " + juce::String (v.velocity, 2), area.removeFromLeft (80), juce::Justification::centredLeft);
        g.drawText ("Osc A " + juce::String (v.oscAHz, 1) + " Hz", area.removeFromLeft (140), juce::Justification::centredLeft);
        g.drawText ("cutoff " + juce::String (juce::roundToInt (v.cutoffHz)) + " Hz", area.removeFromLeft (140),
                    juce::Justification::centredLeft);
        g.drawText (juce::String ("amp ") + stages[juce::jlimit (0, 4, (int) v.ampStage)] + " " + juce::String (v.ampLevel, 2),
                    area, juce::Justification::centredLeft);
    }

private:
    TelemetrySnapshot snapshot;
};

//==================================================================================================
DebugConsole::DebugConsole (P5XAudioProcessor& p)
    : processor (p)
{
    logModel = std::make_unique<LogModel> (*this);
    monitorModel = std::make_unique<MonitorModel> (processor);
    mapModel = std::make_unique<MapModel> (processor);
    voicesModel = std::make_unique<VoicesModel>();

    logList.setModel (logModel.get());
    monitorList.setModel (monitorModel.get());
    mapList.setModel (mapModel.get());
    voicesList.setModel (voicesModel.get());

    for (auto* list : { &logList, &monitorList, &mapList, &voicesList })
        list->setRowHeight (20);

    const auto background = ui::colours::sectionBg;
    tabs.addTab ("Log", background, &logList, false);
    tabs.addTab ("MIDI monitor", background, &monitorList, false);
    tabs.addTab ("MIDI map", background, &mapList, false);
    tabs.addTab ("Voices", background, &voicesList, false);

    // CPU card: CPU %, rate, block size, oversampling factor, xruns.
    cpuCard.setFont (ui::LookAndFeelP5X::font (12.0f));
    cpuCard.setColour (juce::Label::textColourId, ui::colours::text);
    cpuCard.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (cpuCard);
    tabs.setTabBarDepth (28);
    addAndMakeVisible (tabs);

    // Pause freezes the views; logging continues.
    pauseButton.setClickingTogglesState (true);
    clearButton.onClick = [this]
    {
        entries.clear();
        rebuildVisibleLog();
    };
    saveButton.onClick = [this] { saveLog(); };
    panicButton.onClick = [this] { processor.panic(); };

    for (auto* b : { &pauseButton, &clearButton, &saveButton, &panicButton })
        addAndMakeVisible (*b);

    const char* names[] = { "ERROR", "WARN", "INFO", "DEBUG" };

    for (size_t i = 0; i < levelChips.size(); ++i)
    {
        levelChips[i].setButtonText (names[i]);
        levelChips[i].setToggleState (true, juce::dontSendNotification);
        levelChips[i].setColour (juce::ToggleButton::textColourId, levelColour ((uint8_t) i));
        levelChips[i].onClick = [this] { rebuildVisibleLog(); };
        addAndMakeVisible (levelChips[i]);
    }

    // Show what's already in the shared history for this instance.
    setSize (1120, 560);
    timerCallback();
    startTimerHz (30);
}

DebugConsole::~DebugConsole()
{
    stopTimer();

    for (auto* list : { &logList, &monitorList, &mapList, &voicesList })
        list->setModel (nullptr);
}

void DebugConsole::paint (juce::Graphics& g)
{
    g.fillAll (ui::colours::panelBg);
}

void DebugConsole::resized()
{
    auto area = getLocalBounds().reduced (8);
    auto header = area.removeFromTop (30);

    for (auto* b : { &pauseButton, &clearButton, &saveButton, &panicButton })
    {
        b->setBounds (header.removeFromLeft (b == &saveButton ? 100 : 72));
        header.removeFromLeft (6);
    }

    header.removeFromLeft (12);

    for (auto& chip : levelChips)
        chip.setBounds (header.removeFromLeft (84));

    cpuCard.setBounds (header);

    area.removeFromTop (8);
    tabs.setBounds (area);
}

void DebugConsole::timerCallback()
{
    if (pauseButton.getToggleState())
        return;

    // Voices table and CPU card from telemetry.
    const auto t = processor.getTelemetry();
    voicesModel->update (t);
    voicesList.updateContent();
    voicesList.repaint();
    cpuCard.setText (juce::String (juce::CharPointer_UTF8 ("CPU ")) + juce::String (t.cpuPercent, 1) + "%  |  "
                         + juce::String (juce::roundToInt (t.sampleRate)) + " Hz  |  block " + juce::String (t.blockSize)
                         + "  |  " + juce::String (t.oversampling) + "x  |  xruns " + juce::String (t.xruns),
                     juce::dontSendNotification);

    // Log: this instance's entries plus global ones (instance id 0).
    std::vector<LogEntry> fresh;
    nextSequence = processor.getLogService().copyEntriesSince (nextSequence, fresh);

    bool changed = false;

    for (const auto& e : fresh)
    {
        if (e.instanceId == 0 || e.instanceId == processor.getInstanceId())
        {
            entries.push_back (e);
            changed = true;
        }
    }

    if (entries.size() > kMaxEntries)
    {
        entries.erase (entries.begin(), entries.begin() + (std::ptrdiff_t) (entries.size() - kMaxEntries));
        changed = true;
    }

    if (changed)
        rebuildVisibleLog();

    // MIDI monitor
    if (processor.getMonitorEventCount() != lastMonitorCount)
    {
        lastMonitorCount = processor.getMonitorEventCount();
        monitorModel->refresh();
        monitorList.updateContent();
        monitorList.scrollToEnsureRowIsOnscreen (monitorModel->getNumRows() - 1);
        monitorList.repaint();
    }

    // MIDI map, a few times per second
    if (--mapRefreshCountdown <= 0)
    {
        mapRefreshCountdown = 10;

        if (mapModel->refresh())
        {
            mapList.updateContent();
            mapList.repaint();
        }
    }
}

void DebugConsole::rebuildVisibleLog()
{
    visible.clear();

    for (const auto& e : entries)
        if (e.level < levelChips.size() && levelChips[e.level].getToggleState())
            visible.push_back (&e);

    logList.updateContent();
    logList.scrollToEnsureRowIsOnscreen ((int) visible.size() - 1);
    logList.repaint();
}

void DebugConsole::saveLog()
{
    chooser = std::make_unique<juce::FileChooser> ("Save log", juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                                                                   .getChildFile ("p5x-log.txt"),
                                                   "*.txt;*.log");

    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                              | juce::FileBrowserComponent::warnAboutOverwriting,
                          [this] (const juce::FileChooser& fc)
                          {
                              const auto file = fc.getResult();

                              if (file == juce::File())
                                  return;

                              juce::String text;

                              for (const auto* e : visible)
                                  text << LogFileSink::formatLine (*e) << "\r\n";

                              if (file.replaceWithText (text))
                                  P5X_LOG (Info, Ui, processor.getInstanceId(), "Log saved to %s", file.getFullPathName().toRawUTF8());
                              else
                                  P5X_LOG (Error, Ui, processor.getInstanceId(), "Couldn't save the log");
                          });
}
} // namespace p5x::debug
