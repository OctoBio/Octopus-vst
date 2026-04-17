#include "PluginEditor.h"
#include "PluginProcessor.h"

// ============================================================
//  EnvLfoPanel — implémentation
// ============================================================

EnvLfoPanel::EnvLfoPanel (NovaSynthProcessor& proc,
                           juce::AudioProcessorValueTreeState& apvts)
    : proc(proc), apvts(apvts)
{
    static const char* tabNames[7] = { "ENV 1", "ENV 2", "ENV 3", "LFO 1", "LFO 2", "LFO 3", "LFO 4" };

    for (int i = 0; i < 7; ++i)
    {
        juce::Colour c = tabColour (i);
        tabBtns[i].setButtonText (tabNames[i]);
        tabBtns[i].setClickingTogglesState (false);
        tabBtns[i].setColour (juce::TextButton::buttonColourId,  NS_BG2);
        tabBtns[i].setColour (juce::TextButton::buttonOnColourId,NS_BG2);
        tabBtns[i].setColour (juce::TextButton::textColourOffId, NS_TEXT2.withAlpha (0.6f));
        tabBtns[i].setColour (juce::TextButton::textColourOnId,  c);
        // First 3 buttons → ENV section, last 4 → LFO section
        if (i < 3) {
            tabBtns[i].onClick = [this, i]() { showEnvTab (i); };
            // ENV2/ENV3 are draggable as mod sources (ENV1 hardwired to amplitude)
            if (i >= 1)
                tabBtns[i].dragPayload = "ENV:" + juce::String(i);  // 1=ENV2 mod idx 4, 2=ENV3 idx 5
        }
        else {
            tabBtns[i].onClick = [this, i]() { showLfoTab (i - 3); };
            // LFO tabs draggable for routing
            tabBtns[i].dragPayload = "LFO:" + juce::String(i - 3);
        }
        addAndMakeVisible (tabBtns[i]);
    }

    // ---- ENV 1 ----
    envDisplay[0] = std::make_unique<AdsrDisplay> (apvts,
        "attack","decay","sustain","release", NS_CYAN);
    envDisplay[0]->envIndex = 0;

    static const char* e1ids[4]  = { "attack","decay","sustain","release" };
    static const char* elabels[4]= { "ATK","DEC","SUS","REL" };
    for (int k = 0; k < 4; ++k) {
        envKnob[0][k] = std::make_unique<LabelledKnob> (elabels[k], apvts, e1ids[k], NS_CYAN);
        addAndMakeVisible (*envKnob[0][k]);
    }
    addAndMakeVisible (*envDisplay[0]);

    // ---- ENV 2 ----
    envDisplay[1] = std::make_unique<AdsrDisplay> (apvts,
        "env2Attack","env2Decay","env2Sustain","env2Release", NS_ORANGE);
    envDisplay[1]->envIndex = 1;

    static const char* e2ids[4] = { "env2Attack","env2Decay","env2Sustain","env2Release" };
    for (int k = 0; k < 4; ++k) {
        envKnob[1][k] = std::make_unique<LabelledKnob> (elabels[k], apvts, e2ids[k], NS_ORANGE);
        addAndMakeVisible (*envKnob[1][k]);
    }
    addAndMakeVisible (*envDisplay[1]);

    // ---- ENV 3 ----
    envDisplay[2] = std::make_unique<AdsrDisplay> (apvts,
        "env3Attack","env3Decay","env3Sustain","env3Release", NS_GREEN);
    envDisplay[2]->envIndex = 2;

    static const char* e3ids[4] = { "env3Attack","env3Decay","env3Sustain","env3Release" };
    for (int k = 0; k < 4; ++k) {
        envKnob[2][k] = std::make_unique<LabelledKnob> (elabels[k], apvts, e3ids[k], NS_GREEN);
        addAndMakeVisible (*envKnob[2][k]);
    }
    addAndMakeVisible (*envDisplay[2]);

    // ---- LFOs 1-4 ----
    static const char* modeLabels[3] = { "FREE", "TRIG", "ONE" };

    for (int i = 0; i < 4; ++i)
    {
        juce::Colour lc = lfoColour (i);

        lfoDisplay[i] = std::make_unique<LfoCustomDisplay> (proc, i, lc);
        addAndMakeVisible (*lfoDisplay[i]);

        auto rateID  = "lfo" + juce::String(i + 1) + "Rate";
        auto shapeID = "lfo" + juce::String(i + 1) + "Shape";
        lfoRateKnob[i]  = std::make_unique<LabelledKnob> ("RATE",  apvts, rateID,  lc);
        lfoShapeKnob[i] = std::make_unique<LabelledKnob> ("SHAPE", apvts, shapeID, lc);
        addAndMakeVisible (*lfoRateKnob[i]);
        addAndMakeVisible (*lfoShapeKnob[i]);

        // Mode buttons (FREE / TRIG / ONE)
        auto modeID = "lfo" + juce::String(i+1) + "Mode";
        apvts.addParameterListener (modeID, this);
        currentLfoMode[i] = (int)*apvts.getRawParameterValue (modeID);

        for (int m = 0; m < 3; ++m)
        {
            lfoModeBtns[i][m].setButtonText (modeLabels[m]);
            lfoModeBtns[i][m].setClickingTogglesState (false);
            lfoModeBtns[i][m].onClick = [this, i, m]()
            {
                auto mid = "lfo" + juce::String(i+1) + "Mode";
                if (auto* p = this->apvts.getParameter (mid))
                    p->setValueNotifyingHost ((float)m / 2.0f);
            };
            addAndMakeVisible (lfoModeBtns[i][m]);
        }
        updateModeButtonStates (i);

        // BPM sync toggle — when on, the RATE knob snaps to musical divisions
        auto syncID = "lfo" + juce::String(i+1) + "Sync";
        auto divID  = "lfo" + juce::String(i+1) + "SyncDiv";

        // Setup rate knob's text display: shows division when sync on, Hz otherwise
        if (lfoRateKnob[i])
        {
            auto& s = lfoRateKnob[i]->slider;
            s.textFromValueFunction = [this, i, syncID](double v) -> juce::String
            {
                static const char* divLabels[9] = { "4/1","2/1","1/1","1/2","1/4","1/8","1/16","1/32","1/64" };
                if (this->apvts.getRawParameterValue (syncID) != nullptr
                    && *this->apvts.getRawParameterValue (syncID) > 0.5f)
                {
                    auto& sl = lfoRateKnob[i]->slider;
                    double n = sl.getNormalisableRange().convertTo0to1 (v);
                    int    d = juce::jlimit (0, 8, (int)std::round (n * 8.0));
                    return divLabels[d];
                }
                if (v < 1.0) return juce::String (v, 2) + " Hz";
                return juce::String (v, 1) + " Hz";
            };
            // When sync is on, also push div index to the SyncDiv param
            s.onValueChange = [this, i, syncID, divID]()
            {
                if (lfoRateKnob[i]) lfoRateKnob[i]->repaint();
                if (this->apvts.getRawParameterValue (syncID) != nullptr
                    && *this->apvts.getRawParameterValue (syncID) > 0.5f)
                {
                    auto& sl = lfoRateKnob[i]->slider;
                    double n = sl.getNormalisableRange().convertTo0to1 (sl.getValue());
                    int    d = juce::jlimit (0, 8, (int)std::round (n * 8.0));
                    if (auto* p = this->apvts.getParameter (divID))
                        p->setValueNotifyingHost (p->convertTo0to1 ((float)d));
                }
            };
        }

        lfoSyncBtn[i].setButtonText ("BPM");
        lfoSyncBtn[i].setClickingTogglesState (true);
        lfoSyncBtn[i].setToggleState (*apvts.getRawParameterValue (syncID) > 0.5f,
                                      juce::dontSendNotification);
        lfoSyncBtn[i].setColour (juce::TextButton::buttonColourId,   juce::Colour(0xff0a0a14));
        lfoSyncBtn[i].setColour (juce::TextButton::buttonOnColourId, lc.withAlpha(0.45f));
        lfoSyncBtn[i].setColour (juce::TextButton::textColourOffId,  lc.withAlpha(0.5f));
        lfoSyncBtn[i].setColour (juce::TextButton::textColourOnId,   lc);
        lfoSyncBtn[i].onClick = [this, i, syncID]()
        {
            float v = lfoSyncBtn[i].getToggleState() ? 1.0f : 0.0f;
            if (auto* p = this->apvts.getParameter (syncID))
                p->setValueNotifyingHost (v);
            // Refresh rate knob display
            if (lfoRateKnob[i]) {
                lfoRateKnob[i]->slider.updateText();
                lfoRateKnob[i]->repaint();
            }
        };
        addAndMakeVisible (lfoSyncBtn[i]);
    }

    showEnvTab (0);
    showLfoTab (0);
}

EnvLfoPanel::~EnvLfoPanel()
{
    for (int i = 0; i < 4; ++i)
        apvts.removeParameterListener ("lfo" + juce::String(i+1) + "Mode", this);
}

void EnvLfoPanel::setProcessorForKnobs (NovaSynthProcessor* p)
{
    for (int e = 0; e < 3; ++e) {
        for (int k = 0; k < 4; ++k)
            if (envKnob[e][k]) envKnob[e][k]->setProcessor (p);
        if (envDisplay[e]) envDisplay[e]->setProcessor (p);
    }
    for (int l = 0; l < 4; ++l) {
        if (lfoRateKnob[l])  lfoRateKnob[l]->setProcessor (p);
        if (lfoShapeKnob[l]) lfoShapeKnob[l]->setProcessor (p);
    }
}

void EnvLfoPanel::parameterChanged (const juce::String& id, float v)
{
    for (int i = 0; i < 4; ++i)
    {
        if (id == "lfo" + juce::String(i+1) + "Mode")
        {
            currentLfoMode[i] = (int)std::round (v * 2.0f);
            juce::MessageManager::callAsync ([this, i]() {
                updateModeButtonStates (i);
                repaint();
            });
            return;
        }
    }
}

void EnvLfoPanel::updateModeButtonStates (int lfoIdx)
{
    juce::Colour c = lfoColour (lfoIdx);
    for (int m = 0; m < 3; ++m)
    {
        bool active = (m == currentLfoMode[lfoIdx]);
        lfoModeBtns[lfoIdx][m].setColour (juce::TextButton::buttonColourId,
            active ? c.withAlpha(0.25f) : juce::Colour(0xff0f0f20));
        lfoModeBtns[lfoIdx][m].setColour (juce::TextButton::textColourOffId,
            active ? c : c.withAlpha(0.40f));
        lfoModeBtns[lfoIdx][m].setColour (juce::TextButton::buttonOnColourId,
            c.withAlpha(0.25f));
    }
}

void EnvLfoPanel::showTab (int idx)
{
    // Legacy: delegate to env or lfo tab switch
    if (idx < 3) showEnvTab (idx);
    else         showLfoTab (idx - 3);
}

void EnvLfoPanel::showEnvTab (int idx)
{
    currentEnvTab = idx;
    currentTab    = idx;  // keep for paint() colour

    // Update ENV tab button colours
    for (int i = 0; i < 3; ++i)
    {
        juce::Colour c = tabColour (i);
        bool active = (i == idx);
        tabBtns[i].setColour (juce::TextButton::buttonColourId,
            active ? c.withAlpha(0.15f) : juce::Colour(0xff0a0a16));
        tabBtns[i].setColour (juce::TextButton::textColourOffId,
            active ? c : c.withAlpha(0.40f));
    }

    // Show/hide ENV content
    for (int e = 0; e < 3; ++e)
    {
        bool show = (e == idx);
        if (envDisplay[e]) envDisplay[e]->setVisible (show);
        for (int k = 0; k < 4; ++k)
            if (envKnob[e][k]) envKnob[e][k]->setVisible (show);
    }
    resized();
    repaint();
}

void EnvLfoPanel::showLfoTab (int idx)
{
    currentLfoTab = idx;
    currentTab    = idx + 3;  // keep for paint() colour

    // Update LFO tab button colours
    for (int i = 0; i < 4; ++i)
    {
        juce::Colour c = tabColour (i + 3);
        bool active = (i == idx);
        tabBtns[i + 3].setColour (juce::TextButton::buttonColourId,
            active ? c.withAlpha(0.15f) : juce::Colour(0xff0a0a16));
        tabBtns[i + 3].setColour (juce::TextButton::textColourOffId,
            active ? c : c.withAlpha(0.40f));
    }

    // Show/hide LFO content
    for (int l = 0; l < 4; ++l)
    {
        bool show = (l == idx);
        if (lfoDisplay[l])   lfoDisplay[l]->setVisible (show);
        if (lfoRateKnob[l])  lfoRateKnob[l]->setVisible (show);
        if (lfoShapeKnob[l]) lfoShapeKnob[l]->setVisible (show);
        for (int m = 0; m < 3; ++m) lfoModeBtns[l][m].setVisible (show);
        lfoSyncBtn[l].setVisible (show);
    }

    resized();
    repaint();
}

void EnvLfoPanel::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();

    // Panel background
    g.setColour (NS_BG2);
    g.fillRoundedRectangle (b, 6.0f);
    g.setColour (NS_BORDER);
    g.drawRoundedRectangle (b.reduced(0.5f), 6.0f, 1.0f);

    int totalW = getWidth();
    int envW   = totalW * 45 / 100;
    int lfoW   = totalW - envW;

    // ENV section subtle border
    auto envSection = juce::Rectangle<float>(0.f, 0.f, (float)envW, b.getHeight());
    juce::Colour envC = tabColour (currentEnvTab);
    g.setColour (envC.withAlpha (0.12f));
    g.drawRoundedRectangle (envSection.reduced(0.5f), 6.0f, 1.0f);

    // LFO section subtle border
    auto lfoSection = juce::Rectangle<float>((float)envW, 0.f, (float)lfoW, b.getHeight());
    juce::Colour lfoC = tabColour (currentLfoTab + 3);
    g.setColour (lfoC.withAlpha (0.12f));
    g.drawRoundedRectangle (lfoSection.reduced(0.5f), 6.0f, 1.0f);

    // Active tab indicators
    int tabH = 24;
    int envTabW = envW / 3;
    g.setColour (envC);
    g.fillRect ((float)(currentEnvTab * envTabW), (float)(tabH - 2), (float)envTabW, 2.0f);

    int lfoTabW = lfoW / 4;
    g.setColour (lfoC);
    g.fillRect ((float)(envW + currentLfoTab * lfoTabW), (float)(tabH - 2), (float)lfoTabW, 2.0f);
}

void EnvLfoPanel::resized()
{
    auto b = getLocalBounds();
    int tabH = 24;

    // ---- Split into ENV (45%) and LFO (55%) sections ----
    auto envBounds = b.removeFromLeft (b.getWidth() * 45 / 100);
    auto lfoBounds = b;  // remaining

    // ---- ENV section ----
    {
        auto tabRow = envBounds.removeFromTop (tabH);
        int tabW = tabRow.getWidth() / 3;
        for (int i = 0; i < 3; ++i)
            tabBtns[i].setBounds (i < 2 ? tabRow.removeFromLeft (tabW) : tabRow);

        auto content = envBounds.reduced (5, 4);
        int e = currentEnvTab;

        int dispH = content.getHeight() * 38 / 100;
        if (envDisplay[e] && envDisplay[e]->isVisible())
            envDisplay[e]->setBounds (content.removeFromTop (dispH));
        content.removeFromTop (4);
        int kw = content.getWidth() / 4;
        for (int k = 0; k < 4; ++k)
            if (envKnob[e][k] && envKnob[e][k]->isVisible())
                envKnob[e][k]->setBounds (k < 3 ? content.removeFromLeft(kw) : content);
    }

    // ---- LFO section ----
    {
        auto tabRow = lfoBounds.removeFromTop (tabH);
        int tabW = tabRow.getWidth() / 4;
        for (int i = 0; i < 4; ++i)
            tabBtns[i + 3].setBounds (i < 3 ? tabRow.removeFromLeft (tabW) : tabRow);

        auto content = lfoBounds.reduced (5, 4);
        int l = currentLfoTab;

        // Top row: [FREE TRIG ONE] mode buttons + [BPM] sync toggle on right
        auto modeRow = content.removeFromTop (22);
        auto syncArea = modeRow.removeFromRight (44);
        lfoSyncBtn[l].setBounds (syncArea.reduced (1, 0));
        int modeW = modeRow.getWidth() / 3;
        for (int m = 0; m < 3; ++m)
            lfoModeBtns[l][m].setBounds (m < 2 ? modeRow.removeFromLeft (modeW) : modeRow);
        content.removeFromTop (3);

        // LFO Display (65% of remaining height)
        int dispH = content.getHeight() * 65 / 100;
        if (lfoDisplay[l] && lfoDisplay[l]->isVisible())
            lfoDisplay[l]->setBounds (content.removeFromTop (dispH));
        content.removeFromTop (3);

        // RATE + SHAPE knobs side by side
        int kw = content.getWidth() / 2;
        if (lfoRateKnob[l] && lfoRateKnob[l]->isVisible())
            lfoRateKnob[l]->setBounds (content.removeFromLeft (kw));
        if (lfoShapeKnob[l] && lfoShapeKnob[l]->isVisible())
            lfoShapeKnob[l]->setBounds (content);
    }
}

// ============================================================
//  NovaSynthEditor
// ============================================================

NovaSynthEditor::NovaSynthEditor (NovaSynthProcessor& p)
    : AudioProcessorEditor (&p), processor (p),
      osc1Panel   (p.apvts, 1, NS_VIOLET),
      osc2Panel   (p.apvts, 2, NS_PINK),
      osc3Panel   (p.apvts, 3, NS_CYAN),
      filterCurve (p.apvts),
      filterType   ("TYPE",   p.apvts, "filterType", {"LP 6","LP 12","LP 24","HP 12","HP 24","BP","Notch","Comb"}),
      filterCutoff ("CUT",    p.apvts, "filterCutoff", NS_GREEN),
      filterRes    ("RES",    p.apvts, "filterRes",    NS_GREEN),
      filterEnvAmt ("ENV",    p.apvts, "filterEnvAmt", NS_ORANGE),
      filterDrive  ("DRIVE",  p.apvts, "filterDrive",  NS_GREEN),
      filterFat    ("FAT",    p.apvts, "filterFat",    NS_GREEN),
      filterMix    ("MIX",    p.apvts, "filterMix",    NS_GREEN),
      filterPan    ("PAN",    p.apvts, "filterPan",    NS_GREEN),
      subLevel     ("LEVEL",  p.apvts, "subLevel",     NS_VIOLET),
      subOct       ("OCT",    p.apvts, "subOct",       NS_VIOLET),
      subWave      ("WAVE",   p.apvts, "subWave",      {"Sine","Triangle","Saw","Square"}),
      glideKnob    ("GLIDE",  p.apvts, "portaTime",    NS_PINK),
      noiseLevel   ("LEVEL",  p.apvts, "noiseLevel",   NS_CYAN),
      noisePan     ("PAN",    p.apvts, "noisePan",     NS_CYAN),
      noiseTypeCombo("TYPE",  p.apvts, "noiseType",    {"White","Pink","Brown"}),
      envLfoPanel  (p, p.apvts),
      masterGain   ("MASTER", p.apvts, "masterGain",   NS_CYAN)
{
    auto addAll = [this](auto&... c) { (addAndMakeVisible(c), ...); };
    addAll (osc1Panel, osc2Panel, osc3Panel,
            filterCurve, filterType,
            filterCutoff, filterRes, filterEnvAmt,
            filterDrive, filterFat, filterMix, filterPan,
            subLevel, subOct, subWave, glideKnob,
            noiseLevel, noisePan, noiseTypeCombo,
            envLfoPanel, masterGain,
            presetCombo, presetSaveBtn, presetPrevBtn, presetNextBtn);

    // Bouton ON/OFF filtre
    filterEnableBtn.setButtonText ("ON");
    filterEnableBtn.setClickingTogglesState (true);
    filterEnableBtn.setColour (juce::TextButton::buttonColourId,   juce::Colour(0xff141428));
    filterEnableBtn.setColour (juce::TextButton::buttonOnColourId, NS_GREEN.withAlpha(0.4f));
    filterEnableBtn.setColour (juce::TextButton::textColourOffId,  juce::Colour(0xff6070a0));
    filterEnableBtn.setColour (juce::TextButton::textColourOnId,   NS_GREEN);
    filterEnableBtn.setToggleState (true, juce::dontSendNotification);
    filterEnableBtn.onClick = [this, &p]() {
        float v = filterEnableBtn.getToggleState() ? 1.0f : 0.0f;
        if (auto* param = p.apvts.getParameter ("filterEnabled"))
            param->setValueNotifyingHost (v);
    };
    addAndMakeVisible (filterEnableBtn);

    // Sub ON/OFF button
    subEnableBtn.setButtonText ("ON");
    subEnableBtn.setClickingTogglesState (true);
    subEnableBtn.setColour (juce::TextButton::buttonColourId,   juce::Colour(0xff141428));
    subEnableBtn.setColour (juce::TextButton::buttonOnColourId, NS_VIOLET.withAlpha(0.4f));
    subEnableBtn.setColour (juce::TextButton::textColourOffId,  juce::Colour(0xff6070a0));
    subEnableBtn.setColour (juce::TextButton::textColourOnId,   NS_VIOLET);
    subEnableBtn.setToggleState (true, juce::dontSendNotification);
    subEnableBtn.onClick = [this, &p]() {
        float v = subEnableBtn.getToggleState() ? 1.0f : 0.0f;
        if (auto* param = p.apvts.getParameter ("subEnabled"))
            param->setValueNotifyingHost (v);
    };
    addAndMakeVisible (subEnableBtn);

    // Noise ON/OFF button
    noiseEnableBtn.setButtonText ("ON");
    noiseEnableBtn.setClickingTogglesState (true);
    noiseEnableBtn.setColour (juce::TextButton::buttonColourId,   juce::Colour(0xff141428));
    noiseEnableBtn.setColour (juce::TextButton::buttonOnColourId, NS_CYAN.withAlpha(0.4f));
    noiseEnableBtn.setColour (juce::TextButton::textColourOffId,  juce::Colour(0xff6070a0));
    noiseEnableBtn.setColour (juce::TextButton::textColourOnId,   NS_CYAN);
    noiseEnableBtn.setToggleState (false, juce::dontSendNotification);
    noiseEnableBtn.onClick = [this, &p]() {
        float v = noiseEnableBtn.getToggleState() ? 1.0f : 0.0f;
        if (auto* param = p.apvts.getParameter ("noiseEnabled"))
            param->setValueNotifyingHost (v);
    };
    addAndMakeVisible (noiseEnableBtn);

    // Boutons routing OSC1 / OSC2
    auto setupRouteBtn = [&](juce::TextButton& btn, const char* label, const char* paramID)
    {
        btn.setButtonText (label);
        btn.setClickingTogglesState (true);
        btn.setColour (juce::TextButton::buttonColourId,   juce::Colour(0xff141428));
        btn.setColour (juce::TextButton::buttonOnColourId, NS_GREEN.withAlpha(0.35f));
        btn.setColour (juce::TextButton::textColourOffId,  juce::Colour(0xff6070a0));
        btn.setColour (juce::TextButton::textColourOnId,   NS_GREEN);
        btn.setToggleState (true, juce::dontSendNotification);
        btn.onClick = [&btn, &p, paramID]() {
            float v = btn.getToggleState() ? 1.f : 0.f;
            if (auto* param = p.apvts.getParameter (paramID))
                param->setValueNotifyingHost (v);
        };
        addAndMakeVisible (btn);
    };
    setupRouteBtn (filterRouteOsc1Btn, "OSC1", "filterRouteOsc1");
    setupRouteBtn (filterRouteOsc2Btn, "OSC2", "filterRouteOsc2");
    setupRouteBtn (filterRouteOsc3Btn, "OSC3", "filterRouteOsc3");

    // Bouton MONO
    monoBtn.setButtonText ("MONO");
    monoBtn.setClickingTogglesState (true);
    monoBtn.setColour (juce::TextButton::buttonColourId,   juce::Colour(0xff0d0d20));
    monoBtn.setColour (juce::TextButton::buttonOnColourId, NS_PINK.withAlpha(0.40f));
    monoBtn.setColour (juce::TextButton::textColourOffId,  juce::Colour(0xff5060a0));
    monoBtn.setColour (juce::TextButton::textColourOnId,   NS_PINK);
    monoBtn.onClick = [this, &p] {
        float v = monoBtn.getToggleState() ? 1.f : 0.f;
        if (auto* param = p.apvts.getParameter ("monoMode"))
            param->setValueNotifyingHost (v);
    };
    addAndMakeVisible (monoBtn);

    // Preset bar
    presetCombo.setColour (juce::ComboBox::backgroundColourId,   juce::Colour(0xff0a0a1a));
    presetCombo.setColour (juce::ComboBox::outlineColourId,       NS_VIOLET.withAlpha(0.4f));
    presetCombo.setColour (juce::ComboBox::textColourId,          juce::Colour(0xffccd0f0));
    presetCombo.setColour (juce::ComboBox::arrowColourId,         NS_VIOLET.withAlpha(0.7f));
    refreshPresetCombo();
    presetCombo.onChange = [this, &p] {
        int idx = presetCombo.getSelectedItemIndex();
        if (idx >= 0)
            p.presetManager.loadPreset (idx);
    };

    auto setupPresetBtn = [](juce::TextButton& btn, const char* txt) {
        btn.setButtonText (txt);
        btn.setColour (juce::TextButton::buttonColourId,  juce::Colour(0xff0a0a1a));
        btn.setColour (juce::TextButton::textColourOffId, juce::Colour(0xff7080b0));
    };
    setupPresetBtn (presetPrevBtn, "<");
    setupPresetBtn (presetNextBtn, ">");
    setupPresetBtn (presetSaveBtn, "SAVE");

    presetPrevBtn.onClick = [this, &p] {
        p.presetManager.loadPrev();
        refreshPresetCombo();
    };
    presetNextBtn.onClick = [this, &p] {
        p.presetManager.loadNext();
        refreshPresetCombo();
    };
    presetSaveBtn.onClick = [this, &p] {
        auto* aw = new juce::AlertWindow ("Save Preset", "Enter preset name:",
                                          juce::MessageBoxIconType::NoIcon);
        aw->addTextEditor ("name", "My Preset");
        aw->addButton ("Save",   1);
        aw->addButton ("Cancel", 0);
        aw->enterModalState (true,
            juce::ModalCallbackFunction::create ([aw, this, &p](int result) {
                if (result == 1)
                {
                    auto name = aw->getTextEditorContents ("name");
                    if (name.isNotEmpty())
                    {
                        p.presetManager.savePreset (name);
                        refreshPresetCombo();
                    }
                }
                delete aw;
            }), false);
    };

    // Connecter les knobs au processeur (mod rings + shift-drag)
    filterCutoff.setProcessor (&p);
    filterRes.setProcessor    (&p);
    filterEnvAmt.setProcessor (&p);
    filterDrive.setProcessor  (&p);
    filterFat.setProcessor    (&p);
    filterMix.setProcessor    (&p);
    filterPan.setProcessor    (&p);
    subLevel.setProcessor     (&p);
    glideKnob.setProcessor    (&p);
    masterGain.setProcessor   (&p);
    noiseLevel.setProcessor   (&p);
    noisePan.setProcessor     (&p);
    osc1Panel.setProcessor    (&p);
    osc2Panel.setProcessor    (&p);
    osc3Panel.setProcessor    (&p);
    envLfoPanel.setProcessorForKnobs (&p);

    // ---- FX Tab button ----
    fxTabBtn.setButtonText ("FX");
    fxTabBtn.setClickingTogglesState (true);
    fxTabBtn.setColour (juce::TextButton::buttonColourId,   juce::Colour(0xff0e0e20));
    fxTabBtn.setColour (juce::TextButton::buttonOnColourId, NS_ORANGE.withAlpha(0.35f));
    fxTabBtn.setColour (juce::TextButton::textColourOffId,  juce::Colour(0xff707090));
    fxTabBtn.setColour (juce::TextButton::textColourOnId,   NS_ORANGE);
    addAndMakeVisible (fxTabBtn);

    fxTabBtn.onClick = [this]
    {
        showingFX = !showingFX;
        fxTabBtn.setToggleState (showingFX, juce::dontSendNotification);
        setSynthVisible (!showingFX);
        fxPanel->setVisible (showingFX);
        resized();
    };

    // ---- FX Panel ----
    fxPanel = std::make_unique<EffectsPanel> (p, p.apvts);
    addChildComponent (*fxPanel);   // starts hidden
    fxPanel->setVisible (false);

    // ---- Clavier d'ordinateur (style Bitwig) ----
    keysBtn.setButtonText ("KEYS C4");
    keysBtn.setColour (juce::TextButton::buttonColourId,   juce::Colour(0xff0e0e20));
    keysBtn.setColour (juce::TextButton::buttonOnColourId, NS_CYAN.withAlpha(0.35f));
    keysBtn.setColour (juce::TextButton::textColourOffId,  juce::Colour(0xff606090));
    keysBtn.setColour (juce::TextButton::textColourOnId,   NS_CYAN);

    keysBtn.setClickingTogglesState (true);
    keysBtn.onClick = [this]
    {
        computerKeysEnabled = keysBtn.getToggleState();
        if (computerKeysEnabled)
        {
            grabKeyboardFocus();
            startTimerHz (20);  // re-check focus 20x/s
        }
        else
        {
            stopTimer();
            // Relâcher toutes les notes tenues
            for (int kc : heldComputerKeys)
            {
                int note = computerKeyToNote (kc);
                if (note >= 0) processor.keyboardState.noteOff (1, note, 0.f);
            }
            heldComputerKeys.clear();
        }
    };

    addAndMakeVisible (keysBtn);
    setWantsKeyboardFocus (true);

    setSize (1364, 737);
    setResizable (true, true);
    setResizeLimits (900, 616, 1760, 1188);
}

// ============================================================
//  Clavier d'ordinateur — mapping style Bitwig / FL Studio
// ============================================================
//
//  Layout piano sur le clavier AZERTY/QWERTY :
//
//   W  3  E  4  R     T  6  Y  7  U     O  0  P
//  A  Z  E  R  T  Y  U  I  O  P  ...
//
//  Rangée 1 (touches blanches et noires) :
//   A=C  W=C#  S=D  E=D#  D=E  F=F  T=F#  G=G  Y=G#  H=A  U=A#  J=B
//   K=C+1  O=C#+1  L=D+1
//
//  Z = octave bas   X = octave haut  (si KEYS inactif = notes normales)
// ============================================================

int NovaSynthEditor::computerKeyToNote (int kc) const
{
    // Mapping AZERTY — touches blanches sur la rangée du milieu, noires sur la rangée du haut
    //
    //   Z  E     T  Y  U     O  P       ← noires (C# D# F# G# A# C#+1 D#+1)
    //  Q  S  D  F  G  H  J  K  L  M    ← blanches (C D E F G A B C+1 D+1 E+1)
    //
    //  W = octave bas   X = octave haut
    int base = computerKeyOctave * 12;

    // Touches blanches
    if (kc == 'q' || kc == 'Q') return base + 0;   // C
    if (kc == 's' || kc == 'S') return base + 2;   // D
    if (kc == 'd' || kc == 'D') return base + 4;   // E
    if (kc == 'f' || kc == 'F') return base + 5;   // F
    if (kc == 'g' || kc == 'G') return base + 7;   // G
    if (kc == 'h' || kc == 'H') return base + 9;   // A
    if (kc == 'j' || kc == 'J') return base + 11;  // B
    if (kc == 'k' || kc == 'K') return base + 12;  // C+1
    if (kc == 'l' || kc == 'L') return base + 14;  // D+1
    if (kc == 'm' || kc == 'M') return base + 16;  // E+1

    // Touches noires
    if (kc == 'z' || kc == 'Z') return base + 1;   // C#
    if (kc == 'e' || kc == 'E') return base + 3;   // D#
    if (kc == 't' || kc == 'T') return base + 6;   // F#
    if (kc == 'y' || kc == 'Y') return base + 8;   // G#
    if (kc == 'u' || kc == 'U') return base + 10;  // A#
    if (kc == 'o' || kc == 'O') return base + 13;  // C#+1
    if (kc == 'p' || kc == 'P') return base + 15;  // D#+1

    return -1;
}

void NovaSynthEditor::sendComputerKeyNote (int kc, bool noteOn)
{
    // W = octave bas, X = octave haut (AZERTY)
    if (kc == 'w' || kc == 'W')
    {
        if (noteOn)
        {
            computerKeyOctave = juce::jmax (0, computerKeyOctave - 1);
            keysBtn.setButtonText ("KEYS C" + juce::String(computerKeyOctave));
        }
        return;
    }
    if (kc == 'x' || kc == 'X')
    {
        if (noteOn)
        {
            computerKeyOctave = juce::jmin (8, computerKeyOctave + 1);
            keysBtn.setButtonText ("KEYS C" + juce::String(computerKeyOctave));
        }
        return;
    }

    int note = computerKeyToNote (kc);
    if (note < 0 || note > 127) return;

    if (noteOn && heldComputerKeys.find(kc) == heldComputerKeys.end())
    {
        heldComputerKeys.insert (kc);
        processor.keyboardState.noteOn (1, note, 0.8f);
    }
    else if (!noteOn)
    {
        if (heldComputerKeys.erase (kc) > 0)
            processor.keyboardState.noteOff (1, note, 0.0f);
    }
}

bool NovaSynthEditor::keyPressed (const juce::KeyPress& key)
{
    if (!computerKeysEnabled) return false;
    sendComputerKeyNote (key.getKeyCode(), true);
    return true;
}

bool NovaSynthEditor::keyStateChanged (bool /*isKeyDown*/)
{
    if (!computerKeysEnabled) return false;

    // Vérifier quels held keys ne sont plus enfoncés → noteOff
    std::set<int> toRelease;
    for (int kc : heldComputerKeys)
    {
        juce::KeyPress kp (kc);
        if (!kp.isCurrentlyDown())
            toRelease.insert (kc);
    }
    for (int kc : toRelease)
        sendComputerKeyNote (kc, false);

    return false;
}

void NovaSynthEditor::setSynthVisible (bool visible)
{
    osc1Panel.setVisible   (visible);
    osc2Panel.setVisible   (visible);
    osc3Panel.setVisible   (visible);
    filterCurve.setVisible (visible);
    filterType.setVisible  (visible);
    filterCutoff.setVisible   (visible);
    filterRes.setVisible      (visible);
    filterEnvAmt.setVisible   (visible);
    filterDrive.setVisible    (visible);
    filterFat.setVisible      (visible);
    filterMix.setVisible      (visible);
    filterPan.setVisible      (visible);
    filterEnableBtn.setVisible (visible);
    filterRouteOsc1Btn.setVisible (visible);
    filterRouteOsc2Btn.setVisible (visible);
    filterRouteOsc3Btn.setVisible (visible);
    subLevel.setVisible    (visible);
    subOct.setVisible      (visible);
    subWave.setVisible     (visible);
    subEnableBtn.setVisible(visible);
    glideKnob.setVisible   (visible);
    monoBtn.setVisible     (visible);
    noiseLevel.setVisible  (visible);
    noisePan.setVisible    (visible);
    noiseTypeCombo.setVisible (visible);
    noiseEnableBtn.setVisible (visible);
    envLfoPanel.setVisible (visible);
    masterGain.setVisible  (visible);
}

NovaSynthEditor::~NovaSynthEditor() {}

void NovaSynthEditor::refreshPresetCombo()
{
    presetCombo.clear (juce::dontSendNotification);
    auto names = processor.presetManager.getPresetNames();
    int factoryN = processor.presetManager.getNumFactory();
    for (int i = 0; i < names.size(); ++i)
    {
        juce::String display = (i < factoryN ? "" : "* ") + names[i];
        presetCombo.addItem (display, i + 1);
    }
    presetCombo.setSelectedItemIndex (
        processor.presetManager.getCurrentIndex(),
        juce::dontSendNotification);
}

void NovaSynthEditor::drawSection (juce::Graphics& g, juce::Rectangle<int> b,
                                    const juce::String& title, juce::Colour colour)
{
    // Panel background #141622
    g.setColour (NS_BG2);
    g.fillRoundedRectangle (b.toFloat(), 5.0f);
    // Border #1E2235, with slight colour tint
    g.setColour (NS_BORDER.interpolatedWith (colour, 0.15f));
    g.drawRoundedRectangle (b.toFloat().reduced(0.5f), 5.0f, 1.0f);
    // Header accent strip
    g.setColour (colour.withAlpha(0.08f));
    g.fillRoundedRectangle (b.toFloat().removeFromTop(18), 4.0f);
    // Title — 12pt bold, white
    g.setColour (NS_TEXT1);
    g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    g.drawText (title, b.reduced(7,2).removeFromTop(18), juce::Justification::centredLeft);
    // Accent dot/bar on left edge
    g.setColour (colour.withAlpha (0.75f));
    g.fillRoundedRectangle (b.toFloat().removeFromLeft(2).removeFromTop(18), 1.0f);
}

void NovaSynthEditor::paint (juce::Graphics& g)
{
    auto fullB  = getLocalBounds();
    float W     = (float)fullB.getWidth();
    float H     = (float)fullB.getHeight();

    // ============================================================
    //  FOND SOBRE — Dark Navy + Grille subtile
    // ============================================================

    // Base très sombre navy #0D0F1A
    g.fillAll (NS_BG0);

    // Grille fine subtile (40px)
    g.setColour (juce::Colour (0xff121420));
    for (int x = 0; x < (int)W; x += 40)
        g.drawVerticalLine   (x, 0.f, H);
    for (int y = 0; y < (int)H; y += 40)
        g.drawHorizontalLine (y, 0.f, W);

    // Grille large (160px), légèrement plus visible
    g.setColour (juce::Colour (0xff161828));
    for (int x = 0; x < (int)W; x += 160)
        g.drawVerticalLine   (x, 0.f, H);
    for (int y = 0; y < (int)H; y += 160)
        g.drawHorizontalLine (y, 0.f, W);

    // Léger vignette / gradient radial centré
    {
        juce::ColourGradient glow (juce::Colour (0x0a1a2040), W*0.5f, H*0.5f,
                                   juce::Colour (0x00000000), W, H, true);
        g.setGradientFill (glow);
        g.fillRect (fullB);
    }

    // ============================================================
    //  TITRE
    // ============================================================
    auto titleBar = fullB.removeFromTop (40);

    // Fond de la barre titre — panel dark
    g.setColour (NS_BG2);
    g.fillRect (titleBar);

    // Ligne décorative sous le titre — accent vert subtil
    g.setColour (NS_CYAN.withAlpha (0.30f));
    g.drawHorizontalLine (40, 0.f, W);

    // Titre à gauche
    auto nameArea = titleBar.removeFromLeft (140);
    g.setColour (NS_TEXT1);
    g.setFont (juce::FontOptions (18.0f, juce::Font::bold));
    g.drawText ("OCTOPUS", nameArea.reduced(8, 0), juce::Justification::centredLeft);
    g.setColour (NS_TEXT2);
    g.setFont (juce::FontOptions (8.0f));
    g.drawText ("SYNTHESIZER", nameArea.translated (0, 8).reduced(8, 0),
                juce::Justification::bottomLeft);

    // (les widgets preset et fxTabBtn se positionnent via resized())

    const int m   = 5;
    const int top = 40;
    int w = getWidth(), h = getHeight();

    int remaining = h - top - m*3;
    int row1H = remaining * 60 / 100;
    int row2H = remaining - row1H;
    int row2y = top + row1H + m;

    // Ligne 1 : OSC1 / OSC2 / OSC3 / FILTER / SUB / NOISE / LEAD
    int totalW = w - m * 7;
    int oscW   = totalW * 17 / 100;
    int filtW  = totalW * 22 / 100;
    int subW   = totalW * 8  / 100;
    int noiseW = totalW * 8  / 100;
    int leadW  = totalW - 3*oscW - filtW - subW - noiseW;

    drawSection (g, {m,                         top, oscW,   row1H}, "OSC 1",  NS_VIOLET);
    drawSection (g, {m*2+oscW,                  top, oscW,   row1H}, "OSC 2",  NS_PINK);
    drawSection (g, {m*3+oscW*2,                top, oscW,   row1H}, "OSC 3",  NS_CYAN);
    drawSection (g, {m*4+oscW*3,                top, filtW,  row1H}, "FILTER", NS_GREEN);
    drawSection (g, {m*5+oscW*3+filtW,          top, subW,   row1H}, "SUB",    NS_VIOLET);
    drawSection (g, {m*6+oscW*3+filtW+subW,     top, noiseW, row1H}, "NOISE",  NS_CYAN);
    drawSection (g, {m*7+oscW*3+filtW+subW+noiseW, top, leadW, row1H}, "LEAD", NS_PINK);

    // Ligne 2 : ENV/LFO + MASTER
    int totalW2 = w - m * 3;
    int envLfoW = totalW2 * 82 / 100;
    int masterW = totalW2 - envLfoW;
    drawSection (g, {m*2 + envLfoW, row2y, masterW, row2H}, "MASTER", NS_CYAN);
}

void NovaSynthEditor::resized()
{
    const int m   = 5;
    const int top = 40;   // barre preset en haut
    int w = getWidth(), h = getHeight();

    // ---- Barre Preset ----
    {
        auto bar = juce::Rectangle<int>(m, 4, w - m*2, 32);
        // Titre à gauche (dessiné dans paint())
        bar.removeFromLeft (140);  // skip title area
        presetPrevBtn.setBounds (bar.removeFromLeft (26).reduced (1, 2));
        presetNextBtn.setBounds (bar.removeFromLeft (26).reduced (1, 2));
        bar.removeFromLeft (4);
        presetCombo.setBounds   (bar.removeFromLeft (220).reduced (0, 2));
        bar.removeFromLeft (4);
        presetSaveBtn.setBounds (bar.removeFromLeft (48).reduced (1, 2));
        // FX tab button at the right end of the header
        fxTabBtn.setBounds (bar.removeFromRight (40).reduced (1, 3));
        // Clavier d'ordinateur (W=oct-, X=oct+)
        keysBtn.setBounds (bar.removeFromRight (72).reduced (1, 3));
    }

    int remaining = h - top - m*3;
    int row1H = remaining * 60 / 100;
    int row2H = remaining - row1H;
    int row2y = top + row1H + m;

    // ---- Ligne 1 : OSC1 / OSC2 / OSC3 / FILTER / SUB / NOISE / LEAD ----
    int totalW = w - m * 7;
    int oscW   = totalW * 17 / 100;
    int filtW  = totalW * 22 / 100;
    int subW   = totalW * 8  / 100;
    int noiseW = totalW * 8  / 100;
    int leadW  = totalW - 3*oscW - filtW - subW - noiseW;

    osc1Panel.setBounds (m,            top, oscW, row1H);
    osc2Panel.setBounds (m*2+oscW,     top, oscW, row1H);
    osc3Panel.setBounds (m*3+oscW*2,   top, oscW, row1H);

    int x = m*4 + oscW*3;  // starts at FILTER

    // FILTER — layout Serum-style
    {
        auto b = juce::Rectangle<int>(x, top, filtW, row1H).reduced(3).withTrimmedTop(18);

        // Header : [ON] [TYPE] — 1 ligne
        auto header = b.removeFromTop (26);
        filterEnableBtn.setBounds (header.removeFromLeft (32).reduced(1,2));
        filterType.setBounds (header.reduced(1,2));
        b.removeFromTop (2);

        // Courbe filtre (45% de la hauteur restante)
        int curveH = b.getHeight() * 42 / 100;
        filterCurve.setBounds (b.removeFromTop (curveH));
        b.removeFromTop (2);

        // Routing OSC1 / OSC2 / OSC3
        auto routeRow = b.removeFromTop (20);
        int rw = routeRow.getWidth() / 3;
        filterRouteOsc1Btn.setBounds (routeRow.removeFromLeft(rw).reduced(1,1));
        filterRouteOsc2Btn.setBounds (routeRow.removeFromLeft(rw).reduced(1,1));
        filterRouteOsc3Btn.setBounds (routeRow.reduced(1,1));
        b.removeFromTop (2);

        // Knobs — 2 rangées de 4
        int kh = b.getHeight() / 2;
        {
            auto row = b.removeFromTop (kh);
            int kw = row.getWidth() / 4;
            filterCutoff.setBounds (row.removeFromLeft(kw));
            filterRes.setBounds    (row.removeFromLeft(kw));
            filterEnvAmt.setBounds (row.removeFromLeft(kw));
            filterDrive.setBounds  (row);
        }
        {
            int kw = b.getWidth() / 4;
            filterFat.setBounds (b.removeFromLeft(kw));
            filterMix.setBounds (b.removeFromLeft(kw));
            filterPan.setBounds (b.removeFromLeft(kw));
            // 4ème slot vide ou pour extension future
        }
    }
    x += filtW + m;

    // SUB — sub oscillateur avec ON button
    {
        auto b = juce::Rectangle<int>(x, top, subW, row1H).reduced(3).withTrimmedTop(18);

        // Header: [ON] [WAVE]
        auto header = b.removeFromTop (22);
        subEnableBtn.setBounds (header.removeFromLeft (28).reduced(1,2));
        subWave.setBounds (b.removeFromTop (32));
        b.removeFromTop (2);

        int cw = b.getWidth() / 2;
        auto row = b.removeFromTop (b.getHeight());
        subLevel.setBounds (row.removeFromLeft (cw));
        subOct.setBounds   (row);
    }
    x += subW + m;

    // NOISE oscillator panel
    {
        auto b = juce::Rectangle<int>(x, top, noiseW, row1H).reduced(3).withTrimmedTop(18);

        // Header: [ON]
        auto header = b.removeFromTop (22);
        noiseEnableBtn.setBounds (header.reduced(1,2));
        b.removeFromTop (2);

        noiseTypeCombo.setBounds (b.removeFromTop (32));
        b.removeFromTop (2);

        int cw = b.getWidth() / 2;
        auto row = b.removeFromTop (b.getHeight());
        noiseLevel.setBounds (row.removeFromLeft (cw));
        noisePan.setBounds   (row);
    }
    x += noiseW + m;

    // LEAD — MONO mode + GLIDE
    {
        auto b = juce::Rectangle<int>(x, top, leadW, row1H).reduced(3).withTrimmedTop(18);

        int bh = b.getHeight() / 2;
        monoBtn.setBounds   (b.removeFromTop (bh).reduced (2, 2));
        glideKnob.setBounds (b);
    }

    // ---- Ligne 2 ----
    int totalW2 = w - m * 3;
    int envLfoW = totalW2 * 82 / 100;
    int masterW = totalW2 - envLfoW;

    envLfoPanel.setBounds (m, row2y, envLfoW, row2H);

    // MASTER
    {
        auto mb = juce::Rectangle<int>(m*2 + envLfoW, row2y, masterW, row2H)
                      .reduced(3).withTrimmedTop(18);
        masterGain.setBounds (mb.getX() + (mb.getWidth()-70)/2,
                              mb.getY(), 70, mb.getHeight());
    }

    // ---- FX Panel (full content area, shown when showingFX == true) ----
    if (fxPanel)
        fxPanel->setBounds (m, top, w - m*2, h - top - m);
}
