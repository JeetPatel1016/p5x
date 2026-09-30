// P5X — Copyright (c) 2026 Jeet Patel. Licensed under GPL-3.0-or-later.
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <memory>
#include <vector>

class P5XAudioProcessor;

namespace p5x::ui
{
// Milestones 1–4 only (08-ui.md § Placeholder editor): a plain grid with one control per parameter,
// generated from the parameter groups, with right-click MIDI Learn exactly as in 07-midi.md.
// Deleted in milestone 5.
class PlaceholderPanel : public juce::Component
{
public:
    explicit PlaceholderPanel (P5XAudioProcessor& processor);
    ~PlaceholderPanel() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

    // Called at 30 Hz by the editor: repaints the waiting control's pulsing learn ring.
    void tick();

    static constexpr int kPreferredHeight = 670;

private:
    class ParamControl;

    struct Section
    {
        juce::String title;
        std::vector<ParamControl*> controls;
        juce::Rectangle<int> bounds;
    };

    P5XAudioProcessor& processor;
    std::vector<std::unique_ptr<ParamControl>> controls;
    std::vector<Section> sections;
    int lastWaiting = -1;
};
} // namespace p5x::ui
