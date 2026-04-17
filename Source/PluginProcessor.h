#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>
#include "SynthVoice.h"
#include "SynthSound.h"
#include "PresetManager.h"

// ============================================================
//  ModConnection — une connexion LFO → paramètre
// ============================================================
struct ModConnection
{
    int          lfoIndex;
    juce::String paramID;
    float        amount;
    bool operator== (const ModConnection& o) const
    { return lfoIndex == o.lfoIndex && paramID == o.paramID; }
};

// ============================================================
//  LfoPoint — point de contrôle pour la forme d'onde LFO
// ============================================================
struct LfoPoint
{
    float x     { 0.0f };  // 0..1 (phase)
    float y     { 0.0f };  // -1..+1 (amplitude)
    float curve { 0.0f };  // -1..+1 : 0 = linear, >0 = bow toward end, <0 = bow toward start
};

// ============================================================
//  PluginProcessor
// ============================================================
class NovaSynthProcessor : public juce::AudioProcessor
{
public:
    NovaSynthProcessor();
    ~NovaSynthProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Octopus"; }
    bool acceptsMidi()  const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int  getNumPrograms()    override { return 1; }
    int  getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& dest) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    PresetManager presetManager;

    // ---- Système de modulation ----
    void  addModConnection    (int lfoIdx, const juce::String& paramID, float amount = 0.5f);
    void  removeModConnection (int lfoIdx, const juce::String& paramID);
    void  clearModSource      (int lfoIdx);   // supprime TOUTES les connexions de ce source
    void  setModAmount        (int lfoIdx, const juce::String& paramID, float amount);
    float getModAmount        (int lfoIdx, const juce::String& paramID) const;
    bool  hasModConnection    (const juce::String& paramID) const;
    bool  hasModConnection    (int lfoIdx, const juce::String& paramID) const;
    float getLfoValue (int idx) const;
    float getLfoPhase (int idx) const;
    void  resetLfoPhase (int idx);   // resets qLfo[idx].phase = 0

    std::atomic<float> lfo1Value{0.f}, lfo2Value{0.f}, lfo3Value{0.f}, lfo4Value{0.f};
    std::atomic<float> lfo1Phase{0.f}, lfo2Phase{0.f}, lfo3Phase{0.f}, lfo4Phase{0.f};

    // ENV2 (index 4) and ENV3 (index 5) values for mod system
    std::atomic<float> env2Value{0.f}, env3Value{0.f};

    // Gain reduction per band (written by audio thread, read by UI for meters)
    // 1.0 = no reduction, 0.0 = full reduction
    std::atomic<float> bandGainReduction[3] {};

    // Per-band input signal level for compressor visual (RMS, linear 0..1)
    std::atomic<float> bandSignalLevel[3] {};

    // Clavier d'ordinateur (thread-safe, alimenté depuis l'UI)
    juce::MidiKeyboardState keyboardState;

    juce::Array<ModConnection> modConnections;

    // ---- Points LFO custom (thread-safe) ----
    void setLfoPoints (int lfoIdx, const std::vector<LfoPoint>& pts);
    std::vector<LfoPoint> getLfoPoints (int lfoIdx) const;

private:
    juce::Synthesiser synth;
    static constexpr int NUM_VOICES = 8;

    // Multiband compressor (OTT-style)
    // bandComps kept for future use / legacy prepare; main processing uses custom algorithm
    std::array<juce::dsp::Compressor<float>, 3>              bandComps;
    std::array<juce::dsp::LinkwitzRileyFilter<float>, 2>     crossoverLP;
    std::array<juce::dsp::LinkwitzRileyFilter<float>, 2>     crossoverHP;
    juce::dsp::Gain<float>                                   compInGain, compOutGain;
    float bandThresh[3] { -20.f, -20.f, -20.f };
    float bandRatio [3] {   4.f,   4.f,   4.f };

    // Per-band, per-channel gain-smoothing envelope state for the OTT algorithm
    // [band][channel]:  0 = low, 1 = mid, 2 = high
    // State stored in dB (0 dB = unity). Log-domain smoothing for clean GR.
    float compEnvDown[3][2] { {0.f,0.f}, {0.f,0.f}, {0.f,0.f} };
    float compEnvUp  [3][2] { {0.f,0.f}, {0.f,0.f}, {0.f,0.f} };

    // Overdrive
    juce::dsp::Oversampling<float> driveOversampler {
        2, 2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR };
    float driveDcX[2] {0.f, 0.f};
    float driveDcY[2] {0.f, 0.f};
    float driveHoldSample[2] {0.f, 0.f};
    float driveHoldPhase[2]  {0.f, 0.f};

    // Modulation FX
    juce::dsp::Chorus<float>                             chorusFx;
    juce::dsp::Phaser<float>                             phaserFx;

    // Delay (stereo, with ping-pong)
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear>
        delayLineL { 192000 }, delayLineR { 192000 };
    float delayFbStateL { 0.f }, delayFbStateR { 0.f };

    // Reverb
    juce::dsp::Reverb                                    reverbFx;

    void updateVoiceParameters();
    float getModdedParam (const juce::String& id);

    // LFO interne avec points custom et modes de déclenchement
    struct QuickLFO
    {
        float phase { 0.0f };
        bool  done  { false };   // pour mode One-shot

        // Avance la phase de numSamples et retourne la valeur
        float advance (float rate, float sr, int numSamples, int mode,
                       int wave, const std::vector<LfoPoint>& pts)
        {
            if (mode == 2 && done) return 0.0f;   // One-shot terminé

            phase += rate * (float)numSamples / sr;
            if (phase >= 1.f)
            {
                if (mode == 2) { done = true; phase = 1.f; return 0.f; }
                phase -= std::floor (phase);
            }
            return evaluate (wave, pts);
        }

        void reset() { phase = 0.0f; done = false; }

        float evaluate (int wave, const std::vector<LfoPoint>& pts) const
        {
            if (!pts.empty()) return interpolateCustom (pts, phase);
            switch (wave) {
                case 0: return std::sin (phase * juce::MathConstants<float>::twoPi);
                case 1: return (phase < .5f) ? 4*phase-1 : 3-4*phase;
                case 2: return 2*phase-1;
                case 3: return (phase < .5f) ? 1.f : -1.f;
                default: return 0.f;
            }
        }

        float advanceWithShape (float rate, float sr, int numSamples, int mode, float shape)
        {
            if (mode == 2 && done) return 0.f;
            phase += rate * (float)numSamples / sr;
            if (phase >= 1.f) {
                if (mode == 2) { done = true; phase = 1.f; return 0.f; }
                phase -= std::floor (phase);
            }
            return evaluateWithShape (shape);
        }

        float evaluateWithShape (float shape) const
        {
            int   w1 = juce::jlimit (0, 3, (int)shape);
            int   w2 = juce::jlimit (0, 3, w1 + 1);
            float t  = shape - (float)w1;
            return evaluate (w1, {}) * (1.f - t) + evaluate (w2, {}) * t;
        }

    private:
        static float interpolateCustom (const std::vector<LfoPoint>& pts, float x)
        {
            if (pts.size() < 2) return 0.f;
            for (size_t i = 0; i + 1 < pts.size(); ++i)
            {
                if (x >= pts[i].x && x <= pts[i+1].x)
                {
                    float span = pts[i+1].x - pts[i].x;
                    float t    = (span > 1e-6f) ? (x - pts[i].x) / span : 0.f;
                    // Apply curve (atan-based smooth tension)
                    float crv = pts[i].curve;
                    if (std::abs (crv) > 0.001f)
                    {
                        float k     = crv * 6.0f;
                        float a0    = std::atan (k * -0.5f);
                        float a1    = std::atan (k *  0.5f);
                        float denom = a1 - a0;
                        if (std::abs (denom) > 1e-6f)
                            t = (std::atan (k * (t - 0.5f)) - a0) / denom;
                    }
                    return pts[i].y + t * (pts[i+1].y - pts[i].y);
                }
            }
            return pts.back().y;
        }
    } qLfo[4];

    mutable juce::CriticalSection lfoPointsLock;
    std::vector<LfoPoint>     lfoCustomPoints[4];   // écrit par l'UI
    std::vector<LfoPoint>     lfoAudioPoints[4];    // copie audio locale
    std::atomic<bool>         lfoPointsDirty[4];

    mutable juce::CriticalSection modLock;

    // Mono mode state machine
    juce::Array<int> monoNoteStack;
    int              monoActiveNote { -1 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaSynthProcessor)
};
