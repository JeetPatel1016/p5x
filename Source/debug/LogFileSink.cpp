// P5X — Copyright (c) 2026 Jeet Patel. Licensed under GPL-3.0-or-later.
#include "debug/LogFileSink.h"

namespace p5x::debug
{
LogFileSink::LogFileSink (juce::File logDirectory, int keepFiles)
    : directory (std::move (logDirectory)), keep (juce::jmax (1, keepFiles))
{
}

juce::String LogFileSink::formatLine (const LogEntry& entry)
{
    const juce::Time time ((juce::int64) entry.timeMs);
    const auto level = juce::String (levelName ((Level) entry.level)).paddedRight (' ', 5);
    const auto source = juce::String (sourceName ((Source) entry.source)).paddedRight (' ', 10);

    return time.formatted ("%Y-%m-%d %H:%M:%S.") + juce::String (time.getMilliseconds()).paddedLeft ('0', 3)
         + " " + level + " #" + juce::String (entry.instanceId) + " " + source + " "
         + juce::String::fromUTF8 (entry.text);
}

void LogFileSink::write (const LogEntry& entry)
{
    const auto date = juce::Time ((juce::int64) entry.timeMs).formatted ("%Y-%m-%d");

    if (date != currentDate || stream == nullptr)
        openFor (date);

    if (stream != nullptr)
        stream->writeText (formatLine (entry) + "\r\n", false, false, nullptr);
}

void LogFileSink::flush()
{
    if (stream != nullptr)
        stream->flush();
}

void LogFileSink::openFor (const juce::String& date)
{
    stream.reset();
    currentDate = date;

    if (! directory.createDirectory())
        return;

    currentFile = directory.getChildFile ("p5x-" + date + ".log");
    auto newStream = std::make_unique<juce::FileOutputStream> (currentFile); // appends

    if (newStream->openedOk())
        stream = std::move (newStream);

    pruneOldFiles();
}

void LogFileSink::pruneOldFiles()
{
    auto files = directory.findChildFiles (juce::File::findFiles, false, "p5x-*.log");

    // Names sort by date; delete the oldest beyond the limit.
    std::sort (files.begin(), files.end(), [] (const juce::File& a, const juce::File& b)
               { return a.getFileName() < b.getFileName(); });

    for (int i = 0; i < files.size() - keep; ++i)
        if (files[i] != currentFile)
            files[i].deleteFile();
}
} // namespace p5x::debug
