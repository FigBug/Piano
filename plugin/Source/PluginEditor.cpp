#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <memory>

class PianoHorizontalFader : public gin::HorizontalFader
{
public:
    PianoHorizontalFader (gin::Parameter* p, bool c)
        : gin::HorizontalFader (p, c)
    {
    }

    void resized() override
    {
        juce::Rectangle<int> r = getLocalBounds();

        name.setBounds (r.removeFromLeft (100));
        value.setBounds (r.removeFromRight (40));
        fader.setBounds (r.reduced (2));
    }
};

//==============================================================================
PianoAudioProcessorEditor::PianoAudioProcessorEditor (PianoAudioProcessor& p_)
    : ProcessorEditor (p_), proc (p_)
{
    setName ("main");
    
    keyboard.setName ("keys");
    keyboard.setOpaque (false);
    keyboard.setMidiChannel (1);
    keyboard.setKeyWidth (10);
    keyboard.setAvailableRange (PIANO_MIN_NOTE, PIANO_MAX_NOTE);
    keyboard.setScrollButtonsVisible (false);
    addAndMakeVisible (keyboard);

    for (auto pp : proc.getPluginParameters())
    {
        auto pc = std::make_unique<PianoHorizontalFader> (pp, false);

        addAndMakeVisible (pc.get());
        controls.add (pc.release());
    }

    setGridSize (19, 4);
}

PianoAudioProcessorEditor::~PianoAudioProcessorEditor()
{
}

//==============================================================================
void PianoAudioProcessorEditor::paint (juce::Graphics& g)
{
    ProcessorEditor::paint (g);
}

void PianoAudioProcessorEditor::resized()
{
    ProcessorEditor::resized ();
	
	keyboard.setBounds (getGridArea (0, 3, 19, 1));
    keyboard.setKeyWidth ( 10.0f );
    auto ratio = keyboard.getWidth () / keyboard.getTotalKeyboardWidth ();
    keyboard.setKeyWidth ( 10.0f * ratio );

    auto r = getGridArea (0, 0, 19, 3);

    // faders
    auto h = 20;
    auto w = r.getWidth() / 4;
    auto rc = r.removeFromLeft (w);
    auto perCol = int (std::ceil (controls.size () / 4.0f));

    int i = 0;
    for (auto c : controls)
    {
        c->setBounds (rc.removeFromTop (h));
        i++;

        if (i == perCol)
        {
            i = 0;
            rc = r.removeFromLeft (w);
        }
    }
}
