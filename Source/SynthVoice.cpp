#include "SynthVoice.h"
#include "SynthSound.h"

// ============================================================
//  Oscillateur — génération de forme d'onde
// ============================================================

float Oscillator::sampleWaveform (float p) const
{
    switch (waveform)
    {
        case Waveform::Saw:
            return 2.0f * p - 1.0f;

        case Waveform::Square: {
            // PWM : duty cycle variable selon warpAmount
            float duty = (warpMode == WarpMode::PWM)
                ? 0.05f + warpAmount * 0.90f   // 5%..95%
                : 0.5f;
            return (p < duty) ? 1.0f : -1.0f;
        }

        case Waveform::Triangle:
            return (p < 0.5f) ? (4.0f*p - 1.0f) : (3.0f - 4.0f*p);

        case Waveform::Sine:
            return std::sin (p * juce::MathConstants<float>::twoPi);

        default: return 0.0f;
    }
}

float Oscillator::applyWarp (float p) const
{
    float a = warpAmount;

    switch (warpMode)
    {
        case WarpMode::None:    return p;

        case WarpMode::BendPos: // Compresse la première moitié → attaque plus rapide
            return std::pow (p, 1.0f / (1.0f + a * 3.0f));

        case WarpMode::BendNeg: // Étire la première moitié → attaque plus douce
            return std::pow (p, 1.0f + a * 3.0f);

        case WarpMode::Sync:    // Hard sync simulé : cycles multiples
            return std::fmod (p * (1.0f + a * 7.0f), 1.0f);

        case WarpMode::Fold:    // Fold appliqué après la génération (voir foldSample)
        case WarpMode::PWM:     // PWM appliqué dans sampleWaveform
            return p;

        case WarpMode::Mirror:  // Miroir : retourne la 2e moitié
            return (p < 0.5f) ? p * 2.0f : (1.0f - p) * 2.0f;

        default: return p;
    }
}

void Oscillator::foldSample (float& y) const
{
    if (warpMode != WarpMode::Fold || warpAmount <= 0.001f) return;

    float gain = 1.0f + warpAmount * 3.5f;
    y *= gain;

    // Refléter à ±1 (wavefold)
    for (int i = 0; i < 8; ++i)
    {
        if      (y >  1.0f) y =  2.0f - y;
        else if (y < -1.0f) y = -2.0f - y;
        else break;
    }
}

std::pair<float,float> Oscillator::process (float freq, float sampleRate, float pitchModSemitones)
{
    if (!enabled) return { 0.0f, 0.0f };

    int voices = juce::jlimit (1, 8, unisonVoices);

    // Semitones totaux : octave + semi + fine + modulation
    float baseSemitones = (float)(octave * 12) + tune + (detune / 100.0f) + pitchModSemitones;
    float baseFreq = freq * std::pow (2.0f, baseSemitones / 12.0f);

    // Spread de detune total en cents pour toutes les voix unison
    float spreadCents = unisonDetune * 100.0f;

    float leftOut  = 0.0f;
    float rightOut = 0.0f;

    for (int v = 0; v < voices; ++v)
    {
        // Position de cette voix dans le spread [-1..+1]
        float voicePos = (voices > 1)
            ? (float)(v * 2 - (voices - 1)) / (float)(voices - 1)
            : 0.0f;

        // Fréquence de cette voix unison
        float voiceFreq = baseFreq * std::pow (2.0f, voicePos * spreadCents * 0.5f / 1200.0f);

        // dt = phase increment per sample (used by PolyBLEP)
        float dt = voiceFreq / sampleRate;

        // Générer le sample
        float p       = phases[v];
        float warped  = applyWarp (p);
        float sample  = sampleWaveform (warped);

        // ---- PolyBLEP anti-aliasing ----
        // Only applied to raw (non-warped) phase so the correction aligns with
        // the actual discontinuity location.  Warp modes other than None/PWM
        // move the discontinuity; for simplicity we skip BLEP on those modes
        // (oversampling would be needed there anyway).
        if (warpMode == WarpMode::None || warpMode == WarpMode::PWM)
        {
            switch (waveform)
            {
                case Waveform::Saw:
                    // Discontinuity at phase == 0 (wrap-around)
                    sample -= polyBlep (p, dt);
                    break;

                case Waveform::Square:
                {
                    float duty = (warpMode == WarpMode::PWM)
                        ? 0.05f + warpAmount * 0.90f
                        : 0.5f;
                    // Rising edge at 0, falling edge at duty
                    sample += polyBlep (p,               dt);
                    sample -= polyBlep (std::fmod (p + 1.0f - duty, 1.0f), dt);
                    break;
                }

                default: break;  // Sine and Triangle have no hard discontinuities
            }
        }

        foldSample (sample);

        // Pan : pan de base + spread stéréo unison
        float voicePan = juce::jlimit (-1.0f, 1.0f, pan + voicePos * unisonBlend);
        float panAngle = (voicePan + 1.0f) * 0.5f * juce::MathConstants<float>::halfPi;
        leftOut  += sample * std::cos (panAngle);
        rightOut += sample * std::sin (panAngle);

        // Avancer la phase
        phases[v] += dt;
        if (phases[v] >= 1.0f) phases[v] -= 1.0f;
    }

    // Normalisation par √(voices) pour garder le volume stable
    float norm = level / std::sqrt ((float)voices);
    return { leftOut * norm, rightOut * norm };
}

// ============================================================
//  SynthVoice
// ============================================================

SynthVoice::SynthVoice()
{
    osc1.waveform = Oscillator::Waveform::Saw;
    osc1.level    = 0.8f;

    osc2.waveform     = Oscillator::Waveform::Saw;
    osc2.detune       = 5.0f;
    osc2.level        = 0.6f;
    osc2.unisonVoices = 1;

    osc3.waveform     = Oscillator::Waveform::Saw;
    osc3.detune       = -5.0f;
    osc3.level        = 0.0f;   // désactivé par défaut (level=0)
    osc3.unisonVoices = 1;
    osc3.enabled      = true;

    lfo1.rate  = 5.0f;  lfo1.depth = 0.0f;
    lfo2.rate  = 4.0f;  lfo2.depth = 0.0f;
}

bool SynthVoice::canPlaySound (juce::SynthesiserSound* s)
{
    return dynamic_cast<SynthSound*>(s) != nullptr;
}

void SynthVoice::prepareToPlay (double sr, int, int)
{
    sampleRate  = sr;
    adsr.sampleRate  = (float)sr;
    adsr2.sampleRate = (float)sr;
    adsr3.sampleRate = (float)sr;
    prepared    = true;
}

void SynthVoice::startNote (int midiNote, float velocity, juce::SynthesiserSound*, int)
{
    float newFreq = (float)(440.0 * std::pow (2.0, (midiNote - 69) / 12.0));
    currentVelocity = velocity;
    noteFrequency   = newFreq;

    if (portaActive)
    {
        // Legato : on garde la fréquence courante et on glisse vers la nouvelle
        portaTargetFreq = newFreq;
        // Si la voix n'était pas active, partir de la fréquence cible directement
        if (!adsr.isActive())
            portaCurrentFreq = newFreq;
    }
    else
    {
        portaCurrentFreq = newFreq;
        portaTargetFreq  = newFreq;
    }

    osc1.reset();
    osc2.reset();
    osc3.reset();
    lfo1.reset();
    lfo2.reset();
    subPhase = 0.0f;

    // Reset DC blocker state to avoid previous-note offset bleeding in
    dcX[0] = dcX[1] = 0.0f;
    dcY[0] = dcY[1] = 0.0f;

    // Cancel any in-progress steal fade from a previous steal
    isBeingStolen   = false;
    stealFadeGain   = 1.0f;
    stealFadeCounter = 0;

    adsr.noteOn();
    adsr2.noteOn();
    adsr3.noteOn();
}

void SynthVoice::stopNote (float, bool allowTailOff)
{
    if (allowTailOff)
    {
        adsr.noteOff();
        adsr2.noteOff();
        adsr3.noteOff();
    }
    else
    {
        // Voice is being stolen (allowTailOff == false).
        // Instead of an instant kill (which causes a click), fade out over ~5 ms.
        if (adsr.isActive() && !isBeingStolen)
            beginSteal (sampleRate);
        else
        {
            adsr.stage  = ADSREnvelope::Stage::Idle;
            adsr2.stage = ADSREnvelope::Stage::Idle;
            adsr3.stage = ADSREnvelope::Stage::Idle;
        }
    }
}

void SynthVoice::renderNextBlock (juce::AudioBuffer<float>& buffer,
                                   int startSample, int numSamples)
{
    if (!prepared || !adsr.isActive()) return;

    auto* left  = buffer.getWritePointer (0);
    auto* right = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;

    for (int i = startSample; i < startSample + numSamples; ++i)
    {
        lfo1.process ((float)sampleRate);
        lfo2.process ((float)sampleRate);

        // Portamento — glisse vers la fréquence cible
        if (portaActive && portaCoeff > 0.0f)
        {
            portaCurrentFreq += (portaTargetFreq - portaCurrentFreq) * (1.0f - portaCoeff);
        }
        else
        {
            portaCurrentFreq = portaTargetFreq;
        }
        float freq = portaCurrentFreq;

        // Oscillateurs
        auto [l1, r1] = osc1.process (freq, (float)sampleRate, 0.0f);
        auto [l2, r2] = osc2.process (freq, (float)sampleRate, 0.0f);
        auto [l3, r3] = osc3.process (freq, (float)sampleRate, 0.0f);

        // Sub
        float subS = 0.0f;
        if (subEnabled && subLevel > 0.001f)
        {
            float sFreq = freq * std::pow (2.0f, (float)subOctave);
            subPhase += sFreq / (float)sampleRate;
            if (subPhase >= 1.0f) subPhase -= 1.0f;
            switch (subWave)
            {
                case 0: subS = std::sin (subPhase * juce::MathConstants<float>::twoPi); break;
                case 1: subS = (subPhase < 0.5f) ? (4.f*subPhase - 1.f) : (3.f - 4.f*subPhase); break;
                case 2: subS = 2.f * subPhase - 1.f; break;
                case 3: subS = (subPhase < 0.5f) ? 1.f : -1.f; break;
                default: subS = std::sin (subPhase * juce::MathConstants<float>::twoPi); break;
            }
            subS *= subLevel;
        }

        // Noise oscillator
        float noiseL = 0.f, noiseR = 0.f;
        if (noiseEnabled)
        {
            float white = (noiseRng.nextFloat() * 2.f - 1.f) * noiseLevel;
            float noise = white;
            if (noiseType == 1) // Pink (Paul Kellett method)
            {
                noisePinkB0 = 0.99886f * noisePinkB0 + white * 0.0555179f;
                noisePinkB1 = 0.99332f * noisePinkB1 + white * 0.0750759f;
                noisePinkB2 = 0.96900f * noisePinkB2 + white * 0.1538520f;
                noise = (noisePinkB0 + noisePinkB1 + noisePinkB2 + white * 0.5362f) * 0.11f;
            }
            else if (noiseType == 2) // Brown
            {
                noiseBrownX = (noiseBrownX + 0.02f * white) / 1.02f;
                noise = noiseBrownX * 3.5f * noiseLevel;
            }
            float angle = (noisePan + 1.f) * 0.5f * juce::MathConstants<float>::halfPi;
            noiseL = noise * std::cos (angle);
            noiseR = noise * std::sin (angle);
        }

        // Routing OSC → filtre
        float toFiltL = (filterRouteOsc1 ? l1 : 0.f)
                      + (filterRouteOsc2 ? l2 : 0.f)
                      + (filterRouteOsc3 ? l3 : 0.f);
        float toFiltR = (filterRouteOsc1 ? r1 : 0.f)
                      + (filterRouteOsc2 ? r2 : 0.f)
                      + (filterRouteOsc3 ? r3 : 0.f);
        toFiltL = toFiltL * 0.5f + subS * 0.5f + noiseL;
        toFiltR = toFiltR * 0.5f + subS * 0.5f + noiseR;

        // Signaux en bypass (OSC non routés vers le filtre)
        float bypL = (!filterRouteOsc1 ? l1 : 0.f)
                   + (!filterRouteOsc2 ? l2 : 0.f)
                   + (!filterRouteOsc3 ? l3 : 0.f);
        float bypR = (!filterRouteOsc1 ? r1 : 0.f)
                   + (!filterRouteOsc2 ? r2 : 0.f)
                   + (!filterRouteOsc3 ? r3 : 0.f);
        bypL *= 0.5f;  bypR *= 0.5f;

        // ENV2 → cutoff
        float env2Val    = adsr2.process();
        lastEnv2Val      = env2Val;
        lastEnv3Val      = adsr3.process();
        float modCutoff  = juce::jlimit (20.0f, 20000.0f,
                               filterCutoff * std::pow (2.0f, filterEnvAmount * env2Val * 5.0f));

        float outL, outR;

        if (filterEnabled)
        {
            // Drive — saturation douce
            if (filterDrive > 0.001f)
            {
                float dg = 1.0f + filterDrive * 5.0f;
                toFiltL = std::tanh (toFiltL * dg) / dg;
                toFiltR = std::tanh (toFiltR * dg) / dg;
            }

            float wetL = filterL.process (toFiltL, modCutoff, filterResonance,
                                           (float)sampleRate, filterType);
            float wetR = filterR.process (toFiltR, modCutoff, filterResonance,
                                           (float)sampleRate, filterType);

            // Fat — 2ème passe parallèle légèrement décalée
            if (filterFat > 0.001f)
            {
                float fatCut = juce::jlimit (20.0f, 20000.0f,
                                   modCutoff * (1.0f - filterFat * 0.25f));
                float fatL = filterFatL.process (toFiltL, fatCut,
                                   filterResonance * 0.5f, (float)sampleRate, filterType);
                float fatR = filterFatR.process (toFiltR, fatCut,
                                   filterResonance * 0.5f, (float)sampleRate, filterType);
                float inv = 1.0f / (1.0f + filterFat);
                wetL = (wetL + fatL * filterFat) * inv;
                wetR = (wetR + fatR * filterFat) * inv;
            }

            // Mix dry/wet
            float mixedL = wetL * filterMix + toFiltL * (1.0f - filterMix);
            float mixedR = wetR * filterMix + toFiltR * (1.0f - filterMix);

            // Pan sortie filtre
            float panAngle = (filterPan + 1.0f) * 0.5f * juce::MathConstants<float>::halfPi;
            outL = mixedL * std::cos (panAngle) + bypL;
            outR = mixedR * std::sin (panAngle) + bypR;
        }
        else
        {
            outL = toFiltL + bypL;
            outR = toFiltR + bypR;
        }

        float env  = adsr.process();
        float gain = env * currentVelocity;

        // ---- DC blocking filter ----
        // y[n] = x[n] - x[n-1] + R * y[n-1]   (R = DC_R ≈ 0.995)
        // Removes any slow DC offset introduced by the oscillators / filters.
        float rawL = outL * gain;
        float rawR = (right ? outR * gain : 0.0f);

        float dcOutL = rawL - dcX[0] + DC_R * dcY[0];
        dcX[0] = rawL;
        dcY[0] = dcOutL;

        float dcOutR = rawR - dcX[1] + DC_R * dcY[1];
        dcX[1] = rawR;
        dcY[1] = dcOutR;

        // ---- Voice-stealing fade-out ----
        float stealScale = 1.0f;
        if (isBeingStolen)
        {
            stealScale = 1.0f - (float)stealFadeCounter / (float)stealFadeSamples;
            stealScale = juce::jlimit (0.0f, 1.0f, stealScale);
            ++stealFadeCounter;
            if (stealFadeCounter >= stealFadeSamples)
            {
                // Fade complete — force voice idle so JUCE reclaims it
                adsr.stage  = ADSREnvelope::Stage::Idle;
                adsr2.stage = ADSREnvelope::Stage::Idle;
                adsr3.stage = ADSREnvelope::Stage::Idle;
                isBeingStolen = false;
            }
        }

        left[i]  += dcOutL * stealScale;
        if (right) right[i] += dcOutR * stealScale;
    }

    if (!adsr.isActive())
        clearCurrentNote();
}
