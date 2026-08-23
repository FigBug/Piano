#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <mutex>

// If the shared CrashReporter is installed, launch it once per process (on the
// first plugin instance) so it can scan and upload any crash from last session.
static void launchCrashReporterOnce()
{
    static std::once_flag flag;
    std::call_once (flag, []
    {
       #if JUCE_MAC
        juce::File app ("/Library/Application Support/Rabien Software/Crash Reporter/CrashReporter.app");
       #elif JUCE_WINDOWS
        auto app = juce::File::getSpecialLocation (juce::File::globalApplicationsDirectory)
                       .getChildFile ("Rabien Software").getChildFile ("Crash Reporter").getChildFile ("CrashReporter.exe");
       #else
        juce::File app;
       #endif

        if (app.exists())
            juce::Process::openDocument (app.getFullPathName(), {});
    });
}

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

// Inverse of v = scale * exp (rate * (value - offset))
static float invExp (float v, float scale, float rate, float offset)
{
    if (v <= 0.0f)
        return 0.0f;

    return std::log (v / scale) / rate + offset;
}

static float userValueInverse (int32_t index, float v)
{
    float value = 0;

    switch (index)
    {
        case pYoungsModulus:
            value = invExp (v, 200.0f, 4.0f, 0.5f);
            break;
        case pStringDensity:
            value = invExp (v, 7850.0f, 4.0f, 0.5f);
            break;
        case pHammerMass:
            value = invExp (v, 1.0f, 4.0f, 0.5f);
            break;
        case pStringTension:
            value = invExp (v, 800.0f, 3.0f, 0.5f);
            break;
        case pStringLength:
            value = invExp (v, 1.0f, 2.0f, 0.25f);
            break;
        case pStringRadius:
            value = invExp (v, 1.0f, 2.0f, 0.25f);
            break;
        case pHammerCompliance:
            value = v / 2.0f;
            break;
        case pHammerSpringConstant:
            value = v / 2.0f;
            break;
        case pHammerHysteresis:
            value = invExp (v, 1.0f, 4.0f, 0.5f);
            break;
        case pBridgeImpedance:
            value = invExp (v, 8000.0f, 12.0f, 0.5f);
            break;
        case pBridgeHorizontalImpedance:
            value = invExp (v, 60000.0f, 12.0f, 0.5f);
            break;
        case pVerticalHorizontalImpedance:
            value = invExp (v, 400.0f, 12.0f, 0.5f);
            break;
        case pHammerPosition:
            value = (v - 0.05f) / 0.15f;
            break;
        case pSoundboardSize:
            value = v;
            break;
        case pStringDecay:
            value = invExp (v, 0.25f, 6.0f, 0.25f);
            break;
        case pStringLopass:
            value = invExp (v, 5.85f, 6.0f, 0.5f);
            break;
        case pDampedStringDecay:
            value = invExp (v, 8.0f, 6.0f, 0.5f);
            break;
        case pDampedStringLopass:
            value = invExp (v, 25.0f, 6.0f, 0.5f);
            break;
        case pSoundboardDecay:
            value = invExp (v, 20.0f, 4.0f, 0.5f);
            break;
        case pSoundboardLopass:
            value = invExp (v, 20.0f, 4.0f, 0.5f);
            break;
        case pLongitudinalGamma:
            value = invExp (v, 1e-2f, 10.0f, 0.5f);
            break;
        case pLongitudinalGammaQuadratic:
            value = invExp (v, 1.0e-2f, 8.0f, 0.5f);
            break;
        case pLongitudinalGammaDamped:
            value = invExp (v, 5e-2f, 10.0f, 0.5f);
            break;
        case pLongitudinalGammaQuadraticDamped:
            value = invExp (v, 3.0e-2f, 8.0f, 0.5f);
            break;
        case pLongitudinalMix:
            value = (v == 0.0f) ? 0.0f : invExp (v, 1e0f, 16.0f, 0.5f);
            break;
        case pLongitudinalTransverseMix:
            value = (v == 0.0f) ? 0.0f : invExp (v, 1e0f, 16.0f, 0.5f);
            break;
        case pVolume:
            value = invExp (v, 5e-3f, 8.0f, 0.5f);
            break;
        case pMaxVelocity:
            value = invExp (v, 10.0f, 8.0f, 0.5f);
            break;
        case pStringDetuning:
            value = invExp (v, 1.0f, 10.0f, 0.5f);
            break;
        case pBridgeMass:
            value = invExp (v, 10.0f, 10.0f, 0.5f);
            break;
        case pBridgeSpring:
            value = invExp (v, 1e5f, 20.0f, 0.5f);
            break;
        case pDwgs4:
            value = v;
            break;
        case pDownsample:
            value = v - 1.0f;
            break;
        case pLongModes:
            value = v - 1.0f;
            break;
    }
    return juce::jlimit (0.0f, 1.0f, value);
}

//==============================================================================
static gin::ProcessorOptions createProcessorOptions()
{
    return gin::ProcessorOptions()
        .withAdditionalCredits ({"Clayton Otey"})
        .withMidiLearn();
}

PianoAudioProcessor::PianoAudioProcessor()
    : gin::Processor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo()), false, createProcessorOptions())
{
    launchCrashReporterOnce();

	setLatencySamples (interalBlockSize);
	
    auto conversionFunction = [this] (const gin::Parameter& p, const std::variant<float, juce::String>& in) -> std::variant<float, juce::String>
    {
        int idx = params.indexOf (&p);

        if (auto v = std::get_if<float> (&in))
            return juce::String (userValue (idx, *v), 1);

        return userValueInverse (idx, std::get<juce::String> (in).getFloatValue());
    };

    params.add (addExtParam ("YoungsModulus", "Youngs Modulus", "", "GPa", { 0.0f, 1.0f }, 0.5f, {0.0f}, conversionFunction));
    params.add (addExtParam ("StringDensity", "String Density", "", "kg/m^3" , { 0.0f, 1.0f }, 0.5f, {0.0f}, conversionFunction));
    params.add (addExtParam ("HammerMass", "Hammer Mass", "", "kg" , { 0.0f, 1.0f }, 0.5f, {0.0f}, conversionFunction));
    params.add (addExtParam ("StringTension", "String Tension", "", "kg*m/s^2" , { 0.0f, 1.0f }, 0.5f, {0.0f}, conversionFunction));
    params.add (addExtParam ("StringLength", "String Length", "", "m" , { 0.0f, 1.0f }, 0.25f, {0.0f}, conversionFunction));
    params.add (addExtParam ("StringRadius", "String Radius", "", "m" , { 0.0f, 1.0f }, 0.25f, {0.0f}, conversionFunction));
    params.add (addExtParam ("HammerCompliance", "Hammer Compliance", "", "" , { 0.0f, 1.0f }, 0.5f, {0.0f}, conversionFunction));
    params.add (addExtParam ("HammerSpringConstant", "Hammer Spring Constant", "", "kg/s^2" , { 0.0f, 1.0f }, 0.5f, {0.0f}, conversionFunction));
    params.add (addExtParam ("HammerHysteresis", "Hammer Hysteresis", "", "" , { 0.0f, 1.0f }, 0.5f, {0.0f}, conversionFunction));
    params.add (addExtParam ("HammerPosition", "Hammer Position", "", "" , { 0.0f, 1.0f }, 0.5f, {0.0f}, conversionFunction));
    params.add (addExtParam ("BridgeImpedance", "Bridge Impedance", "", "" , { 0.0f, 1.0f }, 0.5f, {0.0f}, conversionFunction));
    params.add (addExtParam ("BridgeHorizontalImpedance", "Bridge Horizontal Impedance", "", "" , { 0.0f, 1.0f }, 0.5f, {0.0f}, conversionFunction));
    params.add (addExtParam ("VerticalHorizontalImpedance", "Vertical Horizontal Impedance", "", "" , { 0.0f, 1.0f }, 0.5f, {0.0f}, conversionFunction));
    params.add (addExtParam ("SoundboardSize", "Soundboard Size", "", "" , { 0.0f, 1.0f }, 0.0f, {0.0f}, conversionFunction));
    params.add (addExtParam ("StringDecay", "String Decay", "", "" , { 0.0f, 1.0f }, 0.25f, {0.0f}, conversionFunction));
    params.add (addExtParam ("StringLopass", "String Lopass", "", "" , { 0.0f, 1.0f }, 0.5f, {0.0f}, conversionFunction));
    params.add (addExtParam ("DampedStringDecay", "Damped String Decay", "", "" , { 0.0f, 1.0f }, 0.5f, {0.0f}, conversionFunction));
    params.add (addExtParam ("DampedStringLopass", "Damped String Lopass", "", "" , { 0.0f, 1.0f }, 0.5f, {0.0f}, conversionFunction));
    params.add (addExtParam ("SoundboardDecay", "Soundboard Decay", "", "" , { 0.0f, 1.0f }, 0.5f, {0.0f}, conversionFunction));
    params.add (addExtParam ("SoundboardLopass", "Soundboard Lopass", "", "" , { 0.0f, 1.0f }, 0.5f, {0.0f}, conversionFunction));
    params.add (addExtParam ("LongitudinalGamma", "Longitudinal Gamma", "", "" , { 0.0f, 1.0f }, 0.5f, {0.0f}, conversionFunction));
    params.add (addExtParam ("LongitudinalGammaQuadratic", "Longitudinal Gamma Quadratic", "", "" , { 0.0f, 1.0f }, 0.0f, {0.0f}, conversionFunction));
    params.add (addExtParam ("LongitudinalGammaDamped", "Longitudinal Gamma Damped", "", "" , { 0.0f, 1.0f }, 0.5f, {0.0f}, conversionFunction));
    params.add (addExtParam ("LongitudinalGammaQuadDamped", "Longitudinal Gamma Quadratic Damped", "", "" , { 0.0f, 1.0f }, 0.0f, {0.0f}, conversionFunction));
    params.add (addExtParam ("LongitudinalMix", "Longitudinal Mix", "", "" , { 0.0f, 1.0f }, 0.5f, {0.0f}, conversionFunction));
    params.add (addExtParam ("LongitudinalTransverseMix", "Longitudinal Transverse Mix", "", "" , { 0.0f, 1.0f }, 0.5f, {0.0f}, conversionFunction));
    params.add (addExtParam ("Volume", "Volume", "", "" , { 0.0f, 1.0f }, 0.5f, {0.0f}, conversionFunction));
    params.add (addExtParam ("MaxVelocity", "Max Velocity", "", "m/s" , { 0.0f, 1.0f }, 0.5f, {0.0f}, conversionFunction));
    params.add (addExtParam ("StringDetuning", "String Detuning", "", "%" , { 0.0f, 1.0f }, 0.5f, {0.0f}, conversionFunction));
    params.add (addExtParam ("BridgeMass", "Bridge Mass", "", "kg" , { 0.0f, 1.0f }, 0.5f, {0.0f}, conversionFunction));
    params.add (addExtParam ("BridgeSpring", "Bridge Spring", "", "kg/s^2" , { 0.0f, 1.0f }, 0.5f, {0.0f}, conversionFunction));
    params.add (addExtParam ("Dwgs4", "Dwgs4", "", "" , { 0.0f, 1.0f }, 1.0f, {0.0f}, conversionFunction));
    params.add (addExtParam ("Downsample", "Downsample", "", "" , { 0.0f, 1.0f }, 0.0f, {0.0f}, conversionFunction));
    params.add (addExtParam ("LongModes", "Long Modes", "", "", { 0.0f, 1.0f }, 0.0f, {0.0f}, conversionFunction));

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

    if (midiLearn)
        midiLearn->processBlock (midi, buffer.getNumSamples());

	buffer.clear ();
    auto numSamples = buffer.getNumSamples();

	keyState.processNextMidiBuffer (midi, 0, numSamples, true);

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
