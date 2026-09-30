// P5X — Copyright (c) 2026 Jeet Patel. Licensed under GPL-3.0-or-later.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace p5x::ui
{
// Palette tokens for code-drawn elements and the flat Debug/Settings windows (08-ui.md § Palette).
namespace colours
{
inline const juce::Colour panelBg { 0xff1a1917 };
inline const juce::Colour topBar { 0xff111010 };
inline const juce::Colour sectionBg { 0xff211f1c };
inline const juce::Colour sectionBorder { 0xff34322d };
inline const juce::Colour text { 0xffefebe2 };
inline const juce::Colour sectionTitle { 0xffd2ccc0 };
inline const juce::Colour label { 0xffb3ada2 };
inline const juce::Colour labelDim { 0xffa39d92 };
inline const juce::Colour control { 0xff2d2b28 };
inline const juce::Colour controlBorder { 0xff4a4640 };
inline const juce::Colour button { 0xff2a2824 };
inline const juce::Colour buttonBorder { 0xff403d37 };
inline const juce::Colour ledOff { 0xff45423b };
inline const juce::Colour accent { 0xfff0a93b };
inline const juce::Colour displayBg { 0xff1c140a };

// Debug console log levels (10-debug-and-harness.md § Debug console window).
inline const juce::Colour logInfo { 0xff8fb7e0 };
inline const juce::Colour logError { 0xffe06c5a };
} // namespace colours

// Kode Mono everywhere (08-ui.md § Tech): the only typeface in P5X.
class LookAndFeelP5X : public juce::LookAndFeel_V4
{
public:
    LookAndFeelP5X();

    juce::Typeface::Ptr getTypefaceForFont (const juce::Font& font) override;

    static juce::Font font (float height, bool bold = false);

private:
    juce::Typeface::Ptr regular, semiBold;
};
} // namespace p5x::ui
