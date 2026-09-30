// P5X — Copyright (c) 2026 Jeet Patel. Licensed under GPL-3.0-or-later.
#pragma once

#include "debug/DebugLog.h"
#include "debug/Telemetry.h"
#include "midi/MidiMonitor.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <vector>

class P5XAudioProcessor;

namespace p5x::debug
{
// Debug console (10-debug-and-harness.md): log, MIDI monitor, MIDI map, Voices table and CPU card.
// The scope arrives in milestone 3, Dump state in 6.
class DebugConsole : public juce::Component, private juce::Timer
{
public:
    explicit DebugConsole (P5XAudioProcessor& processor);
    ~DebugConsole() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    class LogModel;
    class MonitorModel;
    class MapModel;
    class VoicesModel;

    void timerCallback() override;
    void rebuildVisibleLog();
    void saveLog();

    P5XAudioProcessor& processor;

    juce::TextButton pauseButton { "Pause" }, clearButton { "Clear" }, saveButton { "Save log..." },
        panicButton { "Panic" };
    std::array<juce::ToggleButton, 4> levelChips; // ERROR, WARN, INFO, DEBUG

    juce::TabbedComponent tabs { juce::TabbedButtonBar::TabsAtTop };
    std::unique_ptr<LogModel> logModel;
    std::unique_ptr<MonitorModel> monitorModel;
    std::unique_ptr<MapModel> mapModel;
    std::unique_ptr<VoicesModel> voicesModel;
    juce::ListBox logList, monitorList, mapList, voicesList;
    juce::Label cpuCard;

    std::vector<LogEntry> entries;        // this instance's entries plus global ones
    std::vector<const LogEntry*> visible; // after the level filter
    uint64_t nextSequence = 0;
    uint64_t lastMonitorCount = 0;
    int mapRefreshCountdown = 0;

    std::unique_ptr<juce::FileChooser> chooser;
};
} // namespace p5x::debug
