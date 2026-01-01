#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

//==============================================================================
class PianoAudioProcessorEditor : public gin::ProcessorEditor
{
public:
    PianoAudioProcessorEditor (PianoAudioProcessor&);
    ~PianoAudioProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    PianoAudioProcessor& proc;

    juce::MidiKeyboardComponent keyboard { proc.keyState, juce::MidiKeyboardComponent::horizontalKeyboard };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PianoAudioProcessorEditor)
};
