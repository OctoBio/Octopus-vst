#include "PluginProcessor.h"
#include "PluginEditor.h"

juce::AudioProcessorValueTreeState::ParameterLayout NovaSynthProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // ---- Helper lambdas ----
    auto pFloat = [](const char* id, const char* name, float lo, float hi, float def)
    {
        return std::make_unique<juce::AudioParameterFloat>(id, name, lo, hi, def);
    };
    auto pChoice = [](const char* id, const char* name, juce::StringArray choices, int def = 0)
    {
        return std::make_unique<juce::AudioParameterChoice>(id, name, choices, def);
    };

    juce::StringArray waveforms { "Saw", "Square", "Triangle", "Sine" };
    juce::StringArray warpModes { "None", "Bend+", "Bend-", "Sync", "Fold", "Mirror", "PWM" };
    juce::StringArray lfoWaves  { "Sine", "Triangle", "Saw", "Square" };

    for (int osc = 1; osc <= 3; ++osc)
    {
        juce::String o = juce::String(osc);

        // Waveform & Warp
        params.push_back (pChoice (("osc"+o+"Waveform").toRawUTF8(),
                                    ("OSC"+o+" Wave").toRawUTF8(), waveforms));
        params.push_back (pChoice (("osc"+o+"WarpMode").toRawUTF8(),
                                    ("OSC"+o+" Warp Mode").toRawUTF8(), warpModes));
        params.push_back (pFloat  (("osc"+o+"WarpAmt").toRawUTF8(),
                                    ("OSC"+o+" Warp Amt").toRawUTF8(), 0.0f, 1.0f, 0.0f));

        // Tuning
        params.push_back (pFloat (("osc"+o+"Oct").toRawUTF8(),
                                   ("OSC"+o+" Oct").toRawUTF8(),   -4.0f, 4.0f, 0.0f));
        params.push_back (pFloat (("osc"+o+"Tune").toRawUTF8(),
                                   ("OSC"+o+" Semi").toRawUTF8(), -24.0f, 24.0f, 0.0f));
        params.push_back (pFloat (("osc"+o+"Detune").toRawUTF8(),
                                   ("OSC"+o+" Fine").toRawUTF8(), -100.0f, 100.0f,
                                   osc == 2 ? 5.0f : (osc == 3 ? -5.0f : 0.0f)));

        // Level & Pan
        params.push_back (pFloat (("osc"+o+"Level").toRawUTF8(),
                                   ("OSC"+o+" Level").toRawUTF8(), 0.0f, 1.0f,
                                   osc == 1 ? 0.8f : (osc == 2 ? 0.6f : 0.0f)));
        params.push_back (pFloat (("osc"+o+"Pan").toRawUTF8(),
                                   ("OSC"+o+" Pan").toRawUTF8(), -1.0f, 1.0f, 0.0f));

        // Phase
        params.push_back (pFloat (("osc"+o+"Phase").toRawUTF8(),
                                   ("OSC"+o+" Phase").toRawUTF8(), 0.0f, 1.0f, 0.0f));
        params.push_back (pFloat (("osc"+o+"RandPhase").toRawUTF8(),
                                   ("OSC"+o+" Rand Phase").toRawUTF8(), 0.0f, 1.0f, 0.0f));

        // Unison
        params.push_back (pFloat (("osc"+o+"UniVoices").toRawUTF8(),
                                   ("OSC"+o+" Uni Voices").toRawUTF8(), 1.0f, 8.0f, 1.0f));
        params.push_back (pFloat (("osc"+o+"UniDetune").toRawUTF8(),
                                   ("OSC"+o+" Uni Detune").toRawUTF8(), 0.0f, 1.0f, 0.3f));
        params.push_back (pFloat (("osc"+o+"UniBlend").toRawUTF8(),
                                   ("OSC"+o+" Uni Blend").toRawUTF8(), 0.0f, 1.0f, 0.5f));

        // Enable — seul OSC1 est activé par défaut
        float defaultEnabled = (osc == 1) ? 1.0f : 0.0f;
        params.push_back (pFloat (("osc"+o+"Enabled").toRawUTF8(),
                                   ("OSC"+o+" On").toRawUTF8(), 0.0f, 1.0f, defaultEnabled));
    }

    // ---- ADSR (plages Serum : 0.5ms → 10s, courbe log) ----
    auto timeRange = juce::NormalisableRange<float> (0.0005f, 10.0f, 0.f, 0.25f);
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("attack",  "Attack",  timeRange, 0.01f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("decay",   "Decay",   timeRange, 0.10f));
    params.push_back (pFloat ("sustain", "Sustain", 0.0f, 1.0f, 0.70f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("release", "Release", timeRange, 0.30f));
    // ENV1 per-segment curves (-1..+1, 0 = linear)
    params.push_back (pFloat ("env1CurveA", "ENV1 Curve A", -1.0f, 1.0f, 0.0f));
    params.push_back (pFloat ("env1CurveD", "ENV1 Curve D", -1.0f, 1.0f, 0.0f));
    params.push_back (pFloat ("env1CurveR", "ENV1 Curve R", -1.0f, 1.0f, 0.0f));

    // ---- ENV2 (Filter envelope) ----
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("env2Attack",  "ENV2 Attack",  timeRange, 0.01f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("env2Decay",   "ENV2 Decay",   timeRange, 0.30f));
    params.push_back (pFloat ("env2Sustain", "ENV2 Sustain", 0.0f, 1.0f, 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("env2Release", "ENV2 Release", timeRange, 0.20f));
    params.push_back (pFloat ("env2CurveA", "ENV2 Curve A", -1.0f, 1.0f, 0.0f));
    params.push_back (pFloat ("env2CurveD", "ENV2 Curve D", -1.0f, 1.0f, 0.0f));
    params.push_back (pFloat ("env2CurveR", "ENV2 Curve R", -1.0f, 1.0f, 0.0f));

    // ---- ENV3 (OSC3 envelope) ----
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("env3Attack",  "ENV3 Attack",  timeRange, 0.01f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("env3Decay",   "ENV3 Decay",   timeRange, 0.10f));
    params.push_back (pFloat ("env3Sustain", "ENV3 Sustain", 0.0f, 1.0f, 0.70f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("env3Release", "ENV3 Release", timeRange, 0.30f));
    params.push_back (pFloat ("env3CurveA", "ENV3 Curve A", -1.0f, 1.0f, 0.0f));
    params.push_back (pFloat ("env3CurveD", "ENV3 Curve D", -1.0f, 1.0f, 0.0f));
    params.push_back (pFloat ("env3CurveR", "ENV3 Curve R", -1.0f, 1.0f, 0.0f));

    // ---- Filter ----
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "filterCutoff", "Filter Cutoff",
        juce::NormalisableRange<float> (20.0f, 20000.0f, 0.0f, 0.3f), 8000.0f));
    params.push_back (pFloat ("filterRes",     "Filter Res",     0.0f,  1.0f,  0.0f));
    params.push_back (pChoice ("filterType",   "Filter Type",    {
        "LP 6","LP 12","LP 24","HP 12","HP 24","BP 12","Notch","Comb",
        "BP 24","Notch 24","Low Shelf","High Shelf","Peak EQ"
    }));
    params.push_back (pFloat ("filterEnvAmt",  "Filter Env Amt", -1.0f, 1.0f,  0.0f));
    params.push_back (pFloat ("filterDrive",   "Filter Drive",   0.0f,  1.0f,  0.0f));
    params.push_back (pFloat ("filterFat",     "Filter Fat",     0.0f,  1.0f,  0.0f));
    params.push_back (pFloat ("filterMix",     "Filter Mix",     0.0f,  1.0f,  1.0f));
    params.push_back (pFloat ("filterPan",     "Filter Pan",    -1.0f,  1.0f,  0.0f));
    params.push_back (pFloat ("filterEnabled",  "Filter On",      0.0f, 1.0f, 0.0f));
    params.push_back (pFloat ("filterRouteOsc1","Filter R OSC1",  0.0f, 1.0f, 1.0f));
    params.push_back (pFloat ("filterRouteOsc2","Filter R OSC2",  0.0f, 1.0f, 1.0f));
    params.push_back (pFloat ("filterRouteOsc3","Filter R OSC3",  0.0f, 1.0f, 1.0f));

    // Mono Lead Mode
    params.push_back (pFloat ("monoMode",  "Mono Mode",  0.0f, 1.0f, 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "portaTime", "Portamento",
        juce::NormalisableRange<float> (0.0f, 2.0f, 0.f, 0.35f), 0.05f));

    // ---- Sub oscillateur ----
    params.push_back (pFloat  ("subLevel",   "Sub Level", 0.0f, 1.0f, 0.0f));
    params.push_back (pChoice ("subWave",    "Sub Wave",  {"Sine","Triangle","Saw","Square"}));
    params.push_back (pFloat  ("subOct",     "Sub Oct",   -2.0f, 0.0f, -1.0f));
    params.push_back (pFloat  ("subEnabled", "Sub On",     0.0f, 1.0f, 0.0f));

    // ---- Noise oscillateur ----
    params.push_back (pFloat  ("noiseEnabled", "Noise On",    0.0f, 1.0f, 0.0f));
    params.push_back (pFloat  ("noiseLevel",   "Noise Level", 0.0f, 1.0f, 0.5f));
    params.push_back (pFloat  ("noisePan",     "Noise Pan",  -1.0f, 1.0f, 0.0f));
    params.push_back (pChoice ("noiseType",    "Noise Type",  {"White","Pink","Brown"}));

    juce::StringArray lfoModes { "Free", "Trig", "One" };

    // LFO rate : 0.02 Hz → 20 Hz (courbe log, comme Serum)
    auto lfoRateRange = juce::NormalisableRange<float> (0.02f, 20.0f, 0.f, 0.35f);

    // ---- LFO 1 ----
    params.push_back (pChoice ("lfo1Wave",  "LFO1 Wave",  lfoWaves));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("lfo1Rate", "LFO1 Rate", lfoRateRange, 5.0f));
    params.push_back (pFloat  ("lfo1Depth", "LFO1 Depth", 0.0f, 1.0f, 0.0f));
    params.push_back (pChoice ("lfo1Mode",  "LFO1 Mode",  lfoModes));

    // ---- LFO 2 ----
    params.push_back (pChoice ("lfo2Wave",  "LFO2 Wave",  lfoWaves));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("lfo2Rate", "LFO2 Rate", lfoRateRange, 4.0f));
    params.push_back (pFloat  ("lfo2Depth", "LFO2 Depth", 0.0f, 1.0f, 0.0f));
    params.push_back (pChoice ("lfo2Mode",  "LFO2 Mode",  lfoModes));

    // ---- LFO 3 ----
    params.push_back (pChoice ("lfo3Wave",  "LFO3 Wave",  lfoWaves));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("lfo3Rate", "LFO3 Rate", lfoRateRange, 3.0f));
    params.push_back (pChoice ("lfo3Mode",  "LFO3 Mode",  lfoModes));

    // ---- LFO 4 ----
    params.push_back (pChoice ("lfo4Wave",  "LFO4 Wave",  lfoWaves));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("lfo4Rate", "LFO4 Rate", lfoRateRange, 1.5f));
    params.push_back (pChoice ("lfo4Mode",  "LFO4 Mode",  lfoModes));

    // ---- LFO Shape (continuous morph 0=Sine 1=Tri 2=Saw 3=Square) ----
    params.push_back (pFloat ("lfo1Shape", "LFO1 Shape", 0.0f, 3.0f, 0.0f));
    params.push_back (pFloat ("lfo2Shape", "LFO2 Shape", 0.0f, 3.0f, 0.0f));
    params.push_back (pFloat ("lfo3Shape", "LFO3 Shape", 0.0f, 3.0f, 0.0f));
    params.push_back (pFloat ("lfo4Shape", "LFO4 Shape", 0.0f, 3.0f, 0.0f));

    // ---- LFO Tempo Sync (BPM mode + musical division) ----
    juce::StringArray syncDivs { "4/1","2/1","1/1","1/2","1/4","1/8","1/16","1/32","1/64" };
    for (int i = 1; i <= 4; ++i) {
        juce::String n = juce::String(i);
        params.push_back (pFloat  (("lfo"+n+"Sync").toRawUTF8(),
                                    ("LFO"+n+" Sync").toRawUTF8(), 0.0f, 1.0f, 0.0f));
        params.push_back (pChoice (("lfo"+n+"SyncDiv").toRawUTF8(),
                                    ("LFO"+n+" Div").toRawUTF8(), syncDivs));
    }

    // ---- Multiband Compressor ----
    params.push_back (pFloat ("compEnabled",   "Comp On",      0.0f,  1.0f,  0.0f));
    params.push_back (pFloat ("compInGain",    "Comp In",     -12.0f, 12.0f, 0.0f));
    params.push_back (pFloat ("compOutGain",   "Comp Out",    -12.0f,  6.0f, 0.0f));
    params.push_back (pFloat ("compLowThresh", "Comp Lo Thr", -60.0f, 0.0f, -20.0f));
    params.push_back (pFloat ("compMidThresh", "Comp Mi Thr", -60.0f, 0.0f, -20.0f));
    params.push_back (pFloat ("compHiThresh",  "Comp Hi Thr", -60.0f, 0.0f, -20.0f));
    params.push_back (pFloat ("compLowRatio",  "Comp Lo Rat",   1.0f, 100.0f, 4.0f));
    params.push_back (pFloat ("compMidRatio",  "Comp Mi Rat",   1.0f, 100.0f, 4.0f));
    params.push_back (pFloat ("compHiRatio",   "Comp Hi Rat",   1.0f, 100.0f, 4.0f));
    params.push_back (pFloat ("compUpward",    "Comp Upward",   0.0f, 1.0f, 0.5f)); // upward comp amount
    params.push_back (pFloat ("compMix",       "Comp Mix",      0.0f,  1.0f, 1.0f));

    // ---- Compressor additions ----
    params.push_back (pFloat ("compDepth",   "Comp Depth",    0.0f, 1.0f, 1.0f));
    params.push_back (pFloat ("compLowGain", "Comp Lo Gain", -12.0f, 6.0f, 0.0f));
    params.push_back (pFloat ("compMidGain", "Comp Mid Gain",-12.0f, 6.0f, 0.0f));
    params.push_back (pFloat ("compHiGain",  "Comp Hi Gain", -12.0f, 6.0f, 0.0f));

    // ---- Overdrive / Distortion ----
    params.push_back (pFloat  ("driveEnabled", "Drive On",   0.0f, 1.0f, 0.0f));
    params.push_back (pChoice ("driveMode",    "Drive Mode",
        {"Soft Clip","Hard Clip","Tube","Diode","Lin Fold","Sin Fold","Bitcrush","Downsamp"}));
    params.push_back (pFloat  ("driveAmount",  "Drive Amt",  0.0f, 1.0f, 0.5f));
    params.push_back (pFloat  ("driveMix",     "Drive Mix",  0.0f, 1.0f, 1.0f));

    // ---- Chorus ----
    params.push_back (pFloat ("chorusEnabled", "Chorus On",  0.0f, 1.0f, 0.0f));
    params.push_back (pFloat ("chorusRate",    "Chorus Rate", 0.1f, 8.0f, 1.5f));
    params.push_back (pFloat ("chorusDepth",   "Chorus Depth", 0.0f, 1.0f, 0.5f));
    params.push_back (pFloat ("chorusFeedback","Chorus FB",    -0.95f, 0.95f, 0.0f));
    params.push_back (pFloat ("chorusMix",     "Chorus Mix",   0.0f, 1.0f, 0.5f));

    // ---- Phaser ----
    params.push_back (pFloat ("phaserEnabled", "Phaser On",   0.0f, 1.0f, 0.0f));
    params.push_back (pFloat ("phaserRate",    "Phaser Rate", 0.1f, 8.0f, 1.0f));
    params.push_back (pFloat ("phaserDepth",   "Phaser Depth",0.0f, 1.0f, 0.5f));
    params.push_back (pFloat ("phaserFeedback","Phaser FB",   -0.95f, 0.95f, 0.3f));
    params.push_back (pFloat ("phaserCentre",  "Phaser Centre", 200.0f, 5000.0f, 1300.0f));
    params.push_back (pFloat ("phaserMix",     "Phaser Mix",  0.0f, 1.0f, 0.5f));

    // ---- Delay ----
    params.push_back (pFloat  ("delayEnabled", "Delay On",    0.0f, 1.0f, 0.0f));
    params.push_back (pFloat  ("delayTime",    "Delay Time",  0.001f, 2.0f, 0.375f));  // seconds
    params.push_back (pFloat  ("delayFeedback","Delay FB",    0.0f, 0.95f, 0.35f));
    params.push_back (pFloat  ("delayMix",     "Delay Mix",   0.0f, 1.0f, 0.3f));
    params.push_back (pFloat  ("delayPingPong","Delay Ping",  0.0f, 1.0f, 0.0f));
    params.push_back (pFloat  ("delaySync",    "Delay Sync",  0.0f, 1.0f, 0.0f));
    params.push_back (pChoice ("delaySyncDiv", "Delay Div",
        {"1/1","1/2","1/4","1/4D","1/4T","1/8","1/8D","1/8T","1/16","1/32"}));

    // ---- Reverb ----
    params.push_back (pFloat ("reverbEnabled", "Reverb On",   0.0f, 1.0f, 0.0f));
    params.push_back (pFloat ("reverbSize",    "Reverb Size", 0.0f, 1.0f, 0.5f));
    params.push_back (pFloat ("reverbDamp",    "Reverb Damp", 0.0f, 1.0f, 0.5f));
    params.push_back (pFloat ("reverbWidth",   "Reverb Width",0.0f, 1.0f, 1.0f));
    params.push_back (pFloat ("reverbMix",     "Reverb Mix",  0.0f, 1.0f, 0.25f));
    params.push_back (pFloat ("reverbFreeze",  "Reverb Freeze",0.0f, 1.0f, 0.0f));

    // ---- Macros (4 routable macro knobs) ----
    for (int m = 1; m <= 4; ++m)
    {
        juce::String ms = juce::String (m);
        params.push_back (pFloat (("macro" + ms).toRawUTF8(),
                                  ("Macro " + ms).toRawUTF8(), 0.0f, 1.0f, 0.0f));
    }

    // ---- Master ----
    params.push_back (pFloat ("masterGain", "Master", 0.0f, 1.0f, 0.8f));

    return { params.begin(), params.end() };
}

NovaSynthProcessor::NovaSynthProcessor()
    : AudioProcessor (BusesProperties()
        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Parameters", createParameterLayout()),
      presetManager (apvts)
{
    for (int i = 0; i < 4; ++i) lfoPointsDirty[i].store (false);
    for (int i = 0; i < NUM_VOICES; ++i)
        synth.addVoice (new SynthVoice());
    synth.addSound (new SynthSound());
}

NovaSynthProcessor::~NovaSynthProcessor() {}

void NovaSynthProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    synth.setCurrentPlaybackSampleRate (sampleRate);
    for (int i = 0; i < synth.getNumVoices(); ++i)
        if (auto* v = dynamic_cast<SynthVoice*>(synth.getVoice(i)))
            v->prepareToPlay (sampleRate, samplesPerBlock, getTotalNumOutputChannels());

    // Prepare multiband compressor
    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32)samplesPerBlock, 2 };

    compInGain.prepare  (spec);
    compOutGain.prepare (spec);
    compInGain.setGainDecibels  (0.f);
    compOutGain.setGainDecibels (0.f);

    static const float crossoverFreqs[2] = { 300.f, 3000.f };

    for (int i = 0; i < 3; ++i)
    {
        bandComps[i].prepare (spec);
        bandComps[i].setAttack   (0.5f);   // ultra-rapide pour OTT
        bandComps[i].setRelease  (30.f);
        bandComps[i].setThreshold(-20.f);
        bandComps[i].setRatio    (4.f);
    }

    for (int i = 0; i < 2; ++i)
    {
        crossoverLP[i].prepare (spec);
        crossoverHP[i].prepare (spec);
        crossoverLP[i].setType (juce::dsp::LinkwitzRileyFilterType::lowpass);
        crossoverHP[i].setType (juce::dsp::LinkwitzRileyFilterType::highpass);
        crossoverLP[i].setCutoffFrequency (crossoverFreqs[i]);
        crossoverHP[i].setCutoffFrequency (crossoverFreqs[i]);
    }

    driveOversampler.initProcessing (static_cast<size_t>(samplesPerBlock));
    // Reset DC blocker and hold state
    for (int ch = 0; ch < 2; ++ch) {
        driveDcX[ch] = driveDcY[ch] = 0.f;
        driveHoldSample[ch] = driveHoldPhase[ch] = 0.f;
    }

    // Modulation FX
    chorusFx.prepare (spec);
    phaserFx.prepare (spec);
    chorusFx.reset();
    phaserFx.reset();

    // Delay lines (max 2 sec)
    juce::dsp::ProcessSpec monoSpec { sampleRate, (juce::uint32) samplesPerBlock, 1 };
    delayLineL.prepare (monoSpec);
    delayLineR.prepare (monoSpec);
    delayLineL.setMaximumDelayInSamples ((int)(sampleRate * 2.5));
    delayLineR.setMaximumDelayInSamples ((int)(sampleRate * 2.5));
    delayLineL.reset();
    delayLineR.reset();
    delayFbStateL = delayFbStateR = 0.f;

    // Reverb
    reverbFx.prepare (spec);
    reverbFx.reset();
}

// Macro helper pour lire un param APVTS
#define PARAM(id) (*apvts.getRawParameterValue(id))

void NovaSynthProcessor::updateVoiceParameters()
{
    for (int i = 0; i < synth.getNumVoices(); ++i)
    {
        auto* v = dynamic_cast<SynthVoice*>(synth.getVoice(i));
        if (!v) continue;

        // Applique les params sur l'oscillateur osc (1 ou 2)
        // getModdedParam() = valeur de base + modulation LFO appliquée
        auto P = [&](const juce::String& id) { return getModdedParam(id); };

        auto applyOsc = [&](Oscillator& osc, const juce::String& prefix)
        {
            osc.waveform   = static_cast<Oscillator::Waveform>((int)PARAM(prefix+"Waveform"));
            osc.warpMode   = static_cast<Oscillator::WarpMode>((int)PARAM(prefix+"WarpMode"));
            osc.warpAmount = P(prefix+"WarpAmt");

            osc.octave  = (int)std::round (PARAM(prefix+"Oct"));
            osc.tune    = P(prefix+"Tune");
            osc.detune  = P(prefix+"Detune");

            osc.level = P(prefix+"Level");
            osc.pan   = P(prefix+"Pan");

            osc.initPhase = PARAM(prefix+"Phase");
            osc.randPhase = PARAM(prefix+"RandPhase");

            osc.unisonVoices = (int)std::round (PARAM(prefix+"UniVoices"));
            osc.unisonDetune = P(prefix+"UniDetune");
            osc.unisonBlend  = P(prefix+"UniBlend");

            osc.enabled = PARAM(prefix+"Enabled") > 0.5f;
        };

        applyOsc (v->osc1, "osc1");
        applyOsc (v->osc2, "osc2");
        applyOsc (v->osc3, "osc3");

        // ENV1
        v->adsr.attack  = P("attack");
        v->adsr.decay   = P("decay");
        v->adsr.sustain = P("sustain");
        v->adsr.release = P("release");
        v->adsr.curveA  = P("env1CurveA");
        v->adsr.curveD  = P("env1CurveD");
        v->adsr.curveR  = P("env1CurveR");

        v->adsr2.attack  = P("env2Attack");
        v->adsr2.decay   = P("env2Decay");
        v->adsr2.sustain = P("env2Sustain");
        v->adsr2.release = P("env2Release");
        v->adsr2.curveA  = P("env2CurveA");
        v->adsr2.curveD  = P("env2CurveD");
        v->adsr2.curveR  = P("env2CurveR");

        v->adsr3.attack  = P("env3Attack");
        v->adsr3.decay   = P("env3Decay");
        v->adsr3.sustain = P("env3Sustain");
        v->adsr3.release = P("env3Release");
        v->adsr3.curveA  = P("env3CurveA");
        v->adsr3.curveD  = P("env3CurveD");
        v->adsr3.curveR  = P("env3CurveR");

        // Filter
        v->filterCutoff    = P("filterCutoff");
        v->filterResonance = P("filterRes");
        v->filterType      = static_cast<SVFilter::Type>((int)PARAM("filterType"));
        v->filterEnvAmount = P("filterEnvAmt");
        v->filterDrive     = P("filterDrive");
        v->filterFat       = P("filterFat");
        v->filterMix       = P("filterMix");
        v->filterPan       = P("filterPan");
        v->filterEnabled   = PARAM("filterEnabled")   > 0.5f;
        v->filterRouteOsc1 = PARAM("filterRouteOsc1") > 0.5f;
        v->filterRouteOsc2 = PARAM("filterRouteOsc2") > 0.5f;
        v->filterRouteOsc3 = PARAM("filterRouteOsc3") > 0.5f;

        // Portamento (mono lead mode)
        bool monoOn = PARAM("monoMode") > 0.5f;
        float portaT = PARAM("portaTime");
        v->portaActive = monoOn;
        if (portaT > 0.0001f && monoOn)
            v->portaCoeff = std::exp (-1.0f / (portaT * (float)getSampleRate()));
        else
            v->portaCoeff = 0.0f;

        // Sub
        v->subLevel   = P("subLevel");
        v->subWave    = (int)PARAM("subWave");
        v->subOctave  = (int)std::round(PARAM("subOct"));
        v->subEnabled = PARAM("subEnabled") > 0.5f;

        // Noise
        v->noiseEnabled = PARAM("noiseEnabled") > 0.5f;
        v->noiseLevel   = P("noiseLevel");
        v->noisePan     = P("noisePan");
        v->noiseType    = (int)PARAM("noiseType");

        // LFOs internes des voix — maintiennent juste la phase (audio)
        v->lfo1.rate  = PARAM("lfo1Rate");
        v->lfo2.rate  = PARAM("lfo2Rate");
    }
}

// ---- Système de modulation ----

float NovaSynthProcessor::getModdedParam (const juce::String& id)
{
    auto range  = apvts.getParameterRange (id);
    float base  = *apvts.getRawParameterValue (id);
    float baseN = range.convertTo0to1 (base);

    const juce::ScopedLock sl (modLock);
    float modSum = 0.0f;
    for (auto& conn : modConnections)
    {
        if (conn.paramID != id) continue;
        modSum += getLfoValue (conn.lfoIndex) * conn.amount * 0.5f;
    }

    float norm = juce::jlimit (0.0f, 1.0f, baseN + modSum);
    return range.convertFrom0to1 (norm);
}

void NovaSynthProcessor::addModConnection (int lfoIdx, const juce::String& id, float amount)
{
    const juce::ScopedLock sl (modLock);
    for (auto& c : modConnections)
        if (c.lfoIndex == lfoIdx && c.paramID == id) { c.amount = amount; return; }
    modConnections.add ({ lfoIdx, id, amount });
}

void NovaSynthProcessor::removeModConnection (int lfoIdx, const juce::String& id)
{
    const juce::ScopedLock sl (modLock);
    for (int i = modConnections.size() - 1; i >= 0; --i)
        if (modConnections[i].lfoIndex == lfoIdx && modConnections[i].paramID == id)
            modConnections.remove (i);
}

void NovaSynthProcessor::clearModSource (int lfoIdx)
{
    const juce::ScopedLock sl (modLock);
    for (int i = modConnections.size() - 1; i >= 0; --i)
        if (modConnections[i].lfoIndex == lfoIdx)
            modConnections.remove (i);
}

void NovaSynthProcessor::setModAmount (int lfoIdx, const juce::String& id, float amount)
{
    const juce::ScopedLock sl (modLock);
    for (auto& c : modConnections)
        if (c.lfoIndex == lfoIdx && c.paramID == id) { c.amount = amount; return; }
}

float NovaSynthProcessor::getModAmount (int lfoIdx, const juce::String& id) const
{
    const juce::ScopedLock sl (modLock);
    for (auto& c : modConnections)
        if (c.lfoIndex == lfoIdx && c.paramID == id) return c.amount;
    return 0.0f;
}

bool NovaSynthProcessor::hasModConnection (const juce::String& id) const
{
    const juce::ScopedLock sl (modLock);
    for (auto& c : modConnections)
        if (c.paramID == id) return true;
    return false;
}

bool NovaSynthProcessor::hasModConnection (int lfoIdx, const juce::String& id) const
{
    const juce::ScopedLock sl (modLock);
    for (auto& c : modConnections)
        if (c.lfoIndex == lfoIdx && c.paramID == id) return true;
    return false;
}

float NovaSynthProcessor::getLfoValue (int idx) const
{
    switch (idx) {
        case 0: return lfo1Value.load();
        case 1: return lfo2Value.load();
        case 2: return lfo3Value.load();
        case 3: return lfo4Value.load();
        case 4: return env2Value.load();  // ENV2 as mod source
        case 5: return env3Value.load();  // ENV3 as mod source
        default: return 0.0f;
    }
}

float NovaSynthProcessor::getLfoPhase (int idx) const
{
    switch (idx) {
        case 0: return lfo1Phase.load();
        case 1: return lfo2Phase.load();
        case 2: return lfo3Phase.load();
        case 3: return lfo4Phase.load();
        default: return 0.0f;
    }
}

void NovaSynthProcessor::resetLfoPhase (int idx)
{
    if (idx >= 0 && idx < 4)
        qLfo[idx].reset();
}

// ---- processBlock ----

void NovaSynthProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                        juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    // Injecter les notes du clavier d'ordinateur
    keyboardState.processNextMidiBuffer (midi, 0, buffer.getNumSamples(), true);

    float sr         = (float)getSampleRate();
    int   numSamples = buffer.getNumSamples();

    // ---- Mode MONO : filtrage MIDI (style 303) ----
    if (PARAM("monoMode") > 0.5f)
    {
        juce::MidiBuffer transformedMidi;
        for (auto meta : midi)
        {
            auto msg = meta.getMessage();
            int  pos = meta.samplePosition;

            if (msg.isNoteOn())
            {
                int note = msg.getNoteNumber();
                monoNoteStack.addIfNotAlreadyThere (note);
                if (monoActiveNote != -1 && monoActiveNote != note)
                    transformedMidi.addEvent (
                        juce::MidiMessage::noteOff (msg.getChannel(), monoActiveNote),
                        pos);
                monoActiveNote = note;
                transformedMidi.addEvent (msg, pos);
            }
            else if (msg.isNoteOff())
            {
                monoNoteStack.removeFirstMatchingValue (msg.getNoteNumber());
                if (monoActiveNote == msg.getNoteNumber())
                {
                    transformedMidi.addEvent (msg, pos);
                    if (!monoNoteStack.isEmpty())
                    {
                        int resume = monoNoteStack.getLast();
                        monoActiveNote = resume;
                        transformedMidi.addEvent (
                            juce::MidiMessage::noteOn (msg.getChannel(), resume, (uint8_t)64),
                            pos);
                    }
                    else
                    {
                        monoActiveNote = -1;
                    }
                }
                else
                {
                    transformedMidi.addEvent (msg, pos);
                }
            }
            else
            {
                transformedMidi.addEvent (msg, pos);
            }
        }
        midi = transformedMidi;
    }
    else
    {
        monoNoteStack.clear();
        monoActiveNote = -1;
    }

    // Détecter les note-on/off pour modes Trig / One-shot
    {
        // Compter les voix actives avant traitement MIDI
        int activeVoicesBefore = 0;
        for (int v = 0; v < synth.getNumVoices(); ++v)
            if (synth.getVoice(v)->isVoiceActive()) activeVoicesBefore++;

        for (auto m : midi)
        {
            auto msg = m.getMessage();
            if (msg.isNoteOn())
            {
                for (int i = 0; i < 4; ++i)
                {
                    int mode = (int)PARAM ("lfo" + juce::String(i+1) + "Mode");
                    if (mode == 1 || mode == 2)   // Trig ou One
                        qLfo[i].reset();
                }
            }
            else if (msg.isNoteOff())
            {
                // Si cette note était la dernière, remettre les LFO Trig au début
                bool wasLastNote = (activeVoicesBefore == 1);
                if (wasLastNote)
                {
                    for (int i = 0; i < 4; ++i)
                    {
                        int mode = (int)PARAM ("lfo" + juce::String(i+1) + "Mode");
                        if (mode == 1 || mode == 2)   // Trig ou One : retour phase 0
                            qLfo[i].phase = 0.f;
                    }
                }
            }
        }
    }

    // Mise à jour des points LFO si l'UI en a changé (copie thread-safe)
    for (int i = 0; i < 4; ++i)
    {
        if (lfoPointsDirty[i].exchange (false))
        {
            const juce::ScopedLock sl (lfoPointsLock);
            lfoAudioPoints[i] = lfoCustomPoints[i];
        }
    }

    // Avancer les LFOs (phase correcte sur numSamples)
    static const char* lfoRateIDs[4]  = {"lfo1Rate","lfo2Rate","lfo3Rate","lfo4Rate"};
    static const char* lfoWaveIDs[4]  = {"lfo1Wave","lfo2Wave","lfo3Wave","lfo4Wave"};
    static const char* lfoModeIDs[4]  = {"lfo1Mode","lfo2Mode","lfo3Mode","lfo4Mode"};
    static const char* lfoShapeIDs[4] = {"lfo1Shape","lfo2Shape","lfo3Shape","lfo4Shape"};
    static const char* lfoSyncIDs[4]  = {"lfo1Sync","lfo2Sync","lfo3Sync","lfo4Sync"};
    static const char* lfoDivIDs[4]   = {"lfo1SyncDiv","lfo2SyncDiv","lfo3SyncDiv","lfo4SyncDiv"};
    // Multipliers from "4/1, 2/1, 1/1, 1/2, 1/4, 1/8, 1/16, 1/32, 1/64" (whole notes)
    static const float divMul[9]   = { 4.f, 2.f, 1.f, 0.5f, 0.25f, 0.125f, 0.0625f, 0.03125f, 0.015625f };

    // Get tempo from host playhead, fallback to 120 BPM
    double bpm = 120.0;
    if (auto* ph = getPlayHead()) {
        if (auto info = ph->getPosition())
            if (auto t = info->getBpm()) bpm = *t;
    }

    for (int i = 0; i < 4; ++i)
    {
        float rate  = PARAM (lfoRateIDs[i]);
        // Tempo sync: override rate based on musical division
        if (PARAM (lfoSyncIDs[i]) > 0.5f) {
            int divIdx = juce::jlimit (0, 8, (int)PARAM (lfoDivIDs[i]));
            // Whole-note period in seconds = 4 * 60/bpm
            float periodSec = (float)(4.0 * 60.0 / bpm) * divMul[divIdx];
            if (periodSec > 0.0001f) rate = 1.0f / periodSec;
        }
        int   wave  = (int)PARAM (lfoWaveIDs[i]);
        int   mode  = (int)PARAM (lfoModeIDs[i]);
        float shape = PARAM (lfoShapeIDs[i]);
        float val;
        if (!lfoAudioPoints[i].empty())
            val = qLfo[i].advance (rate, sr, numSamples, mode, wave, lfoAudioPoints[i]);
        else
            val = qLfo[i].advanceWithShape (rate, sr, numSamples, mode, shape);
        switch (i) {
            case 0: lfo1Value.store(val); lfo1Phase.store(qLfo[i].phase); break;
            case 1: lfo2Value.store(val); lfo2Phase.store(qLfo[i].phase); break;
            case 2: lfo3Value.store(val); lfo3Phase.store(qLfo[i].phase); break;
            case 3: lfo4Value.store(val); lfo4Phase.store(qLfo[i].phase); break;
        }
    }

    updateVoiceParameters();
    synth.renderNextBlock (buffer, midi, 0, numSamples);

    // Read envelope values from first active voice for modulation
    {
        for (int v = 0; v < synth.getNumVoices(); ++v)
        {
            if (auto* sv = dynamic_cast<SynthVoice*>(synth.getVoice(v)))
            {
                if (sv->isVoiceActive())
                {
                    env2Value.store (sv->lastEnv2Val);
                    env3Value.store (sv->lastEnv3Val);
                    break;
                }
            }
        }
    }

    // ================================================================
    // OTT-style Multiband Compressor
    // Three bands (Low/Mid/Hi) with:
    //   - Downward compression (reduce gain above threshold)
    //   - Upward compression   (boost quiet signals toward threshold)
    //   - Per-sample gain smoothing with attack/release envelopes
    //   - Proper gain-reduction metering for the UI (0 = no reduction,
    //     negative dB = reduction; clamped to [-12, 0] for the meter)
    // ================================================================
    if (PARAM("compEnabled") > 0.5f)
    {
        compInGain.setGainDecibels  (PARAM("compInGain"));
        compOutGain.setGainDecibels (PARAM("compOutGain"));

        bandThresh[0] = PARAM("compLowThresh");
        bandThresh[1] = PARAM("compMidThresh");
        bandThresh[2] = PARAM("compHiThresh");
        bandRatio[0]  = PARAM("compLowRatio");
        bandRatio[1]  = PARAM("compMidRatio");
        bandRatio[2]  = PARAM("compHiRatio");

        float compMix = PARAM("compMix");
        float upAmt   = PARAM("compUpward");

        // ---- Save dry signal ----
        juce::AudioBuffer<float> dryBuffer (buffer.getNumChannels(), numSamples);
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            dryBuffer.copyFrom (ch, 0, buffer, ch, 0, numSamples);

        // ---- Input gain ----
        {
            juce::dsp::AudioBlock<float> block (buffer);
            compInGain.process (juce::dsp::ProcessContextReplacing<float> (block));
        }

        // ---- Split into 3 bands via Linkwitz-Riley crossovers ----
        // Crossover 0: 300 Hz   Crossover 1: 3000 Hz
        // Low  = LP0
        // Mid  = HP0 * LP1
        // High = HP1
        // The Linkwitz-Riley crossovers ensure LP + HP = 1 (flat summing).

        juce::AudioBuffer<float> lowBand  (buffer.getNumChannels(), numSamples);
        juce::AudioBuffer<float> midBand  (buffer.getNumChannels(), numSamples);
        juce::AudioBuffer<float> highBand (buffer.getNumChannels(), numSamples);

        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            lowBand .copyFrom (ch, 0, buffer, ch, 0, numSamples);
            midBand .copyFrom (ch, 0, buffer, ch, 0, numSamples);
            highBand.copyFrom (ch, 0, buffer, ch, 0, numSamples);
        }

        { juce::dsp::AudioBlock<float> b (lowBand);
          crossoverLP[0].process (juce::dsp::ProcessContextReplacing<float> (b)); }

        { juce::dsp::AudioBlock<float> b (midBand);
          crossoverHP[0].process (juce::dsp::ProcessContextReplacing<float> (b)); }
        { juce::dsp::AudioBlock<float> b (midBand);
          crossoverLP[1].process (juce::dsp::ProcessContextReplacing<float> (b)); }

        { juce::dsp::AudioBlock<float> b (highBand);
          crossoverHP[1].process (juce::dsp::ProcessContextReplacing<float> (b)); }

        // ---- Per-band OTT processing ----
        // Log-domain gain smoothing (dB) with separate attack / release per direction.
        // Faster attack, slower release → musical, low-distortion compression.
        //   ~2 ms attack, ~80 ms release
        const float attackCoeff  = std::exp (-1.0f / (0.002f * sr));
        const float releaseCoeff = std::exp (-1.0f / (0.080f * sr));
        const float kneeDb = 6.0f;   // soft-knee width (centered on threshold)

        auto processOTTBand = [&](juce::AudioBuffer<float>& buf, int b)
        {
            float compDepth = PARAM("compDepth");
            static const char* gainIds[3] = {"compLowGain","compMidGain","compHiGain"};
            float bandOutGainLin = std::pow(10.0f, PARAM(gainIds[b]) / 20.0f);

            // Track input RMS for visual
            float sumSq = 0.f;
            for (int ch2 = 0; ch2 < buf.getNumChannels(); ++ch2)
                for (int s2 = 0; s2 < buf.getNumSamples(); ++s2) {
                    float v = buf.getSample(ch2, s2);
                    sumSq += v * v;
                }
            float inputRms = std::sqrt(sumSq / (buf.getNumSamples() * buf.getNumChannels() + 1e-12f));
            bandSignalLevel[b].store(inputRms);

            const float thrDb     = bandThresh[b];
            const float ratio     = bandRatio[b];
            const float invRatio  = (ratio > 1.0f) ? (1.0f / ratio) : 1.0f;
            const float halfKnee  = kneeDb * 0.5f;

            float peakGainReduction = 1.0f;   // worst-case linear gain for metering

            for (int ch = 0; ch < buf.getNumChannels(); ++ch)
            {
                auto* d = buf.getWritePointer (ch);
                for (int s = 0; s < buf.getNumSamples(); ++s)
                {
                    float x    = d[s];
                    float xAbs = std::abs (x);
                    float xDb  = (xAbs > 1e-9f) ? 20.0f * std::log10 (xAbs) : -120.0f;

                    // ---- Downward compression with soft-knee ----
                    // Classic soft-knee formula:
                    //   x - T <= -kneeHalf         : no compression
                    //   |x - T|  <= kneeHalf       : quadratic ramp
                    //   x - T >=  kneeHalf         : full ratio
                    float overDb = xDb - thrDb;
                    float reduceDb = 0.0f;
                    if (overDb >  halfKnee)
                        reduceDb = overDb * (1.0f - invRatio);
                    else if (overDb > -halfKnee)
                    {
                        // Smooth polynomial transition — eliminates threshold "click"
                        float t = (overDb + halfKnee) / kneeDb;   // 0..1
                        reduceDb = (1.0f - invRatio) * kneeDb * t * t * 0.5f;
                    }
                    reduceDb *= compDepth;

                    // Log-domain smoothing: envelope tracks -reduceDb (<=0)
                    float targetDown = -reduceDb;
                    float& envDownDb = compEnvDown[b][ch];   // reused as dB state
                    if (targetDown < envDownDb)
                        envDownDb = attackCoeff  * envDownDb + (1.0f - attackCoeff)  * targetDown;
                    else
                        envDownDb = releaseCoeff * envDownDb + (1.0f - releaseCoeff) * targetDown;
                    float gainDownLin = std::pow (10.0f, envDownDb / 20.0f);

                    // ---- Upward compression (OTT-style) ----
                    float targetUpDb = 0.0f;
                    if (upAmt > 0.001f && xDb < thrDb && xDb > -80.0f)
                    {
                        float belowDb = thrDb - xDb;
                        targetUpDb = belowDb * upAmt * (1.0f - invRatio) * compDepth;
                        targetUpDb = juce::jlimit (0.0f, 18.0f, targetUpDb);
                    }
                    float& envUpDb = compEnvUp[b][ch];
                    if (targetUpDb > envUpDb)
                        envUpDb = attackCoeff  * envUpDb + (1.0f - attackCoeff)  * targetUpDb;
                    else
                        envUpDb = releaseCoeff * envUpDb + (1.0f - releaseCoeff) * targetUpDb;
                    float gainUpLin = std::pow (10.0f, envUpDb / 20.0f);

                    float totalGain = gainDownLin * gainUpLin;
                    d[s] = x * totalGain;

                    if (totalGain < peakGainReduction)
                        peakGainReduction = totalGain;
                }
            }

            // Apply per-band output gain
            if (std::abs(bandOutGainLin - 1.0f) > 0.001f)
                buf.applyGain(bandOutGainLin);

            // Store gain reduction for UI meter.
            // Convert linear gain to dB, clamp to [-12, 0], then normalise to [0, 1]
            // where 1.0 = no reduction and 0.0 = -12 dB (full-scale reduction).
            float grDb  = 20.0f * std::log10 (juce::jmax (1e-6f, peakGainReduction));
            grDb        = juce::jlimit (-12.0f, 0.0f, grDb);
            float grNorm = (grDb + 12.0f) / 12.0f;   // 0 = full reduction, 1 = none
            bandGainReduction[b].store (grNorm);
        };

        processOTTBand (lowBand,  0);
        processOTTBand (midBand,  1);
        processOTTBand (highBand, 2);

        // ---- Sum bands back to wet buffer ----
        juce::AudioBuffer<float> wetBuffer (buffer.getNumChannels(), numSamples);
        wetBuffer.clear();
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            wetBuffer.addFrom (ch, 0, lowBand,  ch, 0, numSamples);
            wetBuffer.addFrom (ch, 0, midBand,  ch, 0, numSamples);
            wetBuffer.addFrom (ch, 0, highBand, ch, 0, numSamples);
        }

        // ---- Output gain ----
        {
            juce::dsp::AudioBlock<float> block (wetBuffer);
            compOutGain.process (juce::dsp::ProcessContextReplacing<float> (block));
        }

        // ---- Dry/wet mix ----
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            const auto* wet = wetBuffer.getReadPointer (ch);
            const auto* dry = dryBuffer.getReadPointer (ch);
            auto*       out = buffer.getWritePointer   (ch);
            for (int s = 0; s < numSamples; ++s)
                out[s] = wet[s] * compMix + dry[s] * (1.0f - compMix);
        }
    }
    else
    {
        // Compressor off — reset metering and envelope state
        for (int i = 0; i < 3; ++i)
        {
            bandGainReduction[i].store (1.0f);
            for (int ch = 0; ch < 2; ++ch)
            {
                compEnvDown[i][ch] = 0.0f;
                compEnvUp  [i][ch] = 0.0f;
            }
        }
    }

    buffer.applyGain (PARAM("masterGain"));

    // ================================================================
    // Overdrive / Distortion with 4x oversampling
    // ================================================================
    if (PARAM("driveEnabled") > 0.5f)
    {
        int   driveMode   = (int)PARAM("driveMode");
        float driveAmt    = PARAM("driveAmount");
        float driveMix    = PARAM("driveMix");
        // Map 0..1 to actual drive gain 1..10
        float driveGain   = 1.0f + driveAmt * 9.0f;

        // Save dry signal
        juce::AudioBuffer<float> dryDrive (buffer.getNumChannels(), numSamples);
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            dryDrive.copyFrom (ch, 0, buffer, ch, 0, numSamples);

        // Oversample UP
        juce::dsp::AudioBlock<float> inputBlock (buffer);
        auto oversampledBlock = driveOversampler.processSamplesUp (inputBlock);
        auto oversampledSamples = (int)oversampledBlock.getNumSamples();

        // Waveshaper — applied to the oversampled buffer
        auto applyShaper = [&](float x) -> float
        {
            float in = x * driveGain;
            switch (driveMode)
            {
                case 0: // Soft Clip (tanh)
                {
                    float tg = std::tanh (driveGain);
                    return (tg > 1e-6f) ? std::tanh (in) / tg : x;
                }
                case 1: // Hard Clip
                    return juce::jlimit (-1.0f, 1.0f, in) / std::max (driveGain, 1.0f);
                case 2: // Tube (asymmetric)
                {
                    float bias   = 0.15f;
                    float biased = in + bias * driveGain;
                    float out    = (biased >= 0.0f)
                        ? std::tanh (biased)
                        : std::atan (biased * 0.8f) * (2.0f / juce::MathConstants<float>::pi);
                    return juce::jlimit (-1.0f, 1.0f, out - bias * 0.4f);
                }
                case 3: // Diode
                {
                    if (in > 0.0f)  return  1.0f - std::exp (-in);
                    if (in < 0.0f)  return -(1.0f - std::exp ( in));
                    return 0.0f;
                }
                case 4: // Lin Fold
                {
                    float v = in;
                    for (int iter = 0; iter < 12; ++iter) {
                        if      (v >  1.0f) v = 2.0f - v;
                        else if (v < -1.0f) v = -2.0f - v;
                        else break;
                    }
                    return v;
                }
                case 5: // Sin Fold
                    return std::sin (in * juce::MathConstants<float>::pi * 0.5f);
                case 6: // Bitcrusher
                {
                    float bits   = juce::jlimit (2.0f, 16.0f, 16.0f - driveAmt * 13.0f);
                    float levels = std::pow (2.0f, bits);
                    return std::round (x * levels) / levels;
                }
                case 7: // Downsample (handled per channel below using hold state)
                    return x;
                default: return x;
            }
        };

        for (int ch = 0; ch < (int)oversampledBlock.getNumChannels(); ++ch)
        {
            auto* d = oversampledBlock.getChannelPointer (ch);

            if (driveMode == 7) // Downsample: hold-sample approach
            {
                float holdFactor = 1.0f + driveAmt * 31.0f; // 1x to 32x
                for (int s = 0; s < oversampledSamples; ++s)
                {
                    driveHoldPhase[ch] += 1.0f;
                    if (driveHoldPhase[ch] >= holdFactor) {
                        driveHoldPhase[ch]  -= holdFactor;
                        driveHoldSample[ch]  = d[s];
                    }
                    d[s] = driveHoldSample[ch];
                }
            }
            else
            {
                for (int s = 0; s < oversampledSamples; ++s)
                    d[s] = applyShaper (d[s]);
            }
        }

        // Oversample DOWN
        driveOversampler.processSamplesDown (inputBlock);

        // DC blocking filter after distortion (mandatory for asymmetric modes)
        static constexpr float DC_R = 0.995f;
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            auto* d = buffer.getWritePointer (ch);
            for (int s = 0; s < numSamples; ++s)
            {
                float y       = d[s] - driveDcX[ch] + DC_R * driveDcY[ch];
                driveDcX[ch]  = d[s];
                driveDcY[ch]  = y;
                d[s]          = y;
            }
        }

        // Dry/Wet mix
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            const auto* dry = dryDrive.getReadPointer (ch);
            auto*       out = buffer.getWritePointer  (ch);
            for (int s = 0; s < numSamples; ++s)
                out[s] = out[s] * driveMix + dry[s] * (1.0f - driveMix);
        }
    }

    // ================================================================
    //  Chorus
    // ================================================================
    if (PARAM("chorusEnabled") > 0.5f)
    {
        chorusFx.setRate       (PARAM("chorusRate"));
        chorusFx.setDepth      (PARAM("chorusDepth"));
        chorusFx.setCentreDelay (7.0f);
        chorusFx.setFeedback   (PARAM("chorusFeedback"));
        chorusFx.setMix        (PARAM("chorusMix"));
        juce::dsp::AudioBlock<float> block (buffer);
        juce::dsp::ProcessContextReplacing<float> ctx (block);
        chorusFx.process (ctx);
    }

    // ================================================================
    //  Phaser
    // ================================================================
    if (PARAM("phaserEnabled") > 0.5f)
    {
        phaserFx.setRate          (PARAM("phaserRate"));
        phaserFx.setDepth         (PARAM("phaserDepth"));
        phaserFx.setCentreFrequency (PARAM("phaserCentre"));
        phaserFx.setFeedback      (PARAM("phaserFeedback"));
        phaserFx.setMix           (PARAM("phaserMix"));
        juce::dsp::AudioBlock<float> block (buffer);
        juce::dsp::ProcessContextReplacing<float> ctx (block);
        phaserFx.process (ctx);
    }

    // ================================================================
    //  Delay (stereo, with optional ping-pong and tempo sync)
    // ================================================================
    if (PARAM("delayEnabled") > 0.5f && buffer.getNumChannels() >= 2)
    {
        float delayTime = PARAM("delayTime");
        // Tempo sync
        if (PARAM("delaySync") > 0.5f)
        {
            double bpm = 120.0;
            if (auto* ph = getPlayHead())
            {
                if (auto pos = ph->getPosition())
                    if (auto b = pos->getBpm()) bpm = *b;
            }
            int divIdx = (int) PARAM("delaySyncDiv");
            // {"1/1","1/2","1/4","1/4D","1/4T","1/8","1/8D","1/8T","1/16","1/32"}
            static const float divMul[10] = {
                4.0f, 2.0f, 1.0f, 1.5f, 2.0f/3.0f, 0.5f, 0.75f, 1.0f/3.0f, 0.25f, 0.125f
            };
            float beatSec = (float) (60.0 / bpm);
            delayTime = beatSec * divMul[juce::jlimit (0, 9, divIdx)];
        }
        float fb        = PARAM("delayFeedback");
        float mix       = PARAM("delayMix");
        bool  pingPong  = PARAM("delayPingPong") > 0.5f;

        int samplesDelay = juce::jlimit (1,
            (int)(getSampleRate() * 2.5 - 1),
            (int)(delayTime * (float)getSampleRate()));
        delayLineL.setDelay ((float) samplesDelay);
        delayLineR.setDelay ((float) samplesDelay);

        auto* L = buffer.getWritePointer (0);
        auto* R = buffer.getWritePointer (1);
        for (int s = 0; s < numSamples; ++s)
        {
            float inL = L[s];
            float inR = R[s];

            float outL = delayLineL.popSample (0);
            float outR = delayLineR.popSample (0);

            float feedL, feedR;
            if (pingPong)
            {
                // Cross-feedback left ↔ right
                feedL = inL + outR * fb;
                feedR = inR + outL * fb;
            }
            else
            {
                feedL = inL + outL * fb;
                feedR = inR + outR * fb;
            }
            delayLineL.pushSample (0, feedL);
            delayLineR.pushSample (0, feedR);

            L[s] = inL * (1.0f - mix) + outL * mix;
            R[s] = inR * (1.0f - mix) + outR * mix;
        }
    }

    // ================================================================
    //  Reverb
    // ================================================================
    if (PARAM("reverbEnabled") > 0.5f && buffer.getNumChannels() >= 2)
    {
        juce::Reverb::Parameters rp;
        rp.roomSize   = PARAM("reverbSize");
        rp.damping    = PARAM("reverbDamp");
        rp.wetLevel   = PARAM("reverbMix");
        rp.dryLevel   = 1.0f - PARAM("reverbMix");
        rp.width      = PARAM("reverbWidth");
        rp.freezeMode = PARAM("reverbFreeze");
        reverbFx.setParameters (rp);

        juce::dsp::AudioBlock<float> block (buffer);
        juce::dsp::ProcessContextReplacing<float> ctx (block);
        reverbFx.process (ctx);
    }

    // Limiteur de sortie : soft saturation (tanh) + hard cap à ±0.98
    // Garantit qu'aucun sample ne dépasse jamais ±1.0 (pas de clip numérique).
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        auto* data = buffer.getWritePointer (ch);
        for (int i = 0; i < numSamples; ++i)
        {
            float x = data[i];
            // Soft knee : linéaire jusqu'à ±0.7, puis saturation progressive
            float ax = std::abs (x);
            if (ax > 0.7f)
            {
                float sign = x < 0.0f ? -1.0f : 1.0f;
                // tanh mappe [0.7..inf] vers [0.7..~0.98] de façon douce
                float over = ax - 0.7f;
                float sat  = 0.28f * std::tanh (over * 3.5f);
                x = sign * (0.7f + sat);
            }
            // Hard cap de sécurité absolu
            data[i] = juce::jlimit (-0.98f, 0.98f, x);
        }
    }
}

juce::AudioProcessorEditor* NovaSynthProcessor::createEditor()
{
    return new NovaSynthEditor (*this);
}

void NovaSynthProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, dest);
}

void NovaSynthProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml && xml->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

void NovaSynthProcessor::setLfoPoints (int idx, const std::vector<LfoPoint>& pts)
{
    if (idx < 0 || idx >= 4) return;
    { const juce::ScopedLock sl (lfoPointsLock); lfoCustomPoints[idx] = pts; }
    lfoPointsDirty[idx].store (true);
}

std::vector<LfoPoint> NovaSynthProcessor::getLfoPoints (int idx) const
{
    if (idx < 0 || idx >= 4) return {};
    const juce::ScopedLock sl (lfoPointsLock);
    return lfoCustomPoints[idx];
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new NovaSynthProcessor();
}
