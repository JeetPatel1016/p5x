// P5X — Copyright (c) 2026 Jeet Patel. Licensed under GPL-3.0-or-later.
#include "ui/LookAndFeelP5X.h"

#include <P5XBinaryData.h>

namespace p5x::ui
{
LookAndFeelP5X::LookAndFeelP5X()
{
    regular = juce::Typeface::createSystemTypefaceFor (P5XBinary::KodeMonoRegular_ttf,
                                                       (size_t) P5XBinary::KodeMonoRegular_ttfSize);
    semiBold = juce::Typeface::createSystemTypefaceFor (P5XBinary::KodeMonoSemiBold_ttf,
                                                        (size_t) P5XBinary::KodeMonoSemiBold_ttfSize);

    setColour (juce::ResizableWindow::backgroundColourId, colours::panelBg);
    setColour (juce::DocumentWindow::textColourId, colours::text);
    setColour (juce::Label::textColourId, colours::label);
    setColour (juce::Slider::backgroundColourId, colours::control);
    setColour (juce::Slider::trackColourId, colours::accent.withAlpha (0.7f));
    setColour (juce::Slider::thumbColourId, colours::text);
    setColour (juce::Slider::textBoxTextColourId, colours::text);
    setColour (juce::Slider::textBoxBackgroundColourId, colours::sectionBg);
    setColour (juce::Slider::textBoxOutlineColourId, colours::sectionBorder);
    setColour (juce::ToggleButton::textColourId, colours::label);
    setColour (juce::ToggleButton::tickColourId, colours::accent);
    setColour (juce::ToggleButton::tickDisabledColourId, colours::ledOff);
    setColour (juce::TextButton::buttonColourId, colours::button);
    setColour (juce::TextButton::buttonOnColourId, colours::accent.darker (0.3f));
    setColour (juce::TextButton::textColourOffId, colours::text);
    setColour (juce::TextButton::textColourOnId, colours::topBar);
    setColour (juce::ComboBox::backgroundColourId, colours::button);
    setColour (juce::ComboBox::outlineColourId, colours::buttonBorder);
    setColour (juce::ComboBox::textColourId, colours::text);
    setColour (juce::ComboBox::arrowColourId, colours::label);
    setColour (juce::PopupMenu::backgroundColourId, colours::sectionBg);
    setColour (juce::PopupMenu::textColourId, colours::text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, colours::accent.withAlpha (0.35f));
    setColour (juce::ListBox::backgroundColourId, colours::sectionBg);
    setColour (juce::ListBox::outlineColourId, colours::sectionBorder);
    setColour (juce::TextEditor::backgroundColourId, colours::sectionBg);
    setColour (juce::TextEditor::textColourId, colours::text);
    setColour (juce::TextEditor::outlineColourId, colours::sectionBorder);
    setColour (juce::TooltipWindow::backgroundColourId, colours::topBar);
    setColour (juce::TooltipWindow::textColourId, colours::text);
    setColour (juce::TooltipWindow::outlineColourId, colours::sectionBorder);
    setColour (juce::TabbedButtonBar::tabTextColourId, colours::label);
    setColour (juce::TabbedButtonBar::frontTextColourId, colours::accent);
    setColour (juce::ScrollBar::thumbColourId, colours::controlBorder);
    setColour (juce::AlertWindow::backgroundColourId, colours::sectionBg);
    setColour (juce::AlertWindow::textColourId, colours::text);
    setColour (juce::AlertWindow::outlineColourId, colours::sectionBorder);
}

juce::Typeface::Ptr LookAndFeelP5X::getTypefaceForFont (const juce::Font& f)
{
    if (f.isBold() && semiBold != nullptr)
        return semiBold;

    if (regular != nullptr)
        return regular;

    return LookAndFeel_V4::getTypefaceForFont (f);
}

juce::Font LookAndFeelP5X::font (float height, bool bold)
{
    return juce::Font (juce::FontOptions (height, bold ? juce::Font::bold : juce::Font::plain));
}
} // namespace p5x::ui
