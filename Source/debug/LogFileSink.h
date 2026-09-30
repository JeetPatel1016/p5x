// P5X — Copyright (c) 2026 Jeet Patel. Licensed under GPL-3.0-or-later.
#pragma once

#include "debug/DebugLog.h"

#include <juce_core/juce_core.h>

#include <memory>

namespace p5x::debug
{
// Daily log files `p5x-YYYY-MM-DD.log` in one folder, appended, keeping the newest `keepFiles`.
// Message thread only (10-debug-and-harness.md § Logging).
class LogFileSink
{
public:
    explicit LogFileSink (juce::File logDirectory, int keepFiles = 5);

    void write (const LogEntry& entry);
    void flush();

    juce::File getCurrentFile() const { return currentFile; }

    // "2026-09-29 16:30:01.123 WARN  #1 engine  Voice 3 stolen"
    static juce::String formatLine (const LogEntry& entry);

private:
    void openFor (const juce::String& date);
    void pruneOldFiles();

    juce::File directory;
    int keep;
    juce::String currentDate;
    juce::File currentFile;
    std::unique_ptr<juce::FileOutputStream> stream;
};
} // namespace p5x::debug
