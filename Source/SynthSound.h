#pragma once
#include <juce_audio_utils/juce_audio_utils.h>

// SynthSound : décrit les notes que le synth peut jouer (toutes les notes MIDI)
class SynthSound : public juce::SynthesiserSound
{
public:
    bool appliesToNote    (int) override { return true; }
    bool appliesToChannel (int) override { return true; }
};
