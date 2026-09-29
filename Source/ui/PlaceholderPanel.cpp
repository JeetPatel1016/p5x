// P5X — Copyright (c) 2026 Jeet Patel. Licensed under GPL-3.0-or-later.
#include "ui/PlaceholderPanel.h"
#include "PluginProcessor.h"
#include "ui/LookAndFeelP5X.h"

#include <cmath>

namespace p5x::ui
{
namespace
{
const juce::String kDot { juce::CharPointer_UTF8 (" \xc2\xb7 ") };

constexpr int kRowHeight = 28;
constexpr int kTitleHeight = 26;
constexpr int kSectionGap = 10;
constexpr int kColumns = 3;

// Wraps a JUCE widget so a right-click opens the MIDI Learn menu instead of reaching the widget,
// and the tooltip is computed live.
template <typename Base>
class Learnable : public Base
{
public:
    std::function<void()> onLearnMenu;
    std::function<juce::String()> tooltipText;

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu())
        {
            if (onLearnMenu)
                onLearnMenu();

            return;
        }

        Base::mouseDown (e);
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (! e.mods.isPopupMenu())
            Base::mouseUp (e);
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (! e.mods.isPopupMenu())
            Base::mouseDrag (e);
    }

    juce::String getTooltip() override { return tooltipText ? tooltipText() : Base::getTooltip(); }
};
} // namespace

//==================================================================================================
class PlaceholderPanel::ParamControl : public juce::Component
{
public:
    ParamControl (P5XAudioProcessor& p, int index)
        : processor (p), paramIndex (index), param (*p.getParameterByIndex (index))
    {
        name.setText (param.getName (64), juce::dontSendNotification);
        name.setFont (LookAndFeelP5X::font (12.0f));
        name.setColour (juce::Label::textColourId, colours::label);
        name.setInterceptsMouseClicks (false, false);
        addAndMakeVisible (name);

        if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (&param))
        {
            auto box = std::make_unique<Learnable<juce::ComboBox>>();
            box->addItemList (choice->choices, 1);
            comboAttachment = std::make_unique<juce::ComboBoxParameterAttachment> (*choice, *box);
            setUp (*box);
            widget = std::move (box);
        }
        else if (auto* toggle = dynamic_cast<juce::AudioParameterBool*> (&param))
        {
            auto button = std::make_unique<Learnable<juce::ToggleButton>>();
            buttonAttachment = std::make_unique<juce::ButtonParameterAttachment> (*toggle, *button);
            setUp (*button);
            widget = std::move (button);
        }
        else
        {
            auto slider = std::make_unique<Learnable<juce::Slider>> ();
            slider->setSliderStyle (juce::Slider::LinearHorizontal);
            slider->setTextBoxStyle (juce::Slider::TextBoxRight, false, 64, 20);
            slider->setDoubleClickReturnValue (true, param.convertFrom0to1 (param.getDefaultValue()));
            sliderAttachment = std::make_unique<juce::SliderParameterAttachment> (param, *slider);
            setUp (*slider);
            widget = std::move (slider);
        }

        addAndMakeVisible (*widget);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (6, 3);
        name.setBounds (area.removeFromLeft (150));
        widget->setBounds (area);
    }

    void paint (juce::Graphics& g) override
    {
        // Learn state: 2 px accent ring around the waiting control, pulsing 1.0 ↔ 0.35 at 1 Hz (08-ui.md).
        if (processor.getMidiLearn().getWaitingParam() != paramIndex)
            return;

        const double t = juce::Time::getMillisecondCounterHiRes() * 0.001;
        const float alpha = 0.35f + 0.65f * (0.5f + 0.5f * (float) std::cos (juce::MathConstants<double>::twoPi * t));
        g.setColour (colours::accent.withAlpha (alpha));
        g.drawRoundedRectangle (widget->getBounds().toFloat().expanded (3.0f), 4.0f, 2.0f);
    }

private:
    template <typename Widget>
    void setUp (Learnable<Widget>& w)
    {
        w.setTitle (param.getName (64)); // accessible title (08-ui.md § Accessibility)
        w.onLearnMenu = [this] { showLearnMenu(); };
        w.tooltipText = [this] { return tooltip(); };
    }

    // "Name · value · CC n" in real units (08-ui.md § Components; DECISIONS.md).
    juce::String tooltip() const
    {
        auto text = param.getName (64) + kDot + param.getCurrentValueAsText();

        if (param.getLabel().isNotEmpty())
            text << " " << param.getLabel();

        if (const int cc = processor.getMidiLearn().getCcForParam (paramIndex); cc >= 0)
            text << kDot << "CC " << cc;

        return text;
    }

    void showLearnMenu()
    {
        auto& learn = processor.getMidiLearn();
        const int cc = learn.getCcForParam (paramIndex);
        const bool waiting = learn.getWaitingParam() == paramIndex;

        juce::PopupMenu menu;
        menu.addItem (1, "Learn MIDI CC");

        if (cc >= 0)
            menu.addItem (2, "Remove MIDI CC " + juce::String (cc));

        if (waiting)
            menu.addItem (3, "Cancel learn");

        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (widget.get()),
                            [safe = juce::Component::SafePointer<ParamControl> (this), cc] (int result)
                            {
                                if (safe != nullptr)
                                    safe->handleMenuResult (result, cc);
                            });
    }

    void handleMenuResult (int result, int cc)
    {
        auto& learn = processor.getMidiLearn();
        const auto id = processor.getInstanceId();

        switch (result)
        {
            case 1:
                learn.startLearn (paramIndex);
                P5X_LOG (Info, Learn, id, "Waiting for CC: %s", param.getName (64).toRawUTF8());
                break;

            case 2:
                learn.removeMappingForParam (paramIndex);
                P5X_LOG (Info, Learn, id, "Removed CC %d from %s", cc, param.getName (64).toRawUTF8());
                break;

            case 3:
                learn.cancelLearn();
                P5X_LOG (Info, Learn, id, "Learn cancelled");
                break;

            default:
                break;
        }

        if (auto* parent = getParentComponent())
            parent->repaint();
    }

    P5XAudioProcessor& processor;
    const int paramIndex;
    juce::RangedAudioParameter& param;
    juce::Label name;
    std::unique_ptr<juce::Component> widget;
    std::unique_ptr<juce::SliderParameterAttachment> sliderAttachment;
    std::unique_ptr<juce::ButtonParameterAttachment> buttonAttachment;
    std::unique_ptr<juce::ComboBoxParameterAttachment> comboAttachment;

    friend class PlaceholderPanel;
};

//==================================================================================================
PlaceholderPanel::PlaceholderPanel (P5XAudioProcessor& p)
    : processor (p)
{
    // One section per parameter group, in layout order (01-parameters.md).
    for (const auto* group : processor.getParameterTree().getSubgroups (false))
    {
        Section section;
        section.title = group->getName().toUpperCase();

        for (const auto* parameter : group->getParameters (false))
        {
            const int index = parameter->getParameterIndex();
            controls.push_back (std::make_unique<ParamControl> (processor, index));
            addAndMakeVisible (*controls.back());
            section.controls.push_back (controls.back().get());
        }

        sections.push_back (std::move (section));
    }
}

PlaceholderPanel::~PlaceholderPanel() = default;

void PlaceholderPanel::paint (juce::Graphics& g)
{
    g.fillAll (colours::panelBg);

    for (const auto& section : sections)
    {
        g.setColour (colours::sectionBg);
        g.fillRoundedRectangle (section.bounds.toFloat(), 6.0f);
        g.setColour (colours::sectionBorder);
        g.drawRoundedRectangle (section.bounds.toFloat().reduced (0.5f), 6.0f, 1.0f);

        g.setColour (colours::sectionTitle);
        g.setFont (LookAndFeelP5X::font (13.0f, true));
        g.drawText (section.title, section.bounds.withHeight (kTitleHeight).reduced (10, 0),
                    juce::Justification::centredLeft);
    }
}

void PlaceholderPanel::resized()
{
    // Fill columns in order, starting a new column once the current one holds about a third of the rows.
    const int totalRows = (int) controls.size();
    const int rowsPerColumn = (totalRows + kColumns - 1) / kColumns;
    const int columnWidth = (getWidth() - kSectionGap * (kColumns + 1)) / kColumns;

    int column = 0, rowsInColumn = 0, y = kSectionGap;

    for (auto& section : sections)
    {
        const int rows = (int) section.controls.size();

        if (rowsInColumn > 0 && rowsInColumn + rows > rowsPerColumn + 1 && column < kColumns - 1)
        {
            ++column;
            rowsInColumn = 0;
            y = kSectionGap;
        }

        const int x = kSectionGap + column * (columnWidth + kSectionGap);
        const int height = kTitleHeight + rows * kRowHeight + 6;
        section.bounds = { x, y, columnWidth, height };

        int rowY = y + kTitleHeight;

        for (auto* control : section.controls)
        {
            control->setBounds (x + 4, rowY, columnWidth - 8, kRowHeight);
            rowY += kRowHeight;
        }

        y += height + kSectionGap;
        rowsInColumn += rows;
    }
}

void PlaceholderPanel::tick()
{
    const int waiting = processor.getMidiLearn().getWaitingParam();

    if (waiting >= 0 || lastWaiting >= 0)
        for (auto& control : controls)
            if (control->paramIndex == waiting || control->paramIndex == lastWaiting)
                control->repaint();

    lastWaiting = waiting;
}
} // namespace p5x::ui
