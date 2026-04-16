#pragma once
#include <juce_audio_utils/juce_audio_utils.h>

// ============================================================
//  PresetManager — gestion des presets pour Octopus
// ============================================================
class PresetManager
{
public:
    explicit PresetManager (juce::AudioProcessorValueTreeState& apvts);

    // Chargement / sauvegarde
    void loadPreset  (int index);
    void savePreset  (const juce::String& name);

    // Noms & index
    juce::StringArray getPresetNames()   const;
    int  getNumPresets()    const;
    int  getNumFactory()    const { return NUM_FACTORY; }
    int  getCurrentIndex()  const { return currentIndex; }
    void setCurrentIndex (int i) { currentIndex = i; }

    void loadNext();
    void loadPrev();

    static constexpr int NUM_FACTORY = 9;

    // Public pour que PresetManager.cpp puisse définir les tables factory
    struct ParamValue    { const char* id; float value; };
    struct FactoryPreset { const char* name; std::vector<ParamValue> params; };

    static const FactoryPreset factoryPresets[NUM_FACTORY];

private:
    juce::AudioProcessorValueTreeState& apvts;
    int currentIndex { 0 };

    juce::File getPresetsDir() const;
    juce::StringArray getUserPresetNames() const;
    void applyPreset (const std::vector<ParamValue>& params);
    void resetToDefaults();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PresetManager)
};
