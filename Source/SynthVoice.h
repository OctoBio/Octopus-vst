#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>

// ============================================================
//  PolyBLEP anti-aliasing helper
//  Adds a correction polynomial at waveform discontinuities.
//  t   = current phase (0..1)
//  dt  = frequency / sampleRate  (phase increment per sample)
// ============================================================
inline float polyBlep (float t, float dt)
{
    if (t < dt)
    {
        t /= dt;
        return t + t - t * t - 1.0f;
    }
    else if (t > 1.0f - dt)
    {
        t = (t - 1.0f) / dt;
        return t * t + t + t + 1.0f;
    }
    return 0.0f;
}

// ============================================================
//  LFO
// ============================================================
struct LFO
{
    enum class Waveform { Sine, Triangle, Saw, Square };
    Waveform waveform { Waveform::Sine };
    float    rate     { 1.0f };
    float    depth    { 0.0f };
    float    phase    { 0.0f };

    void reset() { phase = 0.0f; }

    float process (float sampleRate)
    {
        float value = 0.0f;
        switch (waveform)
        {
            case Waveform::Sine:
                value = std::sin (phase * juce::MathConstants<float>::twoPi); break;
            case Waveform::Triangle:
                value = (phase < 0.5f) ? (4.f*phase - 1.f) : (3.f - 4.f*phase); break;
            case Waveform::Saw:
                value = 2.f * phase - 1.f; break;
            case Waveform::Square:
                value = (phase < 0.5f) ? 1.f : -1.f; break;
        }
        phase += rate / sampleRate;
        if (phase >= 1.f) phase -= 1.f;
        return value * depth;
    }
};

// ============================================================
//  Oscillateur complet — même paramètres que Serum
// ============================================================
struct Oscillator
{
    // --- Waveform ---
    enum class Waveform { Saw=0, Square=1, Triangle=2, Sine=3 };

    // --- Warp modes (comme Serum) ---
    enum class WarpMode { None=0, BendPos=1, BendNeg=2, Sync=3, Fold=4, Mirror=5, PWM=6 };

    Waveform waveform   { Waveform::Saw  };
    WarpMode warpMode   { WarpMode::None };
    float    warpAmount { 0.0f };          // 0..1

    // --- Tuning (comme Serum : OCT / SEMI / FINE) ---
    int   octave  { 0 };     // -4..+4 octaves
    float tune    { 0.0f };  // semitones ±24  (SEMI)
    float detune  { 0.0f };  // cents ±100      (FINE)

    // --- Niveau & Pan ---
    float level { 1.0f };
    float pan   { 0.0f };  // -1..+1

    // --- Phase ---
    float initPhase { 0.0f };  // 0..1  start phase
    float randPhase { 0.0f };  // 0..1  random amount (0=deterministic, 1=fully random)

    // --- On/Off ---
    bool enabled { true };

    // --- Unison ---
    int   unisonVoices { 1    };  // 1..8
    float unisonDetune { 0.3f };  // 0..1  (spread total en semi-tons)
    float unisonBlend  { 0.5f };  // 0..1  (largeur stéréo)

    // --- État interne (une phase par voix unison) ---
    float phases[8] {};

    void reset()
    {
        // Serum-style: initPhase is the base, randPhase scales a random offset on top
        float rnd   = juce::Random::getSystemRandom().nextFloat();  // 0..1
        float start = initPhase + randPhase * rnd;
        start = start - std::floor (start);   // wrap to 0..1

        for (int v = 0; v < 8; ++v)
            phases[v] = std::fmod (start + (float)v * 0.05f, 1.0f);
    }

    // Retourne {sampleGauche, sampleDroit}
    std::pair<float,float> process (float freq, float sampleRate, float pitchModSemitones = 0.0f);

private:
    float sampleWaveform (float p) const;
    float applyWarp (float p) const;
    void  foldSample (float& y) const;
};

// ============================================================
//  ADSR
// ============================================================
struct ADSREnvelope
{
    float attack  { 0.01f };
    float decay   { 0.10f };
    float sustain { 0.70f };
    float release { 0.30f };

    // Serum-style per-segment curves: -1 = exp fast-slow, 0 = linear, +1 = log slow-fast
    float curveA { 0.0f }, curveD { 0.0f }, curveR { 0.0f };

    enum class Stage { Idle, Attack, Decay, Sustain, Release };
    Stage stage        { Stage::Idle };
    float currentLevel { 0.0f };
    float sampleRate   { 44100.0f };

    void noteOn()  { stage = Stage::Attack; stagePhase = 0.0f; }
    void noteOff() {
        if (stage != Stage::Idle) {
            releaseStartLevel = currentLevel;
            stage = Stage::Release;
            stagePhase = 0.0f;
        }
    }
    bool isActive() const { return stage != Stage::Idle; }

    // Atan-based tension curve: c<0 ease-in, c>0 ease-out (same shape as LFO points)
    static float shapeCurve (float t, float c)
    {
        if (std::abs (c) < 1e-3f) return t;
        float k = c * 6.0f;
        float a0 = std::atan (k * -0.5f);
        float a1 = std::atan (k *  0.5f);
        float denom = a1 - a0;
        if (std::abs (denom) < 1e-6f) return t;
        return (std::atan (k * (t - 0.5f)) - a0) / denom;
    }

    float process()
    {
        switch (stage)
        {
            case Stage::Idle:
                currentLevel = 0.0f;
                break;

            case Stage::Attack: {
                float dt = 1.0f / juce::jmax (1e-4f, attack * sampleRate);
                stagePhase += dt;
                if (stagePhase >= 1.0f) {
                    currentLevel = 1.0f;
                    stage = Stage::Decay;
                    stagePhase = 0.0f;
                } else {
                    currentLevel = shapeCurve (stagePhase, curveA);
                }
                break;
            }

            case Stage::Decay: {
                float dt = 1.0f / juce::jmax (1e-4f, decay * sampleRate);
                stagePhase += dt;
                if (stagePhase >= 1.0f) {
                    currentLevel = sustain;
                    stage = Stage::Sustain;
                    stagePhase = 0.0f;
                } else {
                    float s = shapeCurve (stagePhase, curveD);
                    currentLevel = 1.0f - s * (1.0f - sustain);
                }
                break;
            }

            case Stage::Sustain:
                currentLevel = sustain;
                break;

            case Stage::Release: {
                float dt = 1.0f / juce::jmax (1e-4f, release * sampleRate);
                stagePhase += dt;
                if (stagePhase >= 1.0f) {
                    currentLevel = 0.0f;
                    stage = Stage::Idle;
                } else {
                    float s = shapeCurve (stagePhase, curveR);
                    currentLevel = releaseStartLevel * (1.0f - s);
                }
                break;
            }
        }
        return currentLevel;
    }

private:
    float releaseStartLevel { 0.0f };
    float stagePhase        { 0.0f };
};

// ============================================================
//  SVFilter — State Variable Filter multi-mode
//  LP12, LP24, HP, BP, Notch — même moteur que Serum
// ============================================================
struct SVFilter
{
    enum class Type {
        LP6=0, LP12=1, LP24=2, HP12=3, HP24=4, BP=5, Notch=6, Comb=7,
        BP24=8, Notch24=9, LowShelf=10, HighShelf=11, PeakEQ=12
    };

    float s1{0}, s2{0};   // état 1ère passe
    float s1b{0}, s2b{0}; // état 2ème passe (pour LP24 / HP24)
    float combFeedback{0.0f}; // comb filter state

    float process (float in, float cutoff, float res, float sr, Type type)
    {
        float g  = std::tan (juce::MathConstants<float>::pi
                             * juce::jlimit (20.0f, sr * 0.49f, cutoff) / sr);
        float k  = 2.0f - 2.0f * juce::jlimit (0.0f, 0.99f, res);
        float a1 = 1.0f / (1.0f + g * (g + k));
        float a2 = g * a1;
        float a3 = g * a2;

        float v3 = in - s2;
        float v1 = a1 * s1 + a2 * v3;
        float v2 = s2 + a2 * s1 + a3 * v3;
        s1 = 2.0f * v1 - s1;
        s2 = 2.0f * v2 - s2;

        float lp = v2;
        float bp = v1;
        float hp = (in - k * s1 / 2.0f - s2);
        float nt = in - k * bp;

        switch (type)
        {
            case Type::LP6: {
                // Single-pole LP approximation using SVF LP output
                return lp * 0.5f + s2 * 0.5f;
            }
            case Type::LP12: return lp;
            case Type::HP12: return hp;
            case Type::BP:   return bp;
            case Type::Notch: return nt;
            case Type::LP24: {
                // 2ème passe LP pour 24dB/oct
                float v3b = lp - s2b;
                float v1b = a1 * s1b + a2 * v3b;
                float v2b = s2b + a2 * s1b + a3 * v3b;
                s1b = 2.0f * v1b - s1b;
                s2b = 2.0f * v2b - s2b;
                return v2b;
            }
            case Type::HP24: {
                // 2ème passe HP pour 24dB/oct
                float v3b = hp - s2b;
                float v1b = a1 * s1b + a2 * v3b;
                float v2b = s2b + a2 * s1b + a3 * v3b;
                s1b = 2.0f * v1b - s1b;
                s2b = 2.0f * v2b - s2b;
                float hpb = hp - k * s1b / 2.0f - s2b;
                return hpb;
            }
            case Type::Comb: {
                // Simple comb filter with feedback
                float feedback = 0.5f + res * 0.45f;
                float out = in + combFeedback * feedback;
                combFeedback = out;
                return out * 0.5f;
            }
            case Type::BP24: {
                // 2ème passe BP pour 24dB/oct (plus résonnant, plus étroit)
                float v3b = bp - s2b;
                float v1b = a1 * s1b + a2 * v3b;
                float v2b = s2b + a2 * s1b + a3 * v3b;
                s1b = 2.0f * v1b - s1b;
                s2b = 2.0f * v2b - s2b;
                return v1b;
            }
            case Type::Notch24: {
                // 2ème passe Notch
                float v3b = nt - s2b;
                float v1b = a1 * s1b + a2 * v3b;
                float v2b = s2b + a2 * s1b + a3 * v3b;
                s1b = 2.0f * v1b - s1b;
                s2b = 2.0f * v2b - s2b;
                float ntb = nt - k * v1b;
                return ntb;
            }
            case Type::LowShelf: {
                // Low shelf: boost/cut below cutoff.
                // Resonance knob maps to gain: 0 → -12dB, 0.5 → 0dB, 1 → +12dB
                float gainDb = (res - 0.5f) * 24.0f;
                float A = std::pow (10.0f, gainDb / 40.0f);   // shelf scaling
                return in + (A - 1.0f) * lp + (1.0f/A - 1.0f) * hp * 0.0f;
            }
            case Type::HighShelf: {
                float gainDb = (res - 0.5f) * 24.0f;
                float A = std::pow (10.0f, gainDb / 40.0f);
                return in + (A - 1.0f) * hp;
            }
            case Type::PeakEQ: {
                // Peak/bell: resonance doubles as gain (0.5 = flat, 1 = +12dB, 0 = -12dB)
                float gainDb = (res - 0.5f) * 24.0f;
                float A = std::pow (10.0f, gainDb / 20.0f);
                return in + (A - 1.0f) * bp;
            }
        }
        return lp;
    }
};

// ============================================================
//  SynthVoice
// ============================================================
class SynthVoice : public juce::SynthesiserVoice
{
public:
    SynthVoice();

    bool canPlaySound (juce::SynthesiserSound*) override;
    void startNote    (int midiNote, float velocity, juce::SynthesiserSound*, int) override;
    void stopNote     (float velocity, bool allowTailOff) override;
    void pitchWheelMoved (int) override {}
    void controllerMoved (int, int) override {}
    void renderNextBlock (juce::AudioBuffer<float>&, int startSample, int numSamples) override;
    void prepareToPlay  (double sampleRate, int samplesPerBlock, int numChannels);

    Oscillator   osc1, osc2, osc3;
    LFO          lfo1, lfo2;
    ADSREnvelope adsr;    // ENV1 → Volume
    ADSREnvelope adsr2;   // ENV2 → Filter
    ADSREnvelope adsr3;   // ENV3 → OSC3

    // Sub oscillateur
    float subLevel   { 0.0f };
    int   subOctave  { -1   };
    int   subWave    { 0    };  // 0=Sine, 1=Triangle, 2=Saw, 3=Square
    float subPhase   { 0.0f };
    bool  subEnabled { true  };

    // Noise oscillator
    bool  noiseEnabled { false };
    float noiseLevel   { 0.5f  };
    float noisePan     { 0.0f  };
    int   noiseType    { 0     };  // 0=White, 1=Pink, 2=Brown
    juce::Random noiseRng;         // RNG local à la voix (thread-safe)
    float noisePinkB0  { 0.f   };
    float noisePinkB1  { 0.f   };
    float noisePinkB2  { 0.f   };
    float noiseBrownX  { 0.f   };

    // Filtre
    float          filterCutoff    { 8000.0f };
    float          filterResonance { 0.0f    };
    SVFilter::Type filterType      { SVFilter::Type::LP12 };
    float          filterEnvAmount { 0.0f };
    float          filterDrive     { 0.0f };
    float          filterFat       { 0.0f };
    float          filterMix       { 1.0f };
    float          filterPan       { 0.0f };
    bool           filterEnabled   { true  };
    bool           filterRouteOsc1 { true  };
    bool           filterRouteOsc2 { true  };
    bool           filterRouteOsc3 { true  };

    // Portamento (mono lead mode)
    bool  portaActive      { false   };
    float portaCurrentFreq { 440.0f  };
    float portaTargetFreq  { 440.0f  };
    float portaCoeff       { 0.0f    }; // 0=instant, 0.999=très lent

    // Last envelope values (read by PluginProcessor for modulation)
    float lastEnv2Val { 0.f };
    float lastEnv3Val { 0.f };

    // ---- Voice-stealing fade-out ----
    // When a voice is stolen, stealFadeGain ramps from current level to 0
    // over stealFadeSamples to prevent discontinuity clicks.
    bool  isBeingStolen     { false };
    float stealFadeGain     { 1.0f  };
    int   stealFadeSamples  { 0     };
    int   stealFadeCounter  { 0     };

    // Called by the Synthesiser when this voice is about to be stolen.
    // Initiates a short fade-out (≈ 5 ms).
    void beginSteal (double sr)
    {
        isBeingStolen   = true;
        stealFadeGain   = 1.0f;
        stealFadeSamples = juce::jmax (1, (int)(sr * 0.005));   // 5 ms
        stealFadeCounter = 0;
    }

private:
    float noteFrequency   { 440.0f };
    float currentVelocity { 1.0f   };
    double sampleRate     { 44100.0 };
    bool  prepared        { false   };

    SVFilter filterL, filterR, filterFatL, filterFatR;

    // DC blocking filter state (one per channel)
    float dcX[2] { 0.0f, 0.0f };   // previous input sample
    float dcY[2] { 0.0f, 0.0f };   // previous output sample
    static constexpr float DC_R = 0.995f;
};
