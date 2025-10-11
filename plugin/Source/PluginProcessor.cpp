#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cstddef>
#include <span>

static float userValue (int32_t index, float value)
{
    float v = 0;

    switch (index)
    {
        case pYoungsModulus:
            v = 200 * exp (4.0f * (value - 0.5f));
            break;
        case pStringDensity:
            v = 7850 * exp (4.0f * (value - 0.5f));
            break;
        case pHammerMass:
            v = exp (4.0f * (value - 0.5f));
            break;
        case pStringTension:
            v = 800.0f * exp (3.0f * (value - 0.5f));
            break;
        case pStringLength:
            v = exp (2.0f * (value - 0.25f));
            break;
        case pStringRadius:
            v = exp (2.0f * (value - 0.25f));
            break;
        case pHammerCompliance:
            v = 2.0f * value;
            break;
        case pHammerSpringConstant:
            v = (2.0f * value);
            break;
        case pHammerHysteresis:
            v = exp (4.0f * (value - 0.5f));
            break;
        case pBridgeImpedance:
            v = 8000.0f * exp (12.0f * (value - 0.5f));
            break;
        case pBridgeHorizontalImpedance:
            v = 60000.0f * exp (12.0f * (value - 0.5f));
            break;
        case pVerticalHorizontalImpedance:
            v = 400.0f * exp (12.0f * (value - 0.5f));
            break;
        case pHammerPosition:
            v = 0.05f + value * 0.15f;
            break;
        case pSoundboardSize:
            v = value;
            break;
        case pStringDecay:
            v = 0.25f * exp (6.0f * (value - 0.25f));
            break;
        case pStringLopass:
            v = 5.85f * exp (6.0f * (value - 0.5f));
            break;
        case pDampedStringDecay:
            v = 8.0f * exp (6.0f * (value - 0.5f));
            break;
        case pDampedStringLopass:
            v = 25.0f * exp (6.0f * (value - 0.5f));
            break;
        case pSoundboardDecay:
            v = 20.0f * exp (4.0f * (value - 0.5f));
            break;
        case pSoundboardLopass:
            v = 20.0f * exp (4.0f * (value - 0.5f));
            break;
        case pLongitudinalGamma:
            v = 1e-2f * exp (10.0f * (value - 0.5f));
            break;
        case pLongitudinalGammaQuadratic:
            v = 1.0e-2f * exp (8.0f * (value - 0.5f));
            break;
        case pLongitudinalGammaDamped:
            v = 5e-2f * exp (10.0f * (value - 0.5f));
            break;
        case pLongitudinalGammaQuadraticDamped:
            v = 3.0e-2f * exp (8.0f * (value - 0.5f));
            break;
        case pLongitudinalMix:
            v = (value == 0.0f) ? 0.0f : 1e0f * exp (16.0f * (value - 0.5f));
            break;
        case pLongitudinalTransverseMix:
            v = (value == 0.0f) ? 0.0f : 1e0f * exp (16.0f * (value - 0.5f));
            break;
        case pVolume:
            v = 5e-3f * exp (8.0f * (value - 0.5f));
            break;
        case pMaxVelocity:
            v = 10 * exp (8.0f * (value - 0.5f));
            break;
        case pStringDetuning:
            v = 1.0f * exp (10.0f * (value - 0.5f));
            break;
        case pBridgeMass:
            v = 10.0f * exp (10.0f * (value - 0.5f));
            break;
        case pBridgeSpring:
            v = 1e5f * exp (20.0f * (value - 0.5f));
            break;
        case pDwgs4:
            v = lrintf (value);
            break;
        case pDownsample:
            v = 1 + lrintf(value);
            break;
        case pLongModes:
            v = 1 + lrintf(value);
            break;
    }
    return v;
}

//==============================================================================
PianoAudioProcessor::PianoAudioProcessor()
    : gin::Processor (false, gin::ProcessorOptions().withAdditionalCredits({"Clayton Otey"}))
{
	setLatencySamples (interalBlockSize);
	
    auto textFunction = [this] (const gin::Parameter& p, float v)
    {
        int idx = params.indexOf (&p);
        return juce::String (userValue (idx, v), 1);
    };

    params.add (addExtParam ("YoungsModulus", "Youngs Modulus", "", "GPa", { 0.0f, 1.0f }, 0.5f, {0.0f}, textFunction));
    params.add (addExtParam ("StringDensity", "String Density", "", "kg/m^3" , { 0.0f, 1.0f }, 0.5f, {0.0f}, textFunction));
    params.add (addExtParam ("HammerMass", "Hammer Mass", "", "kg" , { 0.0f, 1.0f }, 0.5f, {0.0f}, textFunction));
    params.add (addExtParam ("StringTension", "String Tension", "", "kg*m/s^2" , { 0.0f, 1.0f }, 0.5f, {0.0f}, textFunction));
    params.add (addExtParam ("StringLength", "String Length", "", "m" , { 0.0f, 1.0f }, 0.25f, {0.0f}, textFunction));
    params.add (addExtParam ("StringRadius", "String Radius", "", "m" , { 0.0f, 1.0f }, 0.25f, {0.0f}, textFunction));
    params.add (addExtParam ("HammerCompliance", "Hammer Compliance", "", "" , { 0.0f, 1.0f }, 0.5f, {0.0f}, textFunction));
    params.add (addExtParam ("HammerSpringConstant", "Hammer Spring Constant", "", "kg/s^2" , { 0.0f, 1.0f }, 0.5f, {0.0f}, textFunction));
    params.add (addExtParam ("HammerHysteresis", "Hammer Hysteresis", "", "" , { 0.0f, 1.0f }, 0.5f, {0.0f}, textFunction));
    params.add (addExtParam ("HammerPosition", "Hammer Position", "", "" , { 0.0f, 1.0f }, 0.5f, {0.0f}, textFunction));
    params.add (addExtParam ("BridgeImpedance", "Bridge Impedance", "", "" , { 0.0f, 1.0f }, 0.5f, {0.0f}, textFunction));
    params.add (addExtParam ("BridgeHorizontalImpedance", "Bridge Horizontal Impedance", "", "" , { 0.0f, 1.0f }, 0.5f, {0.0f}, textFunction));
    params.add (addExtParam ("VerticalHorizontalImpedance", "Vertical Horizontal Impedance", "", "" , { 0.0f, 1.0f }, 0.5f, {0.0f}, textFunction));
    params.add (addExtParam ("SoundboardSize", "Soundboard Size", "", "" , { 0.0f, 1.0f }, 0.0f, {0.0f}, textFunction));
    params.add (addExtParam ("StringDecay", "String Decay", "", "" , { 0.0f, 1.0f }, 0.25f, {0.0f}, textFunction));
    params.add (addExtParam ("StringLopass", "String Lopass", "", "" , { 0.0f, 1.0f }, 0.5f, {0.0f}, textFunction));
    params.add (addExtParam ("DampedStringDecay", "Damped String Decay", "", "" , { 0.0f, 1.0f }, 0.5f, {0.0f}, textFunction));
    params.add (addExtParam ("DampedStringLopass", "Damped String Lopass", "", "" , { 0.0f, 1.0f }, 0.5f, {0.0f}, textFunction));
    params.add (addExtParam ("SoundboardDecay", "Soundboard Decay", "", "" , { 0.0f, 1.0f }, 0.5f, {0.0f}, textFunction));
    params.add (addExtParam ("SoundboardLopass", "Soundboard Lopass", "", "" , { 0.0f, 1.0f }, 0.5f, {0.0f}, textFunction));
    params.add (addExtParam ("LongitudinalGamma", "Longitudinal Gamma", "", "" , { 0.0f, 1.0f }, 0.5f, {0.0f}, textFunction));
    params.add (addExtParam ("LongitudinalGammaQuadratic", "Longitudinal Gamma Quadratic", "", "" , { 0.0f, 1.0f }, 0.0f, {0.0f}, textFunction));
    params.add (addExtParam ("LongitudinalGammaDamped", "Longitudinal Gamma Damped", "", "" , { 0.0f, 1.0f }, 0.5f, {0.0f}, textFunction));
    params.add (addExtParam ("LongitudinalGammaQuadDamped", "Longitudinal Gamma Quad Damped", "", "" , { 0.0f, 1.0f }, 0.0f, {0.0f}, textFunction));
    params.add (addExtParam ("LongitudinalMix", "Longitudinal Mix", "", "" , { 0.0f, 1.0f }, 0.5f, {0.0f}, textFunction));
    params.add (addExtParam ("LongitudinalTransverseMix", "Longitudinal Transverse Mix", "", "" , { 0.0f, 1.0f }, 0.5f, {0.0f}, textFunction));
    params.add (addExtParam ("Volume", "Volume", "", "" , { 0.0f, 1.0f }, 0.5f, {0.0f}, textFunction));
    params.add (addExtParam ("MaxVelocity", "Max Velocity", "", "m/s" , { 0.0f, 1.0f }, 0.5f, {0.0f}, textFunction));
    params.add (addExtParam ("StringDetuning", "String Detuning", "", "%" , { 0.0f, 1.0f }, 0.5f, {0.0f}, textFunction));
    params.add (addExtParam ("BridgeMass", "Bridge Mass", "", "kg" , { 0.0f, 1.0f }, 0.5f, {0.0f}, textFunction));
    params.add (addExtParam ("BridgeSpring", "Bridge Spring", "", "kg/s^2" , { 0.0f, 1.0f }, 0.5f, {0.0f}, textFunction));
    params.add (addExtParam ("Dwgs4", "Dwgs4", "", "" , { 0.0f, 1.0f }, 1.0f, {0.0f}, textFunction));
    params.add (addExtParam ("Downsample", "Downsample", "", "" , { 0.0f, 1.0f }, 0.0f, {0.0f}, textFunction));
    params.add (addExtParam ("LongModes", "Long Modes", "", "", { 0.0f, 1.0f }, 0.0f, {0.0f}, textFunction));
    params.add (addExtParam ("MaxTime", "Max Time", "", "", { 0.0f, 1.0f }, 0.0f, {0.0f}, textFunction));
    params.add (addExtParam ("MultiThread", "MultiThread", "", "", { 0.0f, 1.0f }, 0.0f, {0.0f}, textFunction));

    init();
}

PianoAudioProcessor::~PianoAudioProcessor()
{
}

//==============================================================================
void PianoAudioProcessor::stateUpdated()
{
}

void PianoAudioProcessor::updateState()
{
}

//==============================================================================
void PianoAudioProcessor::reset()
{
    Processor::reset();
}

void PianoAudioProcessor::prepareToPlay (double newSampleRate, int newSamplesPerBlock)
{
    Processor::prepareToPlay (newSampleRate, newSamplesPerBlock);

	piano = std::make_unique<Piano>();
    piano->init (float (newSampleRate), interalBlockSize);
	
	fifoIn.setSize (2, newSamplesPerBlock * 2 + interalBlockSize);
	fifoOut.setSize (2, newSamplesPerBlock * 2 + interalBlockSize);
	
	fifoOut.clear();
	fifoOut.writeSilence (interalBlockSize);
}

void PianoAudioProcessor::releaseResources()
{
}

void PianoAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

	buffer.clear ();
    auto numSamples = buffer.getNumSamples();

	keyState.processNextMidiBuffer (midi, 0, numSamples, true);

	if (piano)
	{
		int idx = 0;
		for (auto p : params)
			piano->setParameter (idx++, p->getValue());
		
		fifoIn.write (buffer, midi);
		
		while (fifoIn.getNumSamplesAvailable() >= interalBlockSize)
		{
			juce::MidiBuffer workMidi;
			fifoIn.read (workBuffer, workMidi);
			
			auto ptr = (float**)workBuffer.getArrayOfWritePointers();
			piano->process (ptr, workBuffer.getNumSamples(), workMidi);
			
			fifoOut.write (workBuffer, workMidi);
		}
		
		// write the output
		midi.clear();
		fifoOut.read (buffer, midi);
	}
}

//==============================================================================
bool PianoAudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* PianoAudioProcessor::createEditor()
{
    return new PianoAudioProcessorEditor (*this);
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PianoAudioProcessor();
}
