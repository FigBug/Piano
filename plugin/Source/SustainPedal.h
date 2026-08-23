#pragma once

#include <JuceHeader.h>

//==============================================================================
/** MIDI filter implementing a sustain pedal (CC64). While the pedal is down,
    note offs are held back; they are emitted when the pedal is released. */
class SustainPedal
{
public:
    SustainPedal()
    {
        heldNoteOffs.ensureStorageAllocated (128);
    }

    void reset()
    {
        pedalDown = false;
        heldNoteOffs.clearQuick();
    }

    void processMidi (juce::MidiBuffer& midi)
    {
        filtered.clear();

        for (auto meta : midi)
        {
            auto m = meta.getMessage();
            auto pos = meta.samplePosition;

            if (m.isSustainPedalOn())
            {
                pedalDown = true;
                filtered.addEvent (m, pos);
            }
            else if (m.isSustainPedalOff())
            {
                pedalDown = false;
                for (auto& off : heldNoteOffs)
                    filtered.addEvent (off, pos);
                heldNoteOffs.clearQuick();
                filtered.addEvent (m, pos);
            }
            else if (pedalDown && m.isNoteOff())
            {
                heldNoteOffs.add (m);
            }
            else
            {
                if (pedalDown && m.isNoteOn())
                {
                    // Key is held again, so drop any pending note off for it
                    for (int i = heldNoteOffs.size(); --i >= 0;)
                        if (heldNoteOffs.getReference (i).getNoteNumber() == m.getNoteNumber()
                             && heldNoteOffs.getReference (i).getChannel() == m.getChannel())
                            heldNoteOffs.remove (i);
                }
                else if (m.isAllNotesOff() || m.isAllSoundOff())
                {
                    heldNoteOffs.clearQuick();
                }

                filtered.addEvent (m, pos);
            }
        }

        midi.swapWith (filtered);
    }

private:
    bool pedalDown = false;
    juce::Array<juce::MidiMessage> heldNoteOffs;
    juce::MidiBuffer filtered;
};
