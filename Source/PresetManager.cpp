#include "PresetManager.h"

// ============================================================
//  Presets Factory
//  Valeurs réelles (pas normalisées) — applyPreset() convertit
// ============================================================

// Paramètres à l'état "Init" (tous au défaut)
static const std::vector<PresetManager::ParamValue> kInit = {};

// ---- BASS ----

static const std::vector<PresetManager::ParamValue> kSubBass =
{
    { "osc1Waveform", 3.f },   // Sine
    { "osc1Level",    0.7f },
    { "osc1UniVoices",1.f  },
    { "osc2Level",    0.0f },  // OSC2 muet
    { "osc3Level",    0.0f },
    { "subLevel",     0.9f },
    { "subOct",       -1.f },
    { "subWave",      0.f  },  // Sine sub
    { "filterType",   1.f  },  // LP24
    { "filterCutoff", 180.f},
    { "filterRes",    0.10f},
    { "filterMix",    1.0f },
    { "attack",       0.005f},
    { "decay",        0.20f},
    { "sustain",      0.90f},
    { "release",      0.50f},
    { "monoMode",     0.0f },
};

static const std::vector<PresetManager::ParamValue> kAcidBass =
{
    { "osc1Waveform", 0.f  },   // Saw
    { "osc1Level",    0.90f},
    { "osc1UniVoices",1.f  },
    { "osc2Level",    0.0f },
    { "osc3Level",    0.0f },
    { "subLevel",     0.3f },
    { "filterType",   1.f  },   // LP24
    { "filterCutoff", 700.f},
    { "filterRes",    0.75f},
    { "filterEnvAmt", 0.85f},
    { "filterMix",    1.0f },
    { "attack",       0.002f},
    { "decay",        0.18f},
    { "sustain",      0.0f },
    { "release",      0.08f},
    { "env2Attack",   0.002f},
    { "env2Decay",    0.18f},
    { "env2Sustain",  0.0f },
    { "env2Release",  0.08f},
    { "monoMode",     1.0f },
    { "portaTime",    0.06f},
};

static const std::vector<PresetManager::ParamValue> kReeseBass =
{
    { "osc1Waveform", 0.f  },   // Saw
    { "osc1Level",    0.80f},
    { "osc1Detune",   7.0f },
    { "osc1UniVoices",1.f  },
    { "osc2Waveform", 0.f  },   // Saw
    { "osc2Level",    0.75f},
    { "osc2Detune",   -7.0f},
    { "osc2UniVoices",1.f  },
    { "osc3Level",    0.0f },
    { "filterType",   0.f  },   // LP12
    { "filterCutoff", 1100.f},
    { "filterRes",    0.20f},
    { "filterMix",    1.0f },
    { "attack",       0.005f},
    { "decay",        0.40f},
    { "sustain",      0.80f},
    { "release",      0.30f},
    { "monoMode",     0.0f },
};

// ---- SYNTH / PAD ----

static const std::vector<PresetManager::ParamValue> kSupersawPad =
{
    { "osc1Waveform",  0.f  },  // Saw
    { "osc1Level",     0.80f},
    { "osc1UniVoices", 7.f  },
    { "osc1UniDetune", 0.55f},
    { "osc1UniBlend",  0.85f},
    { "osc2Waveform",  0.f  },  // Saw
    { "osc2Level",     0.65f},
    { "osc2UniVoices", 5.f  },
    { "osc2UniDetune", 0.45f},
    { "osc2UniBlend",  0.75f},
    { "osc3Level",     0.0f },
    { "filterType",    0.f  },  // LP12
    { "filterCutoff",  6500.f},
    { "filterRes",     0.05f},
    { "filterMix",     1.0f },
    { "attack",        0.80f},
    { "decay",         0.30f},
    { "sustain",       0.90f},
    { "release",       1.80f},
    { "monoMode",      0.0f },
};

static const std::vector<PresetManager::ParamValue> kPluck =
{
    { "osc1Waveform",  0.f  },  // Saw
    { "osc1Level",     0.85f},
    { "osc1UniVoices", 1.f  },
    { "osc2Waveform",  0.f  },  // Saw
    { "osc2Level",     0.55f},
    { "osc2Detune",    4.0f },
    { "osc3Level",     0.0f },
    { "filterType",    0.f  },  // LP12
    { "filterCutoff",  4500.f},
    { "filterRes",     0.42f},
    { "filterEnvAmt",  0.75f},
    { "filterMix",     1.0f },
    { "attack",        0.001f},
    { "decay",         0.14f},
    { "sustain",       0.0f },
    { "release",       0.18f},
    { "env2Attack",    0.001f},
    { "env2Decay",     0.14f},
    { "env2Sustain",   0.0f },
    { "env2Release",   0.1f },
    { "monoMode",      0.0f },
};

// ---- LEAD ----

static const std::vector<PresetManager::ParamValue> kSawLead =
{
    { "osc1Waveform",  0.f  },  // Saw
    { "osc1Level",     0.90f},
    { "osc1UniVoices", 4.f  },
    { "osc1UniDetune", 0.30f},
    { "osc1UniBlend",  0.60f},
    { "osc2Waveform",  0.f  },
    { "osc2Level",     0.55f},
    { "osc2UniVoices", 2.f  },
    { "osc2UniDetune", 0.20f},
    { "osc3Level",     0.0f },
    { "filterType",    0.f  },  // LP12
    { "filterCutoff",  5000.f},
    { "filterRes",     0.15f},
    { "filterMix",     1.0f },
    { "attack",        0.005f},
    { "decay",         0.10f},
    { "sustain",       0.80f},
    { "release",       0.30f},
    { "monoMode",      1.0f },
    { "portaTime",     0.04f},
};

static const std::vector<PresetManager::ParamValue> kSquareLead =
{
    { "osc1Waveform",  1.f  },  // Square
    { "osc1WarpMode",  6.f  },  // PWM
    { "osc1WarpAmt",   0.15f},
    { "osc1Level",     0.85f},
    { "osc1UniVoices", 1.f  },
    { "osc2Waveform",  1.f  },  // Square
    { "osc2Level",     0.45f},
    { "osc2Detune",    -3.0f},
    { "osc3Level",     0.0f },
    { "filterType",    0.f  },  // LP12
    { "filterCutoff",  8000.f},
    { "filterRes",     0.10f},
    { "filterMix",     1.0f },
    { "attack",        0.005f},
    { "decay",         0.08f},
    { "sustain",       0.82f},
    { "release",       0.20f},
    { "monoMode",      1.0f },
    { "portaTime",     0.03f},
};

// ============================================================
//  Table des presets factory
// ============================================================
const PresetManager::FactoryPreset PresetManager::factoryPresets[NUM_FACTORY] =
{
    { "Init",            kInit       },
    { "Sub Bass",        kSubBass    },
    { "Acid 303",        kAcidBass   },
    { "Reese Bass",      kReeseBass  },
    { "Supersaw Pad",    kSupersawPad},
    { "Pluck",           kPluck      },
    { "Saw Lead",        kSawLead    },
    { "Square Lead",     kSquareLead },
    // Preset 9 : Init (alias pour fermer la liste proprement)
    { "Init (Copy)",     kInit       },
};

// ============================================================
//  Implémentation
// ============================================================

PresetManager::PresetManager (juce::AudioProcessorValueTreeState& a)
    : apvts (a) {}

juce::File PresetManager::getPresetsDir() const
{
    auto dir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                   .getChildFile ("Octopus").getChildFile ("Presets");
    dir.createDirectory();
    return dir;
}

juce::StringArray PresetManager::getUserPresetNames() const
{
    juce::StringArray names;
    for (auto& f : getPresetsDir().findChildFiles (juce::File::findFiles, false, "*.xml"))
        names.add (f.getFileNameWithoutExtension());
    names.sort (false);
    return names;
}

juce::StringArray PresetManager::getPresetNames() const
{
    juce::StringArray names;
    for (int i = 0; i < NUM_FACTORY; ++i)
        names.add (juce::String (factoryPresets[i].name));
    for (auto& n : getUserPresetNames())
        names.add (n);
    return names;
}

int PresetManager::getNumPresets() const
{
    return NUM_FACTORY + getUserPresetNames().size();
}

void PresetManager::resetToDefaults()
{
    for (auto* param : apvts.processor.getParameters())
        param->setValueNotifyingHost (param->getDefaultValue());
}

void PresetManager::applyPreset (const std::vector<ParamValue>& params)
{
    resetToDefaults();
    for (auto& pv : params)
    {
        auto* param = apvts.getParameter (pv.id);
        if (!param) continue;
        const auto range = apvts.getParameterRange (pv.id);
        param->setValueNotifyingHost (range.convertTo0to1 (pv.value));
    }
}

void PresetManager::loadPreset (int index)
{
    currentIndex = index;

    if (index < NUM_FACTORY)
    {
        applyPreset (factoryPresets[index].params);
        return;
    }

    // Preset utilisateur
    int userIdx = index - NUM_FACTORY;
    auto userNames = getUserPresetNames();
    if (userIdx >= userNames.size()) return;

    auto file = getPresetsDir().getChildFile (userNames[userIdx] + ".xml");
    if (!file.existsAsFile()) return;

    auto xml = juce::XmlDocument::parse (file);
    if (xml)
    {
        auto state = juce::ValueTree::fromXml (*xml);
        apvts.replaceState (state);
    }
}

void PresetManager::savePreset (const juce::String& name)
{
    auto state = apvts.copyState();
    if (auto xml = state.createXml())
    {
        auto file = getPresetsDir().getChildFile (name + ".xml");
        xml->writeTo (file);
    }
}

void PresetManager::loadNext()
{
    int n = getNumPresets();
    loadPreset ((currentIndex + 1) % n);
}

void PresetManager::loadPrev()
{
    int n = getNumPresets();
    loadPreset ((currentIndex - 1 + n) % n);
}
