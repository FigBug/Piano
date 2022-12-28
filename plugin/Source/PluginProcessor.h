#pragma once

#include <JuceHeader.h>
#include "qiano.h"

//==============================================================================
class PianoAudioProcessor : public gin::Processor
{
public:
    //==============================================================================
	PianoAudioProcessor();
    ~PianoAudioProcessor() override;

    void stateUpdated() override;
    void updateState() override;

    //==============================================================================
    void reset() override;
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    juce::MidiKeyboardState keyState;

    Piano piano;

    juce::Array<const gin::Parameter*> params;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PianoAudioProcessor)
};
