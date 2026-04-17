#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

// ============================================================
//  Palette — Sober monochrome (warm amber accent + grayscale)
// ============================================================
// Single accent color for all interactive/active states. Elements
// differentiate via position/label, not hue — for a cohesive look.
inline const juce::Colour NS_ACCENT { 0xffC9A36B };  // Warm amber (all accents)
inline const juce::Colour NS_ACCENT_DIM { 0xff7A6340 };  // Dim amber (inactive)

// Legacy aliases — all point to the single accent for visual coherence.
inline const juce::Colour NS_CYAN   = NS_ACCENT;
inline const juce::Colour NS_ORANGE = NS_ACCENT;
inline const juce::Colour NS_VIOLET = NS_ACCENT;
inline const juce::Colour NS_PINK   = NS_ACCENT;
inline const juce::Colour NS_GREEN  = NS_ACCENT;
inline const juce::Colour NS_SKY    = NS_ACCENT;

// Neutral backgrounds and text.
inline const juce::Colour NS_BG0    { 0xff0B0C10 };  // App background
inline const juce::Colour NS_BG2    { 0xff14161C };  // Panel background
inline const juce::Colour NS_BORDER { 0xff252832 };  // Subtle border
inline const juce::Colour NS_TEXT1  { 0xffE4E6EC };  // Primary text
inline const juce::Colour NS_TEXT2  { 0xff7A7F8C };  // Secondary text

// LFO color helper: slight alpha variation per index for subtle distinction,
// same hue throughout (monochrome palette).
inline juce::Colour lfoColour (int idx)
{
    switch (idx) {
        case 0: return NS_ACCENT;
        case 1: return NS_ACCENT.withBrightness (0.85f);
        case 2: return NS_ACCENT.withBrightness (0.72f);
        case 3: return NS_ACCENT.withBrightness (0.60f);
        default: return NS_ACCENT;
    }
}

// ============================================================
//  WaveformDisplay — affichage OSC réactif (Serum-style)
// ============================================================
class WaveformDisplay : public juce::Component,
                        public juce::AudioProcessorValueTreeState::Listener
{
public:
    WaveformDisplay (juce::AudioProcessorValueTreeState& apvts,
                     const juce::String& waveParam,
                     juce::Colour colour)
        : apvts(apvts), waveParam(waveParam), col(colour)
    {
        apvts.addParameterListener (waveParam, this);
        currentWave = (int)*apvts.getRawParameterValue (waveParam);
    }

    void addParam (const juce::String& id, float* target)
    {
        extraParams.push_back ({ id, target });
        apvts.addParameterListener (id, this);
        *target = *apvts.getRawParameterValue (id);
    }

    ~WaveformDisplay() override
    {
        apvts.removeParameterListener (waveParam, this);
        for (auto& e : extraParams) apvts.removeParameterListener (e.id, this);
    }

    void parameterChanged (const juce::String& id, float v) override
    {
        if (id == waveParam) currentWave = (int)v;
        else for (auto& e : extraParams) if (e.id == id) { *e.target = v; break; }
        juce::MessageManager::callAsync ([this] { repaint(); });
    }

    float level { 0.8f }, detune { 0.0f }, tune { 0.0f };
    float warpAmt { 0.0f }, warpModeF { 0.0f }, uniVoicesF { 1.0f };

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat().reduced (1.5f);
        g.setColour (NS_BG2);
        g.fillRoundedRectangle (b, 4.0f);
        g.setColour (col.withAlpha (0.22f));
        g.drawRoundedRectangle (b.reduced (0.5f), 4.0f, 1.0f);

        float midY = b.getCentreY();
        g.setColour (NS_BORDER);
        g.drawHorizontalLine ((int)midY, b.getX()+4, b.getRight()-4);

        float amp    = b.getHeight() * (0.08f + level * 0.34f);
        float cycles = juce::jlimit (1.0f, 4.0f, 1.0f + std::abs(tune)/24.0f*1.5f);

        auto makeWavePath = [&](float phaseOffset) -> juce::Path
        {
            juce::Path path;
            const int N = 300;
            for (int i = 0; i <= N; ++i)
            {
                float t = (float)i / N;
                float p = std::fmod (t * cycles + phaseOffset, 1.0f);
                if (warpAmt > 0.001f)
                {
                    switch ((int)warpModeF) {
                        case 1: p = std::pow(p, 1.f/(1.f+warpAmt*3.f)); break;
                        case 2: p = std::pow(p, 1.f+warpAmt*3.f);       break;
                        case 3: p = std::fmod(p*(1.f+warpAmt*7.f),1.f); break;
                        case 5: p = (p<.5f)?p*2.f:(1.f-p)*2.f;          break;
                        default: break;
                    }
                }
                float y = sampleWave (p);
                if ((int)warpModeF == 4 && warpAmt > 0.001f) {
                    y *= 1.0f + warpAmt * 3.5f;
                    for (int j=0;j<8;++j) {
                        if      (y>1.f)  y=2.f-y;
                        else if (y<-1.f) y=-2.f-y;
                        else break;
                    }
                }
                if ((int)warpModeF == 6 && currentWave == 1) {
                    float duty = 0.05f + warpAmt * 0.90f;
                    y = (p < duty) ? 1.f : -1.f;
                }
                float px = b.getX() + t * b.getWidth();
                float py = midY - y * amp;
                (i == 0) ? path.startNewSubPath(px,py) : path.lineTo(px,py);
            }
            return path;
        };

        auto path = makeWavePath (0.0f);
        juce::Path fill = path;
        fill.lineTo (b.getRight(), midY);
        fill.lineTo (b.getX(), midY);
        fill.closeSubPath();
        g.setColour (col.withAlpha (0.13f));
        g.fillPath (fill);
        g.setColour (col);
        g.strokePath (path, juce::PathStrokeType (1.8f));

        if (std::abs(detune) > 1.0f) {
            float offset = (detune / 100.0f) * 0.025f;
            g.setColour (col.withAlpha (0.40f));
            g.strokePath (makeWavePath(offset), juce::PathStrokeType (1.1f));
        }
        int uniV = (int)uniVoicesF;
        for (int v = 1; v < std::min(uniV, 4); ++v) {
            float off = (float)v / uniVoicesF * 0.04f;
            g.setColour (col.withAlpha (0.18f));
            g.strokePath (makeWavePath(off), juce::PathStrokeType (1.0f));
        }
    }

private:
    struct ExtraParam { juce::String id; float* target; };
    juce::AudioProcessorValueTreeState& apvts;
    juce::String waveParam;
    juce::Colour col;
    int  currentWave { 0 };
    std::vector<ExtraParam> extraParams;

    float sampleWave (float p) const {
        switch (currentWave) {
            case 0: return 2.0f*p-1.0f;
            case 1: return (p<0.5f)?1.0f:-1.0f;
            case 2: return (p<0.5f)?(4.f*p-1.f):(3.f-4.f*p);
            case 3: return std::sin(p*juce::MathConstants<float>::twoPi);
            default: return 0.0f;
        }
    }
};

// ============================================================
//  LfoCustomDisplay — éditeur LFO style Serum (points dessinables)
// ============================================================
class LfoCustomDisplay : public juce::Component, public juce::Timer
{
public:
    int lfoIndex { 0 };

    LfoCustomDisplay (NovaSynthProcessor& proc, int idx, juce::Colour col)
        : proc(proc), lfoIndex(idx), col(col)
    {
        setPreset (0);   // Sine par défaut
        startTimerHz (30);
    }

    ~LfoCustomDisplay() override { stopTimer(); }

    void setPreset (int wave)
    {
        switch (wave) {
            case 0: points = {{0.f,0.f},{0.25f,1.f},{0.5f,0.f},{0.75f,-1.f},{1.f,0.f}}; break;
            case 1: points = {{0.f,-1.f},{0.5f,1.f},{1.f,-1.f}}; break;
            case 2: points = {{0.f,1.f},{0.98f,-1.f},{1.f,1.f}}; break;
            case 3: points = {{0.f,1.f},{0.498f,1.f},{0.5f,-1.f},{0.998f,-1.f},{1.f,1.f}}; break;
            default: break;
        }
        pushToProcessor();
        repaint();
    }

    void timerCallback() override
    {
        // Paused LFO: freeze the phase dot but still repaint for visual
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat().reduced (1.5f);

        // Dark navy background
        g.setColour (NS_BG2);
        g.fillRoundedRectangle (b, 4.f);

        // Grid — subtle 16 divisions (like Serum)
        float midY = b.getCentreY();
        g.setColour (juce::Colour (0xff1E2235));
        float gw = b.getWidth() / 16.f;
        for (int i = 1; i < 16; ++i)
            g.drawVerticalLine ((int)(b.getX() + i*gw), b.getY()+2, b.getBottom()-2);
        // Emphasize quarter divisions
        g.setColour (juce::Colour (0xff252840));
        for (int i : {4, 8, 12})
            g.drawVerticalLine ((int)(b.getX() + i*gw), b.getY()+2, b.getBottom()-2);
        g.setColour (juce::Colour (0xff252840));
        g.drawHorizontalLine ((int)midY, b.getX()+2, b.getRight()-2);
        g.setColour (col.withAlpha (0.35f));
        g.drawRoundedRectangle (b.reduced(0.5f), 4.f, 1.f);

        // Paused indicator
        if (lfoPaused)
        {
            g.setColour (juce::Colours::orange.withAlpha (0.18f));
            g.fillRoundedRectangle (b, 4.f);
        }

        // Forme d'onde
        float amp = b.getHeight() * 0.43f;
        if (points.size() >= 2)
        {
            juce::Path wPath;
            const int N = 256;
            for (int i = 0; i <= N; ++i)
            {
                float t  = (float)i / N;
                float y  = interpolate (t);
                float px = b.getX() + t * b.getWidth();
                float py = midY - y * amp;
                (i == 0) ? wPath.startNewSubPath (px, py) : wPath.lineTo (px, py);
            }
            juce::Path fill = wPath;
            fill.lineTo (b.getRight(), midY);
            fill.lineTo (b.getX(), midY);
            fill.closeSubPath();
            g.setColour (col.withAlpha (0.09f));
            g.fillPath (fill);
            g.setColour (col);
            g.strokePath (wPath, juce::PathStrokeType (2.0f));
        }

        // Points de contrôle
        for (int i = 0; i < (int)points.size(); ++i)
        {
            float px = b.getX() + points[i].x * b.getWidth();
            float py = midY - points[i].y * amp;
            bool edge = (i == 0 || i == (int)points.size()-1);
            g.setColour (edge ? col.withAlpha(0.5f) : col);
            g.fillEllipse (px-4.f, py-4.f, 8.f, 8.f);
            g.setColour (juce::Colours::white.withAlpha (0.75f));
            g.drawEllipse (px-4.f, py-4.f, 8.f, 8.f, 1.f);
        }

        // Curve handles — diamond at midpoint of each segment
        for (int i = 0; i + 1 < (int)points.size(); ++i)
        {
            float mx   = (points[i].x + points[i+1].x) * 0.5f;
            float crv  = points[i].curve;
            // Evaluate curved midpoint value
            float myCurved = interpolate (mx);
            float hx  = b.getX() + mx * b.getWidth();
            float hy  = midY - myCurved * amp;
            // Draw diamond
            float ds = 5.f;
            juce::Path diamond;
            diamond.startNewSubPath (hx,    hy - ds);
            diamond.lineTo          (hx+ds, hy);
            diamond.lineTo          (hx,    hy + ds);
            diamond.lineTo          (hx-ds, hy);
            diamond.closeSubPath();
            g.setColour ((i == curveDragIdx)
                ? col
                : col.withAlpha (std::abs(crv) > 0.05f ? 0.7f : 0.35f));
            g.fillPath   (diamond);
            g.setColour  (juce::Colours::white.withAlpha (0.6f));
            g.strokePath (diamond, juce::PathStrokeType (0.8f));
        }

        // Point de phase animé
        float phase = proc.getLfoPhase (lfoIndex);
        float dotX  = b.getX() + phase * b.getWidth();
        float dotY  = midY - interpolate (phase) * amp;
        g.setColour (col.withAlpha (0.3f));
        g.fillEllipse (dotX-5.f, dotY-5.f, 10.f, 10.f);
        g.setColour (juce::Colours::white.withAlpha (0.9f));
        g.fillEllipse (dotX-2.5f, dotY-2.5f, 5.f, 5.f);

        // Paused label
        if (lfoPaused)
        {
            g.setColour (juce::Colours::orange.withAlpha (0.85f));
            g.setFont (juce::FontOptions (9.f, juce::Font::bold));
            g.drawText ("PAUSED", b.removeFromTop(14), juce::Justification::centred);
        }

        // Hint
        g.setColour (NS_TEXT2.withAlpha (0.45f));
        g.setFont (juce::FontOptions (8.0f));
        g.drawText ("drag: draw  |  click: add pt  |  dbl-click: remove  |  alt+drag: route  |  right-click: menu",
                    b.removeFromBottom(12), juce::Justification::centred);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        dragPtIdx    = -1;
        curveDragIdx = -1;
        lfoRouteDrag = false;
        freehandDraw = false;
        if (e.mods.isRightButtonDown())
        {
            showContextMenu();
            return;
        }
        auto b = getLocalBounds().toFloat().reduced (1.5f);

        // Alt+drag (or middle-button drag) → LFO routing drag
        if (e.mods.isAltDown())
        {
            lfoRouteDrag = true;
            startLfoRoutingDrag();
            return;
        }

        // Check curve handles first
        int ci = findNearestCurveHandle (e.position.toFloat(), b);
        if (ci >= 0) { curveDragIdx = ci; curveDragStart = points[ci].curve; return; }
        // Then control points
        dragPtIdx = findNearest (e.position.toFloat(), b);

        // If not near any existing point → freehand draw mode
        if (dragPtIdx < 0)
            freehandDraw = true;
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        auto b = getLocalBounds().toFloat().reduced (1.5f);
        if (curveDragIdx >= 0)
        {
            // Drag up/down to adjust curve (-1..+1)
            float delta = -(float)e.getDistanceFromDragStartY() / (b.getHeight() * 0.5f);
            points[curveDragIdx].curve = juce::jlimit (-1.0f, 1.0f, curveDragStart + delta);
            pushToProcessor();
            repaint();
            return;
        }
        if (dragPtIdx >= 0)
        {
            auto [nx, ny] = screenToNorm (e.position.toFloat(), b);
            if (dragPtIdx == 0)
                nx = 0.f;
            else if (dragPtIdx == (int)points.size()-1)
                nx = 1.f;
            else
            {
                float lo = points[dragPtIdx-1].x + 0.015f;
                float hi = points[dragPtIdx+1].x - 0.015f;
                nx = juce::jlimit (lo, hi, nx);
            }
            points[dragPtIdx].x = nx;
            points[dragPtIdx].y = juce::jlimit (-1.f, 1.f, ny);
            pushToProcessor();
            repaint();
            return;
        }
        // Freehand drawing: place/update points along the drag path
        if (freehandDraw)
        {
            auto [nx, ny] = screenToNorm (e.position.toFloat(), b);
            nx = juce::jlimit (0.0f, 1.0f, nx);
            ny = juce::jlimit (-1.f, 1.f, ny);
            // Find if there's already a point close in X → update it
            const float xTol = 0.03f;
            bool updated = false;
            for (auto& pt : points)
            {
                if (std::abs (pt.x - nx) < xTol && pt.x > 0.005f && pt.x < 0.995f)
                {
                    pt.y = ny;
                    updated = true;
                    break;
                }
            }
            if (!updated)
            {
                // Don't add at the very edges (those are fixed anchor points)
                if (nx > 0.005f && nx < 0.995f)
                {
                    LfoPoint np { nx, ny, 0.f };
                    addPointSorted (np);
                }
            }
            pushToProcessor();
            repaint();
            return;
        }
        // Alt+drag from non-point area → LFO routing drag
        if (!lfoRouteDrag && e.mods.isAltDown() && e.getDistanceFromDragStart() > 8)
        {
            lfoRouteDrag = true;
            startLfoRoutingDrag();
        }
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        auto b = getLocalBounds().toFloat().reduced (1.5f);
        // Single click on empty space (no drag, no freehand) → add a point
        if (!e.mods.isRightButtonDown()
            && !e.mouseWasDraggedSinceMouseDown()
            && dragPtIdx < 0
            && !freehandDraw)
        {
            auto [nx, ny] = screenToNorm (e.position.toFloat(), b);
            LfoPoint np { juce::jlimit(0.01f, 0.99f, nx), juce::jlimit(-1.f, 1.f, ny) };
            addPointSorted (np);
            pushToProcessor();
            repaint();
        }
        // After freehand draw: sort points by x
        if (freehandDraw)
        {
            std::sort (points.begin(), points.end(),
                       [](const LfoPoint& a, const LfoPoint& b2) { return a.x < b2.x; });
            pushToProcessor();
            repaint();
        }
        dragPtIdx    = -1;
        curveDragIdx = -1;
        lfoRouteDrag = false;
        freehandDraw = false;
    }

    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        auto b = getLocalBounds().toFloat().reduced (1.5f);
        int near = findNearest (e.position.toFloat(), b);
        if (near > 0 && near < (int)points.size()-1)
        {
            points.erase (points.begin() + near);
            pushToProcessor();
            repaint();
        }
    }

    void showContextMenu()
    {
        // Clipboard shared across all LFO displays (static)
        static std::vector<LfoPoint> clipboard;

        juce::PopupMenu menu;
        menu.addItem (1, "Reset LFO phase");
        menu.addItem (2, lfoPaused ? "Resume LFO" : "Pause LFO");
        menu.addItem (3, "Clear shape (sine)");
        menu.addSeparator();
        menu.addItem (4, "Copy shape");
        menu.addItem (5, "Paste shape", !clipboard.empty());

        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this),
            [this, &clipboard = clipboard](int result)
            {
                switch (result)
                {
                    case 1:
                        // Reset LFO phase to 0
                        proc.resetLfoPhase (lfoIndex);
                        break;
                    case 2:
                        lfoPaused = !lfoPaused;
                        break;
                    case 3:
                        setPreset (0);  // sine
                        break;
                    case 4:
                        clipboard = points;
                        break;
                    case 5:
                        if (!clipboard.empty())
                        {
                            points = clipboard;
                            pushToProcessor();
                            repaint();
                        }
                        break;
                    default: break;
                }
            });
    }

private:
    NovaSynthProcessor& proc;
    juce::Colour        col;
    std::vector<LfoPoint> points;
    int  dragPtIdx    { -1    };
    int  curveDragIdx { -1    };
    float curveDragStart { 0.0f };
    bool lfoRouteDrag { false };
    bool lfoPaused    { false };
    bool freehandDraw { false };

    void startLfoRoutingDrag()
    {
        if (auto* dc = juce::DragAndDropContainer::findParentDragContainerFor (this))
        {
            juce::Image img (juce::Image::ARGB, 50, 20, true);
            juce::Graphics ig (img);
            ig.setColour (col.withAlpha (0.92f));
            ig.fillRoundedRectangle (1.f, 1.f, 48.f, 18.f, 4.f);
            ig.setColour (juce::Colours::white);
            ig.setFont (juce::FontOptions (9.f, juce::Font::bold));
            ig.drawText ("LFO " + juce::String(lfoIndex+1),
                         juce::Rectangle<int>(0,3,50,14), juce::Justification::centred);
            dc->startDragging ("LFO:" + juce::String(lfoIndex), this,
                               juce::ScaledImage(img), false);
        }
    }

    float interpolate (float x) const
    {
        if (points.size() < 2) return 0.f;
        for (size_t i = 0; i+1 < points.size(); ++i)
        {
            if (x >= points[i].x && x <= points[i+1].x)
            {
                float span = points[i+1].x - points[i].x;
                float t    = (span > 1e-6f) ? (x - points[i].x) / span : 0.f;
                float crv  = points[i].curve;
                if (std::abs (crv) > 0.001f)
                {
                    float k  = crv * 6.0f;
                    float a0 = std::atan (k * -0.5f);
                    float a1 = std::atan (k *  0.5f);
                    float dn = a1 - a0;
                    if (std::abs (dn) > 1e-6f)
                        t = (std::atan (k * (t - 0.5f)) - a0) / dn;
                }
                return points[i].y + t * (points[i+1].y - points[i].y);
            }
        }
        return points.back().y;
    }

    std::pair<float,float> screenToNorm (juce::Point<float> pos,
                                          juce::Rectangle<float> b) const
    {
        float nx = (pos.x - b.getX()) / b.getWidth();
        float ny = -(pos.y - b.getCentreY()) / (b.getHeight() * 0.43f);
        return { nx, ny };
    }

    int findNearest (juce::Point<float> pos, juce::Rectangle<float> b) const
    {
        float amp = b.getHeight() * 0.43f;
        int   best = -1;
        float bestD = 12.f;
        for (int i = 0; i < (int)points.size(); ++i)
        {
            float px = b.getX() + points[i].x * b.getWidth();
            float py = b.getCentreY() - points[i].y * amp;
            float d  = pos.getDistanceFrom ({px, py});
            if (d < bestD) { bestD = d; best = i; }
        }
        return best;
    }

    // Returns segment index if near a curve handle, else -1
    int findNearestCurveHandle (juce::Point<float> pos, juce::Rectangle<float> b) const
    {
        float amp = b.getHeight() * 0.43f;
        for (int i = 0; i + 1 < (int)points.size(); ++i)
        {
            float mx = (points[i].x + points[i+1].x) * 0.5f;
            float my = interpolate (mx);
            float hx = b.getX() + mx * b.getWidth();
            float hy = b.getCentreY() - my * amp;
            if (pos.getDistanceFrom ({hx, hy}) < 10.f)
                return i;
        }
        return -1;
    }

    void addPointSorted (LfoPoint p)
    {
        for (size_t i = 0; i < points.size(); ++i)
            if (p.x < points[i].x) { points.insert (points.begin()+(int)i, p); return; }
        points.push_back (p);
    }

    void pushToProcessor() { proc.setLfoPoints (lfoIndex, points); }
};

// ============================================================
//  FilterCurveDisplay — courbe de réponse en fréquence du filtre
// ============================================================
class FilterCurveDisplay : public juce::Component,
                            public juce::AudioProcessorValueTreeState::Listener
{
public:
    FilterCurveDisplay (juce::AudioProcessorValueTreeState& apvts) : apvts_ptr(&apvts)
    {
        for (auto* id : {"filterCutoff","filterRes","filterType","filterEnabled"})
            apvts.addParameterListener (id, this);
        cutoff  = *apvts.getRawParameterValue ("filterCutoff");
        res     = *apvts.getRawParameterValue ("filterRes");
        type    = (int)*apvts.getRawParameterValue ("filterType");
        enabled = *apvts.getRawParameterValue ("filterEnabled") > 0.5f;
    }

    ~FilterCurveDisplay() override
    {
        for (auto* id : {"filterCutoff","filterRes","filterType","filterEnabled"})
            apvts_ptr->removeParameterListener (id, this);
    }

    void parameterChanged (const juce::String& id, float v) override
    {
        if      (id == "filterCutoff")  cutoff  = v;
        else if (id == "filterRes")     res     = v;
        else if (id == "filterType")    type    = (int)v;
        else if (id == "filterEnabled") enabled = v > 0.5f;
        juce::MessageManager::callAsync ([this]{ repaint(); });
    }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat().reduced (1.5f);
        g.setColour (juce::Colour (0x55040412));
        g.fillRoundedRectangle (b, 4.f);
        g.setColour (NS_GREEN.withAlpha (0.35f));
        g.drawRoundedRectangle (b.reduced(0.5f), 4.f, 1.f);

        if (!enabled)
        {
            g.setColour (juce::Colour (0xff404060));
            g.setFont (juce::FontOptions (9.f));
            g.drawText ("FILTER OFF", b, juce::Justification::centred);
            return;
        }

        juce::Path curve;
        const int N = 200;
        for (int i = 0; i <= N; ++i)
        {
            float t    = (float)i / N;
            float freq = 20.f * std::pow (1000.f, t);
            float amp  = computeResponse (freq);
            float x    = b.getX() + t * b.getWidth();
            float y    = b.getBottom() - amp * b.getHeight() * 0.88f - b.getHeight()*0.04f;
            (i == 0) ? curve.startNewSubPath (x, y) : curve.lineTo (x, y);
        }
        juce::Path fill = curve;
        fill.lineTo (b.getRight(), b.getBottom());
        fill.lineTo (b.getX(), b.getBottom());
        fill.closeSubPath();
        g.setColour (NS_GREEN.withAlpha (0.13f));
        g.fillPath (fill);
        g.setColour (NS_GREEN);
        g.strokePath (curve, juce::PathStrokeType (1.8f));

        // Repère de fréquence de coupure
        float ct = std::log (cutoff / 20.f) / std::log (20000.f / 20.f);
        float cx = b.getX() + ct * b.getWidth();
        g.setColour (NS_GREEN.withAlpha (0.45f));
        g.drawVerticalLine ((int)cx, b.getY()+2, b.getBottom()-2);
    }

private:
    float computeResponse (float freq) const
    {
        float w = freq / juce::jmax (20.f, cutoff);
        float Q = 0.5f + res * 4.f;
        float r;
        switch (type)
        {
            case 0: r = 1.f / (1.f + std::pow(w, 1.f)); break;   // LP6
            case 1: r = 1.f / (1.f + std::pow(w, 2.f)); break;   // LP12
            case 2: r = 1.f / (1.f + std::pow(w, 4.f)); break;   // LP24
            case 3: r = std::pow(w, 2.f) / (1.f + std::pow(w, 2.f)); break; // HP12
            case 4: r = std::pow(w, 4.f) / (1.f + std::pow(w, 4.f)); break; // HP24
            case 5: { float d = 1.f + Q*Q*(w-1.f/w)*(w-1.f/w);
                      r = 1.f / std::sqrt (juce::jmax(0.001f, d)); break; } // BP
            case 6: { float n = w*w-1.f;
                      float d = n*n + w*w/(Q*Q);
                      r = std::sqrt (n*n / juce::jmax(0.001f, d)); break; } // Notch
            case 7: r = 0.5f + 0.5f * std::cos (w * juce::MathConstants<float>::twoPi); break; // Comb (approx)
            default: r = 1.f;
        }
        if (res > 0.05f && (type == 1 || type == 2))
        {
            float bump = res * 2.5f * std::exp (-15.f*(w-1.f)*(w-1.f));
            r = juce::jmin (1.2f, r + bump);
        }
        return juce::jlimit (0.f, 1.f, r);
    }

    juce::AudioProcessorValueTreeState* apvts_ptr { nullptr };
    float cutoff { 8000.f };
    float res    {    0.f };
    int   type   {      0 };
    bool  enabled{ true   };
};

// ============================================================
//  ModSlider — Slider avec Shift+drag pour modifier le montant de mod
// ============================================================
class ModSlider : public juce::Slider
{
public:
    using juce::Slider::Slider;

    std::function<bool()>      onShiftDragStart;
    std::function<void(float)> onShiftDrag;
    std::function<void()>      onRightClick;   // called on right-click instead of default

    void mouseDown (const juce::MouseEvent& e) override
    {
        modActive = false;
        if (e.mods.isRightButtonDown())
        {
            if (onRightClick) onRightClick();
            return;   // swallow — don't open JUCE's text-entry popup
        }
        if (e.mods.isShiftDown() && onShiftDragStart && onShiftDragStart())
        {
            modActive = true;
            return;
        }
        Slider::mouseDown (e);
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (modActive && onShiftDrag)
        {
            onShiftDrag (-(float)e.getDistanceFromDragStartY() / 150.0f);
            return;
        }
        Slider::mouseDrag (e);
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        modActive = false;
        Slider::mouseUp (e);
    }

private:
    bool modActive { false };
};

// ============================================================
//  LabelledKnob — potentiomètre avec label, reset, ring de mod
// ============================================================
class LabelledKnob : public juce::Component,
                     public juce::DragAndDropTarget,
                     private juce::Timer
{
public:
    ModSlider    slider;
    juce::Label  label;

    LabelledKnob (const juce::String& lbl,
                  juce::AudioProcessorValueTreeState& apvts,
                  const juce::String& paramID,
                  juce::Colour fillColour = NS_VIOLET)
        : col(fillColour), storedParamID(paramID), apvts_ptr(&apvts), labelText(lbl)
    {
        slider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
        // NoTextBox → tout l'espace va au cercle du potard
        slider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
        slider.setColour (juce::Slider::rotarySliderFillColourId,    fillColour);
        slider.setColour (juce::Slider::rotarySliderOutlineColourId, NS_BORDER);
        slider.setColour (juce::Slider::thumbColourId,               fillColour.brighter(0.2f));
        addAndMakeVisible (slider);

        // Label : montre la valeur + nom en dessous
        label.setText (lbl, juce::dontSendNotification);
        label.setJustificationType (juce::Justification::centred);
        label.setFont (juce::FontOptions (10.0f));
        label.setColour (juce::Label::textColourId, NS_TEXT2);
        addAndMakeVisible (label);

        // Valeur affichée au-dessus du nom
        valueLabel.setJustificationType (juce::Justification::centred);
        valueLabel.setFont (juce::FontOptions (11.0f, juce::Font::bold));
        valueLabel.setColour (juce::Label::textColourId, NS_TEXT1);
        addAndMakeVisible (valueLabel);

        // Met à jour la valeur affichée à chaque changement
        slider.onValueChange = [this] { updateValueDisplay(); };

        att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>
            (apvts, paramID, slider);

        auto* p = apvts.getParameter (paramID);
        defaultVal = p->getNormalisableRange().convertFrom0to1 (p->getDefaultValue());

        // ---- Formateurs d'unités selon le paramètre (style Serum) ----
        // Temps : ADSR → "10 ms" / "1.50 s"
        if (paramID == "attack" || paramID == "decay" || paramID == "release" ||
            paramID == "env2Attack" || paramID == "env2Decay" || paramID == "env2Release" ||
            paramID == "env3Attack" || paramID == "env3Decay" || paramID == "env3Release")
        {
            slider.textFromValueFunction = [](double v) -> juce::String {
                if (v < 0.1)  return juce::String ((int)(v * 1000)) + " ms";
                if (v < 1.0)  return juce::String (v, 2) + " s";
                return juce::String (v, 1) + " s";
            };
            slider.valueFromTextFunction = [](const juce::String& t) -> double {
                if (t.endsWith("ms")) return t.getDoubleValue() / 1000.0;
                return t.getDoubleValue();
            };
        }
        // Sustain / pourcentage
        else if (paramID == "sustain" || paramID == "env2Sustain" ||
                 paramID == "filterRes" || paramID == "filterDrive" ||
                 paramID == "filterFat" || paramID == "filterMix" ||
                 paramID.endsWith("Level") || paramID.endsWith("Blend") ||
                 paramID.endsWith("WarpAmt"))
        {
            slider.textFromValueFunction = [](double v) -> juce::String {
                return juce::String ((int)std::round (v * 100)) + "%";
            };
        }
        // Pan (bipolaire → L/R/C)
        else if (paramID.contains ("Pan") || paramID == "filterPan")
        {
            slider.textFromValueFunction = [](double v) -> juce::String {
                int pct = (int)std::round (v * 100);
                if (pct == 0) return "C";
                return (pct < 0 ? "L" : "R") + juce::String (std::abs(pct)) + "%";
            };
        }
        // Cutoff Hz/kHz
        else if (paramID == "filterCutoff")
        {
            slider.textFromValueFunction = [](double v) -> juce::String {
                if (v >= 1000.0) return juce::String (v / 1000.0, 2) + " kHz";
                return juce::String ((int)v) + " Hz";
            };
        }
        // Detune (cents)
        else if (paramID.endsWith ("Detune"))
        {
            slider.textFromValueFunction = [](double v) -> juce::String {
                return juce::String ((int)std::round(v)) + " ct";
            };
        }
        // Tune (semitones)
        else if (paramID.endsWith ("Tune") || paramID.endsWith ("tune"))
        {
            slider.textFromValueFunction = [](double v) -> juce::String {
                int s = (int)std::round(v);
                return (s >= 0 ? "+" : "") + juce::String(s) + " st";
            };
        }
        // Env amt (bipolaire %)
        else if (paramID == "filterEnvAmt")
        {
            slider.textFromValueFunction = [](double v) -> juce::String {
                int pct = (int)std::round(v * 100);
                return (pct >= 0 ? "+" : "") + juce::String(pct) + "%";
            };
        }
        // LFO Rate → Hz
        else if (paramID.startsWith ("lfo") && paramID.endsWith ("Rate"))
        {
            slider.textFromValueFunction = [](double v) -> juce::String {
                if (v < 0.1)  return juce::String (v, 3) + " Hz";
                if (v < 1.0)  return juce::String (v, 2) + " Hz";
                if (v < 10.0) return juce::String (v, 1) + " Hz";
                return juce::String ((int)std::round (v)) + " Hz";
            };
        }

        // Initial value display update
        updateValueDisplay();

        // Shift+drag : ajuste le montant de modulation au lieu de la valeur du knob
        slider.onShiftDragStart = [this]() -> bool
        {
            if (!proc) return false;
            for (int i = 0; i < 6; ++i)
            {
                if (proc->hasModConnection (i, storedParamID))
                {
                    modDragLfoIdx   = i;
                    modDragStartAmt = proc->getModAmount (i, storedParamID);
                    return true;
                }
            }
            return false;
        };

        slider.onShiftDrag = [this](float delta)
        {
            if (proc && modDragLfoIdx >= 0)
            {
                float newAmt = juce::jlimit (-1.0f, 1.0f, modDragStartAmt + delta);
                proc->setModAmount (modDragLfoIdx, storedParamID, newAmt);
                repaint();
            }
        };

        // Right-click on slider → popup menu with Reset + Set value
        slider.onRightClick = [this]
        {
            juce::PopupMenu menu;
            menu.addItem (1, "Reset to default");
            menu.addItem (2, "Set value...");
            menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&slider),
                [this](int result)
                {
                    if (result == 1)
                    {
                        slider.setValue (defaultVal, juce::sendNotification);
                    }
                    else if (result == 2)
                    {
                        // Show alert window with text entry
                        auto* aw = new juce::AlertWindow ("Set value",
                            "Enter value for " + labelText + ":",
                            juce::MessageBoxIconType::NoIcon);
                        aw->addTextEditor ("val",
                            slider.textFromValueFunction
                                ? slider.textFromValueFunction (slider.getValue())
                                : juce::String (slider.getValue(), 3));
                        aw->addButton ("OK",     1);
                        aw->addButton ("Cancel", 0);
                        aw->enterModalState (true,
                            juce::ModalCallbackFunction::create ([aw, this](int r) {
                                if (r == 1)
                                {
                                    auto txt = aw->getTextEditorContents ("val");
                                    double val = slider.valueFromTextFunction
                                        ? slider.valueFromTextFunction (txt)
                                        : txt.getDoubleValue();
                                    slider.setValue (val, juce::sendNotification);
                                }
                                delete aw;
                            }), false);
                    }
                });
        };
    }

    void setProcessor (NovaSynthProcessor* p)
    {
        proc = p;
        if (p) startTimerHz (30);
    }

    const juce::String& getParamID() const { return storedParamID; }

    void resized() override
    {
        auto b = getLocalBounds();
        label.setBounds      (b.removeFromBottom (14));
        valueLabel.setBounds (b.removeFromBottom (14));
        slider.setBounds     (b);
    }

    void paintOverChildren (juce::Graphics& g) override
    {
        // Toujours dessiner le knob custom (visible même sans modulation)
        auto sb         = slider.getBounds();
        auto rotaryArea = sb;
        int  side       = juce::jmin (rotaryArea.getWidth(), rotaryArea.getHeight());
        float cx        = (float)rotaryArea.getCentreX();
        float cy        = (float)rotaryArea.getCentreY();
        float outerR    = side * 0.42f;
        float baseRingR = outerR * 1.18f;

        // --- Dessin du knob de base (toujours) ---
        {
            auto*  param     = apvts_ptr ? apvts_ptr->getParameter (storedParamID) : nullptr;
            float  normVal   = param ? param->getValue() : 0.5f;
            float  halfPi    = juce::MathConstants<float>::halfPi;
            auto   rp        = slider.getRotaryParameters();
            float  startA    = rp.startAngleRadians;
            float  endA      = rp.endAngleRadians;
            float  valueA    = startA + normVal * (endA - startA);
            float  knobR     = outerR;

            // Fond sombre (recouvre slider JUCE)
            g.setColour (NS_BG0);
            g.fillEllipse (cx - knobR, cy - knobR, knobR * 2.f, knobR * 2.f);

            // Piste complète
            juce::Path track;
            track.addCentredArc (cx, cy, knobR * 0.86f, knobR * 0.86f, 0.f, startA, endA, true);
            g.setColour (NS_BORDER);
            g.strokePath (track, juce::PathStrokeType (3.f));

            // Arc valeur — vert accent NS_CYAN
            juce::Path valArc;
            valArc.addCentredArc (cx, cy, knobR * 0.86f, knobR * 0.86f, 0.f, startA, valueA, true);
            g.setColour (col.withAlpha (0.92f));
            g.strokePath (valArc, juce::PathStrokeType (3.f));

            // Corps central — dark navy #1A1D2E
            float innerR = knobR * 0.62f;
            juce::ColourGradient grad (juce::Colour (0xff1A1D2E), cx, cy - innerR,
                                       juce::Colour (0xff0D0F1A), cx, cy + innerR, false);
            g.setGradientFill (grad);
            g.fillEllipse (cx - innerR, cy - innerR, innerR * 2.f, innerR * 2.f);
            g.setColour (col.withAlpha (0.25f));
            g.drawEllipse (cx - innerR, cy - innerR, innerR * 2.f, innerR * 2.f, 1.2f);

            // Indicateur (ligne blanche) à la position de base
            float lineLen = innerR * 0.70f;
            float ldx = cx + lineLen * std::cos (valueA - halfPi);
            float ldy = cy + lineLen * std::sin (valueA - halfPi);
            g.setColour (juce::Colours::white.withAlpha (0.90f));
            g.drawLine (cx, cy, ldx, ldy, 2.0f);
            g.fillEllipse (ldx - 2.5f, ldy - 2.5f, 5.f, 5.f);
        }

        bool anyMod   = isDragOver;
        bool hasMod[6] = {};
        if (proc) {
            for (int i = 0; i < 6; ++i) {
                hasMod[i] = proc->hasModConnection (i, storedParamID);
                anyMod |= hasMod[i];
            }
        }
        if (!anyMod) return;

        if (isDragOver)
        {
            g.setColour (juce::Colours::white.withAlpha (0.35f));
            g.drawEllipse (cx - baseRingR - 3, cy - baseRingR - 3,
                           (baseRingR + 3) * 2, (baseRingR + 3) * 2, 2.0f);
        }

        // Helper: returns display colour for a mod source index (0-3=LFO1-4, 4=ENV2, 5=ENV3)
        auto modSourceColour = [](int idx) -> juce::Colour {
            if (idx < 4) return lfoColour(idx);
            if (idx == 4) return NS_ORANGE;  // ENV2
            return NS_GREEN;                  // ENV3
        };

        for (int i = 0; i < 6; ++i)
        {
            if (!hasMod[i]) continue;

            float modAmt = proc->getModAmount (i, storedParamID);
            float lfoVal = proc->getLfoValue (i);
            float pulse  = (lfoVal + 1.0f) * 0.5f;
            float alpha  = 0.45f + 0.50f * pulse;

            float span   = juce::MathConstants<float>::pi * 1.5f * std::abs (modAmt);
            float startA = -juce::MathConstants<float>::pi * 0.75f;
            float r      = baseRingR + (float)i * 3.5f;

            juce::Path arc;
            arc.addArc (cx - r, cy - r, r * 2.0f, r * 2.0f,
                        startA, startA + span, true);
            g.setColour (modSourceColour(i).withAlpha (alpha));
            g.strokePath (arc, juce::PathStrokeType (2.5f,
                juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

            float endAngle = startA + span;
            float dotX = cx + r * std::cos (endAngle);
            float dotY = cy + r * std::sin (endAngle);
            g.setColour (modSourceColour(i).withAlpha (alpha));
            g.fillEllipse (dotX - 2.5f, dotY - 2.5f, 5.0f, 5.0f);

            // Tooltip Shift+drag si modifié
            if (modDragLfoIdx == i)
            {
                juce::String amtStr = (modAmt >= 0 ? "+" : "")
                                    + juce::String ((int)(modAmt * 100)) + "%";
                g.setColour (modSourceColour(i));
                g.setFont   (juce::FontOptions (8.5f, juce::Font::bold));
                g.drawText  (amtStr, rotaryArea, juce::Justification::centred);
            }
        }

        // ---- Redessine le knob à la position modulée (style Serum) ----
        if (apvts_ptr && anyMod)
        {
            auto* param = apvts_ptr->getParameter (storedParamID);
            if (param)
            {
                float baseNorm   = param->getValue();
                float modSum     = 0.f;
                for (int i = 0; i < 6; ++i)
                    if (proc->hasModConnection (i, storedParamID))
                        modSum += proc->getLfoValue (i) * proc->getModAmount (i, storedParamID) * 0.5f;
                float moddedNorm = juce::jlimit (0.f, 1.f, baseNorm + modSum);

                // Quand modulation active : redessine l'arc à la position modulée par-dessus
                auto  rp      = slider.getRotaryParameters();
                float startA  = rp.startAngleRadians;
                float endA    = rp.endAngleRadians;
                float baseA   = startA + baseNorm   * (endA - startA);
                float moddedA = startA + moddedNorm * (endA - startA);
                float halfPi  = juce::MathConstants<float>::halfPi;
                float knobR   = outerR;

                // Redessine l'arc à la position modulée (par-dessus l'arc base)
                juce::Path moddedArc;
                moddedArc.addCentredArc (cx, cy, knobR * 0.86f, knobR * 0.86f,
                                         0.f, startA, moddedA, true);
                g.setColour (col.withAlpha (0.95f));
                g.strokePath (moddedArc, juce::PathStrokeType (3.5f));

                // Petit marqueur blanc à la position de base
                float btx = cx + knobR * 0.94f * std::cos (baseA - halfPi);
                float bty = cy + knobR * 0.94f * std::sin (baseA - halfPi);
                g.setColour (juce::Colour (0xffaabbd0));
                g.fillEllipse (btx - 2.f, bty - 2.f, 4.f, 4.f);

                // Redessine l'indicateur à la position modulée
                float innerR  = knobR * 0.62f;
                float lineLen = innerR * 0.70f;
                float ldx = cx + lineLen * std::cos (moddedA - halfPi);
                float ldy = cy + lineLen * std::sin (moddedA - halfPi);
                g.setColour (juce::Colours::white.withAlpha (0.98f));
                g.drawLine (cx, cy, ldx, ldy, 2.2f);
                g.fillEllipse (ldx - 3.f, ldy - 3.f, 6.f, 6.f);
            }
        }
    }

    // ---- DragAndDropTarget ----
    bool isInterestedInDragSource (const SourceDetails& d) override
    {
        if (proc == nullptr) return false;
        auto desc = d.description.toString();
        return desc.startsWith ("LFO:") || desc.startsWith ("ENV:");
    }

    void itemDragEnter (const SourceDetails&) override { isDragOver = true;  repaint(); }
    void itemDragExit  (const SourceDetails&) override { isDragOver = false; repaint(); }
    void itemDragMove  (const SourceDetails&) override {}

    void itemDropped (const SourceDetails& details) override
    {
        isDragOver = false;
        repaint();
        if (!proc) return;

        auto desc = details.description.toString();
        int modIdx = desc.fromFirstOccurrenceOf (":", false, false).getIntValue();

        // ENV2 → internal index 4, ENV3 → internal index 5
        if (desc.startsWith ("ENV:"))
            modIdx = modIdx + 3;  // ENV index 1→4, ENV index 2→5

        if (proc->hasModConnection (modIdx, storedParamID))
            proc->removeModConnection (modIdx, storedParamID);
        else
            proc->addModConnection (modIdx, storedParamID, 0.5f);
    }

private:
    void updateValueDisplay()
    {
        juce::String val;
        if (slider.textFromValueFunction)
            val = slider.textFromValueFunction (slider.getValue());
        else
            val = juce::String (slider.getValue(), 2);
        valueLabel.setText (val, juce::dontSendNotification);
        label.setText (labelText, juce::dontSendNotification);
    }

    void timerCallback() override
    {
        if (proc && (proc->hasModConnection (storedParamID)))
            repaint();
    }

    juce::Colour        col;
    juce::String        storedParamID;
    juce::Label         valueLabel;
    juce::String        labelText;
    float               defaultVal     { 0.0f };
    bool                isDragOver     { false };
    NovaSynthProcessor* proc           { nullptr };
    juce::AudioProcessorValueTreeState* apvts_ptr { nullptr };
    int                 modDragLfoIdx  { -1 };
    float               modDragStartAmt{ 0.0f };
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
};

// ============================================================
//  LfoRateBar — affichage horizontal de la vitesse LFO (style Serum)
//  Drag vertical pour changer la valeur, affiche "X.XX Hz"
// ============================================================
class LfoRateBar : public juce::Component
{
public:
    LfoRateBar (juce::AudioProcessorValueTreeState& apvts,
                const juce::String& paramID,
                juce::Colour col)
        : col(col)
    {
        slider.setSliderStyle (juce::Slider::LinearVertical);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
        slider.setAlpha (0.f);   // invisible — on dessine nous-même
        slider.onValueChange = [this]() { repaint(); };
        addAndMakeVisible (slider);

        att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>
            (apvts, paramID, slider);
    }

    void setProcessor (NovaSynthProcessor*) {}  // compat avec setProcessorForKnobs

    void resized() override { slider.setBounds (getLocalBounds()); }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat().reduced (2.f, 2.f);

        // Fond
        g.setColour (juce::Colour (0x550a0a18));
        g.fillRoundedRectangle (b, 4.f);

        // Remplissage proportionnel à la valeur (gauche → droite)
        double lo   = slider.getMinimum();
        double hi   = slider.getMaximum();
        float  norm = (hi > lo) ? (float)((slider.getValue() - lo) / (hi - lo)) : 0.f;
        g.setColour (col.withAlpha (0.22f));
        g.fillRoundedRectangle (b.withWidth (b.getWidth() * norm), 4.f);

        // Contour
        g.setColour (col.withAlpha (0.50f));
        g.drawRoundedRectangle (b.reduced (0.5f), 4.f, 1.f);

        // Label "RATE" à gauche + valeur Hz à droite
        float half = b.getWidth() * 0.5f;
        g.setFont (juce::FontOptions (9.f));
        g.setColour (col.withAlpha (0.55f));
        g.drawText ("RATE", b.withWidth (half), juce::Justification::centred);
        g.setFont (juce::FontOptions (10.f, juce::Font::bold));
        g.setColour (col);
        g.drawText (formatHz (slider.getValue()),
                    b.withX (b.getX() + half).withWidth (half),
                    juce::Justification::centred);
    }

private:
    static juce::String formatHz (double hz)
    {
        if (hz < 0.1)  return juce::String (hz, 3) + " Hz";
        if (hz < 1.0)  return juce::String (hz, 2) + " Hz";
        if (hz < 10.0) return juce::String (hz, 1) + " Hz";
        return juce::String ((int)std::round (hz)) + " Hz";
    }

    juce::Colour  col;
    juce::Slider  slider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
};

// ============================================================
//  StepDisplay — entier visuel style Serum (OCT, SEMI, UNI)
// ============================================================
class StepDisplay : public juce::Component,
                    private juce::Slider::Listener
{
public:
    StepDisplay (const juce::String& lbl,
                 juce::AudioProcessorValueTreeState& apvts,
                 const juce::String& paramID,
                 juce::Colour colour = NS_VIOLET)
        : col(colour)
    {
        slider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
        slider.setAlpha (0.0f);
        slider.addListener (this);
        addAndMakeVisible (slider);

        label.setText (lbl, juce::dontSendNotification);
        label.setJustificationType (juce::Justification::centred);
        label.setFont (juce::FontOptions (10.0f));
        label.setColour (juce::Label::textColourId, NS_TEXT2);
        addAndMakeVisible (label);

        att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>
            (apvts, paramID, slider);

        auto r = slider.getRange();
        slider.setRange (r.getStart(), r.getEnd(), 1.0);

        auto* p = apvts.getParameter (paramID);
        defaultVal = (float)(int)std::round (
            p->getNormalisableRange().convertFrom0to1 (p->getDefaultValue()));

        slider.addMouseListener (this, false);
    }

    ~StepDisplay() override { slider.removeListener (this); }

    void paint (juce::Graphics& g) override
    {
        auto full = getLocalBounds();
        auto box  = full.withTrimmedBottom (29).reduced (3, 2);

        g.setColour (juce::Colour(0x55080815));
        g.fillRoundedRectangle (box.toFloat(), 4.0f);
        g.setColour (col.withAlpha (0.45f));
        g.drawRoundedRectangle (box.toFloat().reduced(0.5f), 4.0f, 1.0f);

        int val = (int)slider.getValue();
        juce::String text = (val > 0 ? "+" : "") + juce::String(val);
        g.setColour (col);
        g.setFont (juce::FontOptions (14.0f, juce::Font::bold));
        g.drawText (text, box, juce::Justification::centred);

        g.setColour (col.withAlpha (0.4f));
        g.setFont (juce::FontOptions (8.0f));
        g.drawText (juce::String::charToString (0x25B2),
                    box.removeFromRight (10).removeFromTop (box.getHeight()/2),
                    juce::Justification::centred);
    }

    void resized() override
    {
        auto b = getLocalBounds();
        label.setBounds    (b.removeFromBottom (13));
        slider.setBounds   (b);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isRightButtonDown())
        {
            juce::PopupMenu menu;
            menu.addItem (1, "Reset to default");
            menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&slider),
                [this](int result)
                {
                    if (result == 1)
                        slider.setValue (defaultVal, juce::sendNotification);
                });
        }
    }

    juce::Slider slider;

private:
    void sliderValueChanged (juce::Slider*) override { repaint(); }
    juce::Colour     col;
    float            defaultVal { 0.0f };
    juce::Label      label;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
};

// ============================================================
//  AdsrDisplay — courbe ADSR visuelle (drag-and-droppable comme LFO)
// ============================================================
class AdsrDisplay : public juce::Component,
                    public juce::AudioProcessorValueTreeState::Listener
{
public:
    int envIndex { 0 };  // 0=ENV1, 1=ENV2, 2=ENV3

    AdsrDisplay (juce::AudioProcessorValueTreeState& apvts,
                 const juce::String& attackID, const juce::String& decayID,
                 const juce::String& sustainID, const juce::String& releaseID,
                 juce::Colour colour)
        : col(colour)
    {
        struct P { const juce::String& id; float& val; };
        for (auto [id, ref] : std::initializer_list<P> {
            {attackID, attack}, {decayID, decay}, {sustainID, sustain}, {releaseID, release}
        }) {
            apvts.addParameterListener (id, this);
            ref = *apvts.getRawParameterValue (id);
            ids.push_back (id);
        }
        apvts_ptr = &apvts;
    }

    ~AdsrDisplay() override
    {
        for (auto& id : ids) apvts_ptr->removeParameterListener (id, this);
    }

    void parameterChanged (const juce::String& id, float v) override
    {
        if (id == ids[0]) attack  = v;
        if (id == ids[1]) decay   = v;
        if (id == ids[2]) sustain = v;
        if (id == ids[3]) release = v;
        juce::MessageManager::callAsync ([this] { repaint(); });
    }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat().reduced (2.0f);
        g.setColour (NS_BG2);
        g.fillRoundedRectangle (b, 4.0f);
        g.setColour (col.withAlpha(0.25f));
        g.drawRoundedRectangle (b.reduced(0.5f), 4.0f, 1.0f);

        float total = attack + decay + 0.3f + release;
        float ax = attack / total, dx = decay / total;
        float sx = 0.3f / total, rx = release / total;
        (void)rx;

        float w = b.getWidth(), h = b.getHeight();
        float x0 = b.getX(), bot = b.getBottom() - 2.0f, top = b.getY() + 2.0f;

        float x1 = x0 + ax*w, x2 = x1 + dx*w, x3 = x2 + sx*w, x4 = x3 + (release/total)*w;
        float susY = top + (1.0f - sustain) * (bot - top);

        juce::Path path;
        path.startNewSubPath (x0, bot);
        path.lineTo (x1, top);
        path.lineTo (x2, susY);
        path.lineTo (x3, susY);
        path.lineTo (x4, bot);

        juce::Path fill = path;
        fill.lineTo (x0, bot);
        fill.closeSubPath();
        g.setColour (col.withAlpha(0.15f));
        g.fillPath (fill);
        g.setColour (col);
        g.strokePath (path, juce::PathStrokeType (1.8f));
        g.setColour (col.brighter(0.3f));
        for (auto [px, py] : std::initializer_list<std::pair<float,float>> {
            {x1,top},{x2,susY},{x3,susY}
        })
            g.fillEllipse (px-2.5f, py-2.5f, 5.0f, 5.0f);

        // Hint for drag
        if (envIndex >= 1)  // ENV2 and ENV3 are assignable
        {
            g.setColour (col.withAlpha (0.35f));
            g.setFont (juce::FontOptions (7.5f));
            g.drawText ("drag: router ENV",
                        b.removeFromBottom(12), juce::Justification::centred);
        }
    }

    void setProcessor (NovaSynthProcessor* p) { proc_ptr = p; }

    // mouseDown requis pour recevoir les events mouseDrag dans JUCE
    void mouseDown (const juce::MouseEvent& e) override
    {
        envDragActive = false;

        // Clic droit : menu pour reset les modulations (ENV2/ENV3 seulement)
        if (e.mods.isRightButtonDown() && envIndex >= 1 && proc_ptr != nullptr)
        {
            int modIdx = envIndex + 3;  // ENV2 -> 4, ENV3 -> 5
            juce::PopupMenu m;
            m.addItem (1, "Reset modulations (ENV " + juce::String(envIndex + 1) + ")");
            m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this),
                [this, modIdx](int r) {
                    if (r == 1 && proc_ptr != nullptr)
                        proc_ptr->clearModSource (modIdx);
                });
        }
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        // Seuls ENV2 (index 1) et ENV3 (index 2) sont des sources de mod libres
        if (envIndex < 1) return;
        if (!envDragActive && e.getDistanceFromDragStart() > 8)
        {
            envDragActive = true;
            if (auto* dc = juce::DragAndDropContainer::findParentDragContainerFor (this))
            {
                juce::Image img (juce::Image::ARGB, 60, 20, true);
                juce::Graphics ig (img);
                ig.setColour (col.withAlpha (0.92f));
                ig.fillRoundedRectangle (1.f, 1.f, 58.f, 18.f, 4.f);
                ig.setColour (juce::Colours::white);
                ig.setFont (juce::FontOptions (9.f, juce::Font::bold));
                ig.drawText ("ENV " + juce::String(envIndex + 1),
                             juce::Rectangle<int>(0, 3, 60, 14), juce::Justification::centred);
                dc->startDragging ("ENV:" + juce::String(envIndex), this,
                                   juce::ScaledImage(img), false);
            }
        }
    }

    void mouseUp (const juce::MouseEvent&) override { envDragActive = false; }

private:
    bool envDragActive { false };
    juce::AudioProcessorValueTreeState* apvts_ptr { nullptr };
    NovaSynthProcessor* proc_ptr { nullptr };
    std::vector<juce::String> ids;
    juce::Colour col;
    float attack{0.01f}, decay{0.1f}, sustain{0.7f}, release{0.3f};
};

// ============================================================
//  LabelledCombo
// ============================================================
class LabelledCombo : public juce::Component
{
public:
    juce::ComboBox combo;
    juce::Label    label;

    LabelledCombo (const juce::String& lbl,
                   juce::AudioProcessorValueTreeState& apvts,
                   const juce::String& paramID,
                   juce::StringArray choices)
    {
        combo.addItemList (choices, 1);
        combo.setColour (juce::ComboBox::backgroundColourId, juce::Colour(0xff141428));
        combo.setColour (juce::ComboBox::textColourId,       juce::Colour(0xffe2e8f0));
        combo.setColour (juce::ComboBox::outlineColourId,    juce::Colour(0xff3d3d7a));
        combo.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (combo);

        label.setText (lbl, juce::dontSendNotification);
        label.setJustificationType (juce::Justification::centred);
        label.setFont (juce::FontOptions (8.0f));
        label.setColour (juce::Label::textColourId, juce::Colour(0xff7a8aaa));
        addAndMakeVisible (label);

        att = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>
            (apvts, paramID, combo);
    }

    void resized() override
    {
        auto b = getLocalBounds();
        label.setBounds (b.removeFromBottom (14));
        combo.setBounds (b.reduced (2, 3));
    }

private:
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> att;
};

// ============================================================
//  ToggleBtn
// ============================================================
class ToggleBtn : public juce::Component
{
public:
    juce::TextButton btn;

    ToggleBtn (const juce::String& lbl,
               juce::AudioProcessorValueTreeState& apvts,
               const juce::String& paramID,
               juce::Colour onColour = NS_VIOLET)
    {
        btn.setButtonText (lbl);
        btn.setClickingTogglesState (true);
        btn.setColour (juce::TextButton::buttonColourId,   juce::Colour(0xff141428));
        btn.setColour (juce::TextButton::buttonOnColourId, onColour);
        btn.setColour (juce::TextButton::textColourOffId,  juce::Colour(0xff6070a0));
        btn.setColour (juce::TextButton::textColourOnId,   juce::Colours::white);
        addAndMakeVisible (btn);

        att = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>
            (apvts, paramID, btn);
    }

    void resized() override { btn.setBounds (getLocalBounds()); }

private:
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> att;
};

// ============================================================
//  UnisonDisplay — visualisation stéréo des voix unison
// ============================================================
class UnisonDisplay : public juce::Component,
                      public juce::AudioProcessorValueTreeState::Listener
{
public:
    UnisonDisplay (juce::AudioProcessorValueTreeState& apvts,
                   const juce::String& prefix, juce::Colour col)
        : col(col), prefix(prefix), apvts_ptr(&apvts)
    {
        for (auto* id : { "UniVoices", "UniDetune", "UniBlend" })
            apvts.addParameterListener (prefix + id, this);
        voices = (int)*apvts.getRawParameterValue (prefix + "UniVoices");
        detune = *apvts.getRawParameterValue (prefix + "UniDetune");
        blend  = *apvts.getRawParameterValue (prefix + "UniBlend");
    }

    ~UnisonDisplay() override
    {
        for (auto* id : { "UniVoices", "UniDetune", "UniBlend" })
            apvts_ptr->removeParameterListener (prefix + id, this);
    }

    void parameterChanged (const juce::String& id, float v) override
    {
        if      (id == prefix + "UniVoices") voices = (int)v;
        else if (id == prefix + "UniDetune") detune = v;
        else if (id == prefix + "UniBlend")  blend  = v;
        juce::MessageManager::callAsync ([this] { repaint(); });
    }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat().reduced (2.f, 2.f);

        g.setColour (juce::Colour (0x55060610));
        g.fillRoundedRectangle (b, 3.f);
        g.setColour (col.withAlpha (0.22f));
        g.drawRoundedRectangle (b.reduced (0.5f), 3.f, 1.f);

        // Ligne centrale
        float cy  = b.getCentreY();
        float lx0 = b.getX() + 4, lx1 = b.getRight() - 4;
        g.setColour (col.withAlpha (0.18f));
        g.drawHorizontalLine ((int)cy, lx0, lx1);

        // Graduations stéréo (L / C / R)
        g.setColour (col.withAlpha (0.25f));
        g.setFont (juce::FontOptions (7.f));
        g.drawText ("L", juce::Rectangle<float>(lx0, b.getY(), 10, b.getHeight()), juce::Justification::centred);
        g.drawText ("C", juce::Rectangle<float>(b.getCentreX()-5, b.getY(), 10, b.getHeight()), juce::Justification::centred);
        g.drawText ("R", juce::Rectangle<float>(lx1-10, b.getY(), 10, b.getHeight()), juce::Justification::centred);

        // Points de voix unison
        int  v     = juce::jlimit (1, 8, voices);
        float midX = b.getCentreX();
        float halfW = (lx1 - lx0 - 20.f) * 0.5f * juce::jlimit (0.f, 1.f, blend);
        float dotR  = 4.f;

        for (int i = 0; i < v; ++i)
        {
            float pos  = (v > 1) ? (float)(i * 2 - (v - 1)) / (float)(v - 1) : 0.f;
            float x    = midX + pos * halfW;
            float dist = std::abs (pos);
            float a    = 1.f - dist * 0.45f;

            // Halo de detune
            if (detune > 0.01f)
            {
                float glowR = dotR + detune * 10.f;
                g.setColour (col.withAlpha (0.10f * a));
                g.fillEllipse (x - glowR, cy - glowR, glowR * 2.f, glowR * 2.f);
            }

            // Corps du point
            g.setColour (col.withAlpha (0.30f * a));
            g.fillEllipse (x - dotR, cy - dotR, dotR * 2.f, dotR * 2.f);
            g.setColour (col.withAlpha (a));
            g.drawEllipse (x - dotR, cy - dotR, dotR * 2.f, dotR * 2.f, 1.5f);

            // Reflet blanc central
            g.setColour (juce::Colours::white.withAlpha (0.65f * a));
            g.fillEllipse (x - 2.f, cy - 2.f, 4.f, 4.f);
        }

        // Indicateur detune en cents (coin droit)
        if (detune > 0.01f)
        {
            g.setColour (col.withAlpha (0.55f));
            g.setFont (juce::FontOptions (7.5f));
            g.drawText (juce::String ((int)(detune * 100)) + " ct",
                        juce::Rectangle<float> (b.getRight() - 32, b.getY(), 30, b.getHeight()),
                        juce::Justification::centredRight);
        }
    }

private:
    juce::Colour col;
    juce::String prefix;
    juce::AudioProcessorValueTreeState* apvts_ptr { nullptr };
    int   voices { 1 };
    float detune { 0.f };
    float blend  { 0.f };
};

// ============================================================
//  WaveDropButton — single dropdown button for waveform selection
// ============================================================
class WaveDropButton : public juce::Component,
                       public juce::AudioProcessorValueTreeState::Listener
{
public:
    WaveDropButton (juce::AudioProcessorValueTreeState& apvts,
                    const juce::String& paramID,
                    juce::Colour col)
        : apvts(apvts), paramID(paramID), col(col)
    {
        btn.setButtonText (getWaveName ((int)*apvts.getRawParameterValue (paramID)));
        btn.onClick = [this] { showMenu(); };
        addAndMakeVisible (btn);
        apvts.addParameterListener (paramID, this);
        styleBtn();
    }

    ~WaveDropButton() override { apvts.removeParameterListener (paramID, this); }

    void parameterChanged (const juce::String&, float v) override
    {
        juce::MessageManager::callAsync ([this, v] {
            btn.setButtonText (getWaveName ((int)v));
        });
    }

    void resized() override { btn.setBounds (getLocalBounds()); }

private:
    static juce::String getWaveName (int w)
    {
        switch (w) {
            case 0: return juce::String::charToString(0x25B6) + "  SAW";
            case 1: return juce::String::charToString(0x25A0) + "  SQUARE";
            case 2: return juce::String::charToString(0x25B3) + "  TRIANGLE";
            case 3: return "~  SINE";
            default: return "SAW";
        }
    }

    void styleBtn()
    {
        btn.setColour (juce::TextButton::buttonColourId,  juce::Colour(0xff0d0d22));
        btn.setColour (juce::TextButton::buttonOnColourId, col.withAlpha(0.3f));
        btn.setColour (juce::TextButton::textColourOffId,  col);
        btn.setColour (juce::TextButton::textColourOnId,   juce::Colours::white);
    }

    void showMenu()
    {
        juce::PopupMenu m;
        int cur = (int)*apvts.getRawParameterValue (paramID);
        m.addItem (1, juce::String::charToString(0x25B6) + "  SAW",      true, cur == 0);
        m.addItem (2, juce::String::charToString(0x25A0) + "  SQUARE",   true, cur == 1);
        m.addItem (3, juce::String::charToString(0x25B3) + "  TRIANGLE", true, cur == 2);
        m.addItem (4, "~  SINE",                                          true, cur == 3);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (btn),
            [this] (int result) {
                if (result > 0) {
                    auto* param = apvts.getParameter (paramID);
                    if (param)
                        param->setValueNotifyingHost (
                            param->getNormalisableRange().convertTo0to1 ((float)(result - 1)));
                }
            });
    }

    juce::AudioProcessorValueTreeState& apvts;
    juce::String paramID;
    juce::Colour col;
    juce::TextButton btn;
};

// ============================================================
//  WaveSelectBar — 4 boutons SAW/SQR/TRI/SIN style Serum
// ============================================================
class WaveSelectBar : public juce::Component,
                      public juce::AudioProcessorValueTreeState::Listener
{
public:
    std::function<void(int)> onChange;

    WaveSelectBar (juce::AudioProcessorValueTreeState& apvts,
                   const juce::String& paramID,
                   juce::Colour col)
        : apvts(apvts), paramID(paramID), col(col)
    {
        juce::StringArray waves {"SAW", "SQR", "TRI", "SIN"};
        for (int i = 0; i < 4; ++i)
        {
            auto* btn = &buttons[i];
            btn->setButtonText (waves[i]);
            btn->setClickingTogglesState (false);
            btn->onClick = [this, i] {
                auto* p = this->apvts.getParameter (this->paramID);
                if (p) p->setValueNotifyingHost (
                    p->getNormalisableRange().convertTo0to1 ((float)i));
                updateButtons();
            };
            addAndMakeVisible (*btn);
        }
        apvts.addParameterListener (paramID, this);
        updateButtons();
    }

    ~WaveSelectBar() override { apvts.removeParameterListener (paramID, this); }

    void parameterChanged (const juce::String&, float) override
    {
        juce::MessageManager::callAsync ([this] { updateButtons(); });
    }

    void resized() override
    {
        int bw = getWidth() / 4;
        for (int i = 0; i < 4; ++i)
            buttons[i].setBounds (i * bw, 0, bw, getHeight());
    }

    void paint (juce::Graphics&) override {}

private:
    void updateButtons()
    {
        auto* rawParam = apvts.getRawParameterValue (paramID);
        int cur = rawParam ? (int)std::round (*rawParam) : 0;
        for (int i = 0; i < 4; ++i)
        {
            bool sel = (i == cur);
            buttons[i].setColour (juce::TextButton::buttonColourId,
                sel ? col.withAlpha (0.8f) : juce::Colour (0xff0d0d1e));
            buttons[i].setColour (juce::TextButton::textColourOffId,
                sel ? juce::Colours::white : col.withAlpha (0.5f));
        }
    }

    juce::AudioProcessorValueTreeState& apvts;
    juce::String  paramID;
    juce::Colour  col;
    juce::TextButton buttons[4];
};

// ============================================================
//  OscillatorPanel — layout Serum complet
// ============================================================
class OscillatorPanel : public juce::Component
{
public:
    OscillatorPanel (juce::AudioProcessorValueTreeState& apvts,
                     int oscNumber, juce::Colour colour)
        : col(colour), oscNum(oscNumber),
          prefix("osc" + juce::String(oscNumber)),
          display (apvts, prefix+"Waveform", colour),
          waveBtn    (apvts, prefix+"Waveform", colour),
          warpCombo  ("WARP", apvts, prefix+"WarpMode",  {"None","Bend+","Bend-","Sync","Fold","Mirror","PWM"}),
          warpAmt  ("AMT",   apvts, prefix+"WarpAmt",   colour),
          uniVoices("UNI",   apvts, prefix+"UniVoices", colour),
          uniDetune("DET",   apvts, prefix+"UniDetune", colour),
          uniBlend ("BLEND", apvts, prefix+"UniBlend",  colour),
          level    ("LVL",   apvts, prefix+"Level",     colour),
          pan      ("PAN",   apvts, prefix+"Pan",       colour),
          oct      ("OCT",   apvts, prefix+"Oct",       colour),
          semi     ("SEMI",  apvts, prefix+"Tune",      colour),
          fine     ("FINE",  apvts, prefix+"Detune",    colour),
          phase    ("PHASE", apvts, prefix+"Phase",     colour),
          enableBtn("ON",    apvts, prefix+"Enabled",   colour),
          randBtn  ("RAND",  apvts, prefix+"RandPhase", colour),
          uniView  (apvts, prefix, colour)
    {
        display.addParam (prefix+"Level",     &display.level);
        display.addParam (prefix+"Detune",    &display.detune);
        display.addParam (prefix+"Tune",      &display.tune);
        display.addParam (prefix+"WarpAmt",   &display.warpAmt);
        display.addParam (prefix+"WarpMode",  &display.warpModeF);
        display.addParam (prefix+"UniVoices", &display.uniVoicesF);

        addAndMakeVisible (display);
        addAndMakeVisible (uniView);
        addAndMakeVisible (waveBtn);   addAndMakeVisible (warpCombo);
        addAndMakeVisible (warpAmt);   addAndMakeVisible (uniVoices);
        addAndMakeVisible (uniDetune); addAndMakeVisible (uniBlend);
        addAndMakeVisible (level);     addAndMakeVisible (pan);
        addAndMakeVisible (oct);       addAndMakeVisible (semi);
        addAndMakeVisible (fine);      addAndMakeVisible (phase);
        addAndMakeVisible (enableBtn); addAndMakeVisible (randBtn);

        resetAllBtn.setButtonText (juce::String::charToString (0x21BA) + " RST");
        resetAllBtn.setColour (juce::TextButton::buttonColourId,  juce::Colour(0xff0a0a18));
        resetAllBtn.setColour (juce::TextButton::textColourOffId, colour.withAlpha(0.6f));
        resetAllBtn.setColour (juce::TextButton::textColourOnId,  colour);
        resetAllBtn.onClick = [this, &apvts] {
            for (auto id : { prefix+"Waveform", prefix+"WarpMode", prefix+"WarpAmt",
                             prefix+"Oct", prefix+"Tune", prefix+"Detune",
                             prefix+"Level", prefix+"Pan", prefix+"Phase",
                             prefix+"RandPhase", prefix+"UniVoices",
                             prefix+"UniDetune", prefix+"UniBlend", prefix+"Enabled" })
                if (auto* p = apvts.getParameter (id))
                    p->setValueNotifyingHost (p->getDefaultValue());
        };
        addAndMakeVisible (resetAllBtn);
    }

    void setProcessor (NovaSynthProcessor* p)
    {
        warpAmt.setProcessor(p);  uniDetune.setProcessor(p);
        uniBlend.setProcessor(p); level.setProcessor(p);
        pan.setProcessor(p);      fine.setProcessor(p);
        phase.setProcessor(p);
    }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        g.setColour (NS_BG2);
        g.fillRoundedRectangle (b, 6.0f);
        g.setColour (NS_BORDER.interpolatedWith (col, 0.15f));
        g.drawRoundedRectangle (b.reduced(0.5f), 6.0f, 1.0f);
        g.setColour (col.withAlpha(0.07f));
        g.fillRoundedRectangle (b.removeFromTop(20), 5.0f);
        g.setColour (NS_TEXT1);
        g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
        g.drawText ("OSC " + juce::String(oscNum),
                    getLocalBounds().removeFromTop(20).reduced(8,0),
                    juce::Justification::centredLeft);
        // Accent left bar
        g.setColour (col.withAlpha (0.75f));
        g.fillRoundedRectangle (getLocalBounds().toFloat().removeFromLeft(2).removeFromTop(20), 1.0f);
    }

    void resized() override
    {
        // ---- Boutons dans la barre titre ----
        {
            auto header = getLocalBounds().removeFromTop (20);
            enableBtn.setBounds   (header.removeFromRight (30).reduced (2, 2));
            randBtn.setBounds     (header.removeFromRight (36).reduced (2, 2));
            resetAllBtn.setBounds (header.removeFromRight (44).reduced (2, 2));
        }

        auto b = getLocalBounds().reduced (4, 2);
        b.removeFromTop (20);   // skip title row

        // ---- Waveform display (15% hauteur restante) ----
        int dispH = b.getHeight() * 15 / 100;
        display.setBounds (b.removeFromTop (dispH));
        b.removeFromTop (2);

        // ---- Row 1: WAVE dropdown button full width (26px) ----
        waveBtn.setBounds (b.removeFromTop (26));
        b.removeFromTop (2);

        // ---- Row 2: WARP combo (60%) + WarpAmt knob (40%) ----
        {
            auto row = b.removeFromTop (48);
            int warpComboW = row.getWidth() * 60 / 100;
            warpCombo.setBounds (row.removeFromLeft (warpComboW));
            warpAmt.setBounds (row);
        }
        b.removeFromTop (2);

        // ---- Row 3: Level (BIG, 50%) + Pan (50%) ----
        {
            auto row = b.removeFromTop (78);
            int hw = row.getWidth() / 2;
            level.setBounds (row.removeFromLeft (hw));
            pan.setBounds   (row);
        }
        b.removeFromTop (2);

        // ---- Row 4: UniVoices | UniDetune | UniBlend ----
        {
            auto row = b.removeFromTop (65);
            int cw = row.getWidth() / 3;
            uniVoices.setBounds (row.removeFromLeft (cw));
            uniDetune.setBounds (row.removeFromLeft (cw));
            uniBlend.setBounds  (row);
        }
        b.removeFromTop (2);

        // ---- Row 5: Oct | Semi | Fine ----
        {
            auto row = b.removeFromTop (65);
            int cw = row.getWidth() / 3;
            oct.setBounds  (row.removeFromLeft (cw));
            semi.setBounds (row.removeFromLeft (cw));
            fine.setBounds (row);
        }
        b.removeFromTop (2);

        // ---- Row 6 (remaining): UnisonDisplay + Phase (small) ----
        {
            // UnisonDisplay on top half, phase on remaining
            int uniH = juce::jmin (16, b.getHeight() / 2);
            uniView.setBounds (b.removeFromTop (uniH));
            b.removeFromTop (2);
            // Phase in remaining space
            if (b.getHeight() > 0)
                phase.setBounds (b);
        }
    }

private:
    juce::Colour col;
    int          oscNum;
    juce::String prefix;

    WaveformDisplay display;
    UnisonDisplay   uniView;
    WaveDropButton  waveBtn;
    LabelledCombo   warpCombo;
    LabelledKnob    warpAmt, uniDetune, uniBlend, level, pan, fine, phase;
    StepDisplay     uniVoices, oct, semi;
    ToggleBtn       enableBtn, randBtn;
    juce::TextButton resetAllBtn;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OscillatorPanel)
};

// ============================================================
//  OscTabPanel — 3 oscillateurs avec onglets
// ============================================================
class OscTabPanel : public juce::Component
{
public:
    OscTabPanel (juce::AudioProcessorValueTreeState& apvts)
        : panel1 (apvts, 1, NS_VIOLET)
        , panel2 (apvts, 2, NS_PINK)
        , panel3 (apvts, 3, NS_CYAN)
    {
        const char* labels[3] = { "OSC 1", "OSC 2", "OSC 3" };
        juce::Colour cols[3]  = { NS_VIOLET, NS_PINK, NS_CYAN };

        for (int i = 0; i < 3; ++i)
        {
            tabBtns[i].setButtonText (labels[i]);
            tabBtns[i].setClickingTogglesState (false);
            tabBtns[i].setColour (juce::TextButton::buttonColourId,   juce::Colour(0xff080812));
            tabBtns[i].setColour (juce::TextButton::buttonOnColourId, cols[i].withAlpha(0.25f));
            tabBtns[i].setColour (juce::TextButton::textColourOffId,  juce::Colour(0xff4a5580));
            tabBtns[i].setColour (juce::TextButton::textColourOnId,   cols[i]);
            tabBtns[i].setToggleState (i == 0, juce::dontSendNotification);
            tabBtns[i].onClick = [this, i] { showTab (i); };
            addAndMakeVisible (tabBtns[i]);
        }
        addChildComponent (panel1);
        addChildComponent (panel2);
        addChildComponent (panel3);
        showTab (0);
    }

    void setProcessor (NovaSynthProcessor* p)
    {
        panel1.setProcessor (p);
        panel2.setProcessor (p);
        panel3.setProcessor (p);
    }

    void paint (juce::Graphics&) override {}

    void resized() override
    {
        auto b = getLocalBounds();
        auto tabRow = b.removeFromTop (22);
        int tabW = tabRow.getWidth() / 3;
        for (int i = 0; i < 3; ++i)
            tabBtns[i].setBounds (i < 2 ? tabRow.removeFromLeft (tabW) : tabRow);
        panel1.setBounds (b);
        panel2.setBounds (b);
        panel3.setBounds (b);
    }

    void showTab (int idx)
    {
        currentTab = idx;
        panel1.setVisible (idx == 0);
        panel2.setVisible (idx == 1);
        panel3.setVisible (idx == 2);
        for (int i = 0; i < 3; ++i)
            tabBtns[i].setToggleState (i == idx, juce::dontSendNotification);
    }

private:
    int              currentTab { 0 };
    juce::TextButton tabBtns[3];
    OscillatorPanel  panel1, panel2, panel3;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OscTabPanel)
};

// ============================================================
//  EnvLfoPanel — panel combiné ENV + LFO avec onglets (style Serum)
// ============================================================
class EnvLfoPanel : public juce::Component,
                    public juce::AudioProcessorValueTreeState::Listener
{
public:
    EnvLfoPanel (NovaSynthProcessor& proc, juce::AudioProcessorValueTreeState& apvts);
    ~EnvLfoPanel() override;

    void setProcessorForKnobs (NovaSynthProcessor* p);

    void paint        (juce::Graphics& g) override;
    void resized      () override;
    void parameterChanged (const juce::String& id, float v) override;

private:
    void showTab    (int idx);
    void showEnvTab (int idx);
    void showLfoTab (int idx);
    void updateModeButtonStates (int lfoIdx);

    static juce::Colour tabColour (int idx)
    {
        // 0=ENV1(cyan) 1=ENV2(orange) 2=ENV3(green) 3=LFO1(violet) 4=LFO2(pink) 5=LFO3(green) 6=LFO4(sky)
        switch (idx) {
            case 0: return NS_CYAN;
            case 1: return NS_ORANGE;
            case 2: return NS_GREEN;
            case 3: return NS_VIOLET;
            case 4: return NS_PINK;
            case 5: return NS_GREEN;
            case 6: return NS_SKY;
            default: return NS_VIOLET;
        }
    }

    NovaSynthProcessor&                  proc;
    juce::AudioProcessorValueTreeState&  apvts;
    int  currentTab    { 0 };  // legacy (kept for paint() colour lookup)
    int  currentEnvTab { 0 };  // 0=ENV1 1=ENV2 2=ENV3
    int  currentLfoTab { 0 };  // 0=LFO1 1=LFO2 2=LFO3 3=LFO4

    juce::TextButton tabBtns[7];  // [0-2]=ENV1-3, [3-6]=LFO1-4

    // ENV (3 envs × 4 knobs)
    std::unique_ptr<AdsrDisplay>  envDisplay[3];
    std::unique_ptr<LabelledKnob> envKnob[3][4];

    // LFO (4 LFOs)
    std::unique_ptr<LfoCustomDisplay> lfoDisplay[4];
    std::unique_ptr<LabelledKnob>     lfoRateKnob[4];
    std::unique_ptr<LabelledKnob>     lfoShapeKnob[4];
    juce::TextButton                  lfoModeBtns[4][3];   // [lfoIdx][mode: FREE/TRIG/ONE]
    int currentLfoMode[4] = { 0, 0, 0, 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EnvLfoPanel)
};

// ============================================================
//  TransferCurveDisplay — animated transfer curve for Overdrive
// ============================================================
class TransferCurveDisplay : public juce::Component
{
public:
    TransferCurveDisplay() = default;

    void setParams (int mode, float driveNorm)
    {
        this->mode      = mode;
        this->driveGain = 1.0f + driveNorm * 9.0f;
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        g.setColour (juce::Colour (0xff050512));
        g.fillRoundedRectangle (b, 5.f);
        g.setColour (NS_VIOLET.withAlpha (0.30f));
        g.drawRoundedRectangle (b.reduced (0.5f), 5.f, 1.f);

        // Grid lines
        g.setColour (juce::Colour (0xff10102a));
        float cx = b.getCentreX(), cy = b.getCentreY();
        g.drawLine (cx, b.getY()+3.f, cx, b.getBottom()-3.f, 0.5f);
        g.drawLine (b.getX()+3.f, cy, b.getRight()-3.f, cy, 0.5f);
        // Diagonal reference (y=x)
        g.setColour (juce::Colour (0xff181830));
        g.drawLine (b.getX()+3.f, b.getBottom()-3.f, b.getRight()-3.f, b.getY()+3.f, 0.5f);

        // Transfer curve
        juce::Path curve;
        const int n = 200;
        bool started = false;
        for (int i = 0; i < n; i++)
        {
            float xN  = (float)i / (n - 1);
            float xIn = xN * 2.0f - 1.0f;
            float yOut = applyShaper (xIn);
            float xPx  = b.getX() + xN * b.getWidth();
            float yPx  = b.getCentreY() - yOut * b.getHeight() * 0.46f;
            yPx = juce::jlimit (b.getY() + 1.f, b.getBottom() - 1.f, yPx);
            if (!started) { curve.startNewSubPath (xPx, yPx); started = true; }
            else            curve.lineTo (xPx, yPx);
        }
        g.setColour (NS_GREEN);
        g.strokePath (curve, juce::PathStrokeType (1.8f,
            juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

private:
    int   mode      { 0 };
    float driveGain { 1.0f };

    float applyShaper (float x) const
    {
        float in = x * driveGain;
        switch (mode)
        {
            case 0: { float tg = std::tanh (driveGain); return (tg > 1e-6f) ? std::tanh (in) / tg : x; }
            case 1:   return juce::jlimit (-1.0f, 1.0f, in) / std::max (driveGain, 1.0f);
            case 2:
            {
                float bias = 0.15f;
                float biased = in + bias * driveGain;
                float out = (biased >= 0.f)
                    ? std::tanh (biased)
                    : std::atan (biased * 0.8f) * (2.f / juce::MathConstants<float>::pi);
                return juce::jlimit (-1.0f, 1.0f, out - bias * 0.4f);
            }
            case 3:
            {
                if (in > 0.f) return  1.f - std::exp (-in);
                if (in < 0.f) return -(1.f - std::exp ( in));
                return 0.f;
            }
            case 4:
            {
                float v = in;
                for (int j = 0; j < 12; ++j) {
                    if      (v >  1.f) v =  2.f - v;
                    else if (v < -1.f) v = -2.f - v;
                    else break;
                }
                return v;
            }
            case 5:  return std::sin (in * juce::MathConstants<float>::pi * 0.5f);
            case 6:
            {
                float amt  = (driveGain - 1.0f) / 9.0f;
                float bits = juce::jlimit (2.f, 16.f, 16.f - amt * 13.f);
                float lev  = std::pow (2.f, bits);
                return std::round (x * lev) / lev;
            }
            case 7:  return x;
            default: return x;
        }
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TransferCurveDisplay)
};

// ============================================================
//  CompTransferCurveDisplay — compressor transfer curve (Serum OTT style)
//  Shows the input→output gain mapping for one band.
//  X axis: input level (dB, -60 to 0)
//  Y axis: output level (dB, -60 to 0)
// ============================================================
class CompTransferCurveDisplay : public juce::Component, public juce::Timer
{
public:
    CompTransferCurveDisplay (NovaSynthProcessor& proc) : proc(proc)
    {
        startTimerHz (30);
    }
    ~CompTransferCurveDisplay() override { stopTimer(); }

    // Set which band to display (0=Low, 1=Mid, 2=High)
    void setBand (int b, float thresh, float ratio)
    {
        band     = b;
        thrDb    = thresh;
        compRatio = ratio;
        repaint();
    }

    void timerCallback() override { repaint(); }

    void paint (juce::Graphics& g) override
    {
        static const juce::Colour bandCols[3] = { NS_VIOLET, NS_GREEN, NS_PINK };
        auto b = getLocalBounds().toFloat();

        // Background
        g.setColour (juce::Colour (0xff040410));
        g.fillRoundedRectangle (b, 4.f);

        // Grid lines
        g.setColour (juce::Colour (0xff0e0e22));
        int steps = 6;
        for (int i = 1; i < steps; ++i)
        {
            float t = (float)i / steps;
            g.drawHorizontalLine ((int)(b.getY() + t * b.getHeight()),
                                   b.getX() + 2, b.getRight() - 2);
            g.drawVerticalLine   ((int)(b.getX() + t * b.getWidth()),
                                   b.getY() + 2, b.getBottom() - 2);
        }

        // 1:1 diagonal (no compression reference)
        g.setColour (juce::Colour (0xff181838));
        g.drawLine (b.getX() + 2, b.getBottom() - 2, b.getRight() - 2, b.getY() + 2, 0.5f);

        // Threshold vertical marker
        float thrNorm = juce::jlimit (0.f, 1.f, (thrDb + 60.f) / 60.f);
        float thrX    = b.getX() + thrNorm * b.getWidth();
        g.setColour (juce::Colours::white.withAlpha (0.22f));
        g.drawVerticalLine ((int)thrX, b.getY() + 2, b.getBottom() - 2);

        // Transfer curve
        juce::Colour lineCol = (band >= 0 && band < 3) ? bandCols[band] : NS_CYAN;
        juce::Path   curve;
        const float  dBmin = -60.f, dBmax = 0.f;
        const float  invRatio = (compRatio > 1.f) ? (1.f / compRatio) : 1.f;
        bool         started  = false;
        const int    N        = 120;

        for (int i = 0; i <= N; ++i)
        {
            float t      = (float)i / N;
            float inDb   = dBmin + t * (dBmax - dBmin);
            float outDb;
            if (inDb <= thrDb)
                outDb = inDb;                    // below threshold: 1:1
            else
                outDb = thrDb + (inDb - thrDb) * invRatio;  // above: compressed

            outDb = juce::jlimit (dBmin, dBmax, outDb);

            float xPx = b.getX() + t * b.getWidth();
            float yPx = b.getBottom() - ((outDb - dBmin) / (dBmax - dBmin)) * b.getHeight();
            yPx = juce::jlimit (b.getY() + 1.f, b.getBottom() - 1.f, yPx);

            if (!started) { curve.startNewSubPath (xPx, yPx); started = true; }
            else            curve.lineTo (xPx, yPx);
        }

        // Fill under curve
        juce::Path fill = curve;
        fill.lineTo (b.getRight() - 1, b.getBottom() - 1);
        fill.lineTo (b.getX() + 1, b.getBottom() - 1);
        fill.closeSubPath();
        g.setColour (lineCol.withAlpha (0.07f));
        g.fillPath (fill);
        g.setColour (lineCol.withAlpha (0.9f));
        g.strokePath (curve, juce::PathStrokeType (1.8f,
            juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Moving dot at current signal level
        float sigLin  = proc.bandSignalLevel[juce::jlimit(0,2,band)].load();
        float sigDb   = (sigLin > 1e-9f)
            ? juce::jlimit (dBmin, dBmax, 20.f * std::log10 (sigLin))
            : dBmin;
        float dotInNorm  = (sigDb - dBmin) / (dBmax - dBmin);
        float dotOutDb   = (sigDb <= thrDb) ? sigDb : thrDb + (sigDb - thrDb) * invRatio;
        float dotOutNorm = (juce::jlimit (dBmin, dBmax, dotOutDb) - dBmin) / (dBmax - dBmin);

        float dotX = b.getX() + dotInNorm  * b.getWidth();
        float dotY = b.getBottom() - dotOutNorm * b.getHeight();
        dotX = juce::jlimit (b.getX() + 2, b.getRight()  - 2, dotX);
        dotY = juce::jlimit (b.getY() + 2, b.getBottom() - 2, dotY);

        g.setColour (lineCol.withAlpha (0.4f));
        g.fillEllipse (dotX - 5.f, dotY - 5.f, 10.f, 10.f);
        g.setColour (juce::Colours::white.withAlpha (0.9f));
        g.fillEllipse (dotX - 2.5f, dotY - 2.5f, 5.f, 5.f);

        // Border
        g.setColour (lineCol.withAlpha (0.35f));
        g.drawRoundedRectangle (b.reduced (0.5f), 4.f, 1.f);
    }

private:
    NovaSynthProcessor& proc;
    int   band      { 0 };
    float thrDb     { -20.f };
    float compRatio { 4.f   };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CompTransferCurveDisplay)
};

// ============================================================
//  EffectsPanel — Sidebar + content panel (Serum/Vital style)
// ============================================================
class EffectsPanel : public juce::Component, public juce::Timer
{
public:
    EffectsPanel (NovaSynthProcessor& proc, juce::AudioProcessorValueTreeState& apvts)
        : proc (proc)
        // Compressor knobs
        , compInGain  ("IN",    apvts, "compInGain",    NS_ORANGE)
        , compOutGain ("OUT",   apvts, "compOutGain",   NS_ORANGE)
        , compMix     ("MIX",   apvts, "compMix",       NS_ORANGE)
        , compUpward  ("OTT",   apvts, "compUpward",    NS_ORANGE)
        , compDepth   ("DEPTH", apvts, "compDepth",     NS_ORANGE)
        , loThresh    ("THR",   apvts, "compLowThresh", NS_VIOLET)
        , loRatio     ("RATIO", apvts, "compLowRatio",  NS_VIOLET)
        , loGain      ("GAIN",  apvts, "compLowGain",   NS_VIOLET)
        , midThresh   ("THR",   apvts, "compMidThresh", NS_GREEN)
        , midRatio    ("RATIO", apvts, "compMidRatio",  NS_GREEN)
        , midGain     ("GAIN",  apvts, "compMidGain",   NS_GREEN)
        , hiThresh    ("THR",   apvts, "compHiThresh",  NS_PINK)
        , hiRatio     ("RATIO", apvts, "compHiRatio",   NS_PINK)
        , hiGain      ("GAIN",  apvts, "compHiGain",    NS_PINK)
        , compTransferCurve (proc)
        // Overdrive knobs
        , driveAmt    ("DRIVE", apvts, "driveAmount",   NS_GREEN)
        , driveMix    ("MIX",   apvts, "driveMix",      NS_GREEN)
    {
        // Sidebar buttons: COMP, DIST, EQ, REV, DLY
        static const char* fxNames[5] = { "COMP", "DIST", "EQ", "REV", "DLY" };
        for (int i = 0; i < 5; ++i)
        {
            sideBtn[i].setButtonText (fxNames[i]);
            sideBtn[i].setClickingTogglesState (false);
            sideBtn[i].setColour (juce::TextButton::buttonColourId,   juce::Colour(0xff080812));
            sideBtn[i].setColour (juce::TextButton::buttonOnColourId, NS_ORANGE.withAlpha(0.2f));
            sideBtn[i].setColour (juce::TextButton::textColourOffId,  juce::Colour(0xff5060a0));
            sideBtn[i].setColour (juce::TextButton::textColourOnId,   NS_ORANGE);
            sideBtn[i].onClick = [this, i] { selectFx (i); };
            addAndMakeVisible (sideBtn[i]);
        }

        // Compressor band selector buttons (LOW / MID / HIGH)
        static const char* bandNames[3] = { "LOW", "MID", "HIGH" };
        static const juce::Colour bandCols2[3] = { NS_VIOLET, NS_GREEN, NS_PINK };
        for (int i = 0; i < 3; ++i)
        {
            compBandBtns[i].setButtonText (bandNames[i]);
            compBandBtns[i].setClickingTogglesState (false);
            compBandBtns[i].setColour (juce::TextButton::buttonColourId,
                (i == 0) ? bandCols2[i].withAlpha(0.22f) : juce::Colour(0xff080814));
            compBandBtns[i].setColour (juce::TextButton::textColourOffId,
                (i == 0) ? bandCols2[i] : bandCols2[i].withAlpha(0.45f));
            compBandBtns[i].onClick = [this, i, &apvts]
            {
                compCurveSelectedBand = i;
                // Update colours
                static const juce::Colour bc[3] = { NS_VIOLET, NS_GREEN, NS_PINK };
                for (int j = 0; j < 3; ++j)
                {
                    bool sel = (j == i);
                    compBandBtns[j].setColour (juce::TextButton::buttonColourId,
                        sel ? bc[j].withAlpha(0.22f) : juce::Colour(0xff080814));
                    compBandBtns[j].setColour (juce::TextButton::textColourOffId,
                        sel ? bc[j] : bc[j].withAlpha(0.45f));
                }
                // Update transfer curve with selected band params
                static const char* thrIds[3]   = {"compLowThresh","compMidThresh","compHiThresh"};
                static const char* ratioIds[3]  = {"compLowRatio", "compMidRatio", "compHiRatio"};
                float thr   = *apvts.getRawParameterValue (thrIds[i]);
                float ratio = *apvts.getRawParameterValue (ratioIds[i]);
                compTransferCurve.setBand (i, thr, ratio);
            };
            addAndMakeVisible (compBandBtns[i]);
        }
        addAndMakeVisible (compTransferCurve);
        // Init transfer curve for band 0
        compTransferCurve.setBand (0,
            *apvts.getRawParameterValue ("compLowThresh"),
            *apvts.getRawParameterValue ("compLowRatio"));

        // Compressor ON button
        compEnableBtn.setButtonText ("ON");
        compEnableBtn.setClickingTogglesState (true);
        compEnableBtn.setColour (juce::TextButton::buttonColourId,   juce::Colour(0xff141428));
        compEnableBtn.setColour (juce::TextButton::buttonOnColourId, NS_ORANGE.withAlpha(0.5f));
        compEnableBtn.setColour (juce::TextButton::textColourOffId,  juce::Colour(0xff505070));
        compEnableBtn.setColour (juce::TextButton::textColourOnId,   NS_ORANGE);
        compEnableBtn.onClick = [this, &proc] {
            float v = compEnableBtn.getToggleState() ? 1.0f : 0.0f;
            if (auto* p = proc.apvts.getParameter ("compEnabled"))
                p->setValueNotifyingHost (v);
        };
        addAndMakeVisible (compEnableBtn);

        // Drive ON button
        driveEnableBtn.setButtonText ("ON");
        driveEnableBtn.setClickingTogglesState (true);
        driveEnableBtn.setColour (juce::TextButton::buttonColourId,   juce::Colour(0xff0f1a0f));
        driveEnableBtn.setColour (juce::TextButton::buttonOnColourId, NS_GREEN.withAlpha(0.4f));
        driveEnableBtn.setColour (juce::TextButton::textColourOffId,  juce::Colour(0xff405040));
        driveEnableBtn.setColour (juce::TextButton::textColourOnId,   NS_GREEN);
        driveEnableBtn.onClick = [this, &proc] {
            float v = driveEnableBtn.getToggleState() ? 1.0f : 0.0f;
            if (auto* p = proc.apvts.getParameter ("driveEnabled"))
                p->setValueNotifyingHost (v);
        };
        addAndMakeVisible (driveEnableBtn);

        // Drive mode prev/next buttons
        drivePrev.setButtonText ("<");
        driveNext.setButtonText (">");
        for (auto* b2 : { &drivePrev, &driveNext })
        {
            b2->setColour (juce::TextButton::buttonColourId,  juce::Colour(0xff0a1a0a));
            b2->setColour (juce::TextButton::textColourOffId, NS_GREEN);
            addAndMakeVisible (b2);
        }
        drivePrev.onClick = [this, &apvts] {
            int cur = (int)*apvts.getRawParameterValue ("driveMode");
            if (auto* p = apvts.getParameter("driveMode"))
                p->setValueNotifyingHost (p->convertTo0to1 (std::max(0, cur - 1)));
            updateDriveModeButtons (apvts);
        };
        driveNext.onClick = [this, &apvts] {
            int cur = (int)*apvts.getRawParameterValue ("driveMode");
            if (auto* p = apvts.getParameter("driveMode"))
                p->setValueNotifyingHost (p->convertTo0to1 (std::min(7, cur + 1)));
            updateDriveModeButtons (apvts);
        };

        // Drive mode selector buttons (Soft/Hard/Tube/Diode/Fold/Sin/Bit/Down)
        static const char* driveModeNames[8] = {
            "SOFT", "HARD", "TUBE", "DIODE", "FOLD", "SIN", "BIT", "DOWN"
        };
        for (int i = 0; i < 8; ++i)
        {
            driveModeBtn[i].setButtonText (driveModeNames[i]);
            driveModeBtn[i].setClickingTogglesState (false);
            driveModeBtn[i].setColour (juce::TextButton::buttonColourId,  juce::Colour(0xff0a1a0a));
            driveModeBtn[i].setColour (juce::TextButton::textColourOffId, NS_GREEN.withAlpha(0.45f));
            driveModeBtn[i].onClick = [this, i, &apvts]
            {
                if (auto* p = apvts.getParameter("driveMode"))
                    p->setValueNotifyingHost (p->convertTo0to1 ((float)i));
                updateDriveModeButtons (apvts);
            };
            addAndMakeVisible (driveModeBtn[i]);
        }
        updateDriveModeButtons (apvts);

        addAndMakeVisible (transferCurve);
        addAndMakeVisible (driveAmt);
        addAndMakeVisible (driveMix);

        // Comp knobs
        for (auto* k : { &compInGain, &compOutGain, &compMix, &compUpward, &compDepth,
                          &loThresh, &loRatio, &loGain,
                          &midThresh, &midRatio, &midGain,
                          &hiThresh,  &hiRatio,  &hiGain })
        {
            k->setProcessor (&proc);
            addAndMakeVisible (k);
        }
        driveAmt.setProcessor (&proc);
        driveMix.setProcessor (&proc);

        selectFx (0);
        startTimerHz (30);
    }

    ~EffectsPanel() override { stopTimer(); }

    void timerCallback() override
    {
        // Update transfer curve when drive params change
        if (selectedFx == 1)
        {
            int   mode  = (int)*proc.apvts.getRawParameterValue ("driveMode");
            float amt   = *proc.apvts.getRawParameterValue ("driveAmount");
            transferCurve.setParams (mode, amt);
        }
        // Update compressor transfer curve with current band params
        if (selectedFx == 0)
        {
            static const char* thrIds[3]  = {"compLowThresh","compMidThresh","compHiThresh"};
            static const char* ratIds[3]  = {"compLowRatio", "compMidRatio", "compHiRatio"};
            int b2 = juce::jlimit (0, 2, compCurveSelectedBand);
            float thr   = *proc.apvts.getRawParameterValue (thrIds[b2]);
            float ratio = *proc.apvts.getRawParameterValue (ratIds[b2]);
            compTransferCurve.setBand (b2, thr, ratio);
        }
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();

        g.setColour (juce::Colour(0x880a0a16));
        g.fillRoundedRectangle (b, 8.0f);
        g.setColour (NS_ORANGE.withAlpha (0.30f));
        g.drawRoundedRectangle (b.reduced(0.5f), 8.0f, 1.0f);

        auto titleArea = b.removeFromTop (28.f);
        g.setColour (NS_ORANGE.withAlpha (0.10f));
        g.fillRoundedRectangle (titleArea, 6.0f);
        g.setColour (NS_ORANGE);
        g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
        g.drawText ("EFFECTS", titleArea.withTrimmedLeft (8.f), juce::Justification::centredLeft);

        int sideW = 110;
        auto sidebar = getLocalBounds().withTrimmedTop (28).withWidth (sideW).toFloat().reduced (4.f, 4.f);
        g.setColour (juce::Colour (0xff060610));
        g.fillRoundedRectangle (sidebar, 5.0f);
        g.setColour (NS_VIOLET.withAlpha (0.18f));
        g.drawRoundedRectangle (sidebar.reduced(0.5f), 5.0f, 1.0f);

        auto content = getLocalBounds().withTrimmedTop (28).withTrimmedLeft (sideW + 8).toFloat().reduced (4.f, 4.f);
        g.setColour (juce::Colour (0xff07070f));
        g.fillRoundedRectangle (content, 5.0f);
        g.setColour (NS_ORANGE.withAlpha (0.20f));
        g.drawRoundedRectangle (content.reduced(0.5f), 5.0f, 1.0f);

        if (selectedFx == 0)
            paintCompressor (g, content);
        else if (selectedFx == 1)
            paintDriveHeader (g, content);
        else
            paintComingSoon (g, content);
    }

    void resized() override
    {
        int sideW = 110;
        const int top = 28;

        auto sidebar = getLocalBounds().withTrimmedTop (top).withWidth (sideW).reduced (6, 6);
        int bh = 28;
        for (int i = 0; i < 5; ++i)
            sideBtn[i].setBounds (sidebar.removeFromTop (bh).reduced (0, 2));

        auto content = getLocalBounds().withTrimmedTop (top).withTrimmedLeft (sideW + 8).reduced (6, 6);

        // Hide all widgets first
        compEnableBtn.setBounds ({});
        driveEnableBtn.setBounds ({});
        drivePrev.setBounds ({});
        driveNext.setBounds ({});
        transferCurve.setBounds ({});
        for (auto* k : { &compInGain, &compOutGain, &compMix, &compUpward, &compDepth,
                          &loThresh, &loRatio, &loGain,
                          &midThresh, &midRatio, &midGain,
                          &hiThresh,  &hiRatio,  &hiGain })
            k->setBounds ({});
        driveAmt.setBounds ({});
        driveMix.setBounds ({});
        for (int i = 0; i < 8; ++i) driveModeBtn[i].setBounds ({});

        if (selectedFx == 0) // COMP
        {
            // Header: [ON] [DEPTH] [IN] [OUT] [MIX] [OTT]
            auto header = content.removeFromTop (52);
            int hkw = header.getWidth() / 6;
            compEnableBtn.setBounds (header.removeFromLeft (hkw).reduced (4, 8));
            compDepth.setBounds     (header.removeFromLeft (hkw));
            compInGain.setBounds    (header.removeFromLeft (hkw));
            compOutGain.setBounds   (header.removeFromLeft (hkw));
            compMix.setBounds       (header.removeFromLeft (hkw));
            compUpward.setBounds    (header);
            content.removeFromTop (6);

            // Animated meter bands: 40% of remaining height
            int meterH = content.getHeight() * 40 / 100;
            content.removeFromTop ((float)meterH);  // painted only
            content.removeFromTop (4);

            // Band knob rows: LO | MID | HI — each column has THR, RATIO, GAIN
            int bw = content.getWidth() / 3;
            auto lo  = content.withWidth (bw).reduced (2, 2);
            auto mid = content.withX (content.getX() + bw).withWidth (bw).reduced (2, 2);
            auto hi  = content.withX (content.getX() + bw*2).withWidth (bw).reduced (2, 2);
            lo.removeFromTop (18); mid.removeFromTop (18); hi.removeFromTop (18);
            int kw3 = lo.getWidth() / 3;
            loThresh.setBounds  (lo.removeFromLeft(kw3));  loRatio.setBounds  (lo.removeFromLeft(kw3));  loGain.setBounds  (lo);
            midThresh.setBounds (mid.removeFromLeft(kw3)); midRatio.setBounds (mid.removeFromLeft(kw3)); midGain.setBounds (mid);
            hiThresh.setBounds  (hi.removeFromLeft(kw3));  hiRatio.setBounds  (hi.removeFromLeft(kw3));  hiGain.setBounds  (hi);
        }
        else if (selectedFx == 1) // DIST
        {
            // Header row: [ON] [< >]
            auto header = content.removeFromTop (36);
            driveEnableBtn.setBounds (header.removeFromLeft (44).reduced (2, 4));
            drivePrev.setBounds      (header.removeFromLeft (24).reduced (2, 4));
            driveNext.setBounds      (header.removeFromRight(24).reduced (2, 4));
            content.removeFromTop (4);

            // Drive mode selector row: 8 buttons
            auto modeRow = content.removeFromTop (28);
            int btnW = modeRow.getWidth() / 8;
            for (int i = 0; i < 8; ++i)
                driveModeBtn[i].setBounds (modeRow.removeFromLeft (btnW).reduced (2, 2));
            content.removeFromTop (6);

            // Transfer curve: left 60%, knobs: right 40%
            auto curveArea = content.removeFromLeft (content.getWidth() * 6 / 10);
            transferCurve.setBounds (curveArea.reduced (4));

            // Knobs on right
            int kh = content.getHeight() / 2;
            driveAmt.setBounds (content.removeFromTop (kh).reduced (4, 4));
            driveMix.setBounds (content.reduced (4, 4));
        }
    }

private:
    void selectFx (int idx)
    {
        selectedFx = idx;
        for (int i = 0; i < 5; ++i)
        {
            bool active = (i == idx);
            sideBtn[i].setColour (juce::TextButton::buttonColourId,
                active ? NS_ORANGE.withAlpha(0.18f) : juce::Colour(0xff080812));
            sideBtn[i].setColour (juce::TextButton::textColourOffId,
                active ? NS_ORANGE : juce::Colour(0xff5060a0));
        }
        bool showComp  = (idx == 0);
        bool showDrive = (idx == 1);
        compEnableBtn.setVisible  (showComp);
        driveEnableBtn.setVisible (showDrive);
        drivePrev.setVisible      (showDrive);
        driveNext.setVisible      (showDrive);
        transferCurve.setVisible  (showDrive);
        driveAmt.setVisible       (showDrive);
        driveMix.setVisible       (showDrive);
        for (int i = 0; i < 8; ++i) driveModeBtn[i].setVisible (showDrive);
        for (auto* k : { &compInGain, &compOutGain, &compMix, &compUpward, &compDepth,
                          &loThresh, &loRatio, &loGain,
                          &midThresh, &midRatio, &midGain,
                          &hiThresh,  &hiRatio,  &hiGain })
            k->setVisible (showComp);
        resized();
        repaint();
    }

    // ---- OTT-style compressor painter ----
    void paintCompressor (juce::Graphics& g, juce::Rectangle<float> content)
    {
        auto titleR = content.removeFromTop (18.f);
        g.setColour (NS_ORANGE);
        g.setFont (juce::FontOptions (10.5f, juce::Font::bold));
        g.drawText ("MULTIBAND COMPRESSOR", titleR, juce::Justification::centred);

        content.removeFromTop (52.f + 6.f);   // skip header knobs

        // ---- Animated OTT band display ----
        int meterH = (int)(content.getHeight() * 40 / 100);
        auto meterRegion = content.removeFromTop ((float)meterH).reduced (4.f, 2.f);
        content.removeFromTop (4.f);

        struct Band { juce::Colour col; const char* name; const char* thrId; };
        Band bands[3] = { {NS_VIOLET,"LOW","compLowThresh"}, {NS_GREEN,"MID","compMidThresh"}, {NS_PINK,"HIGH","compHiThresh"} };
        int bw = (int)meterRegion.getWidth() / 3;

        for (int i = 0; i < 3; ++i)
        {
            auto br = meterRegion.withX (meterRegion.getX() + i * bw).withWidth ((float)bw).reduced (4.f, 2.f);

            // Background
            g.setColour (juce::Colour (0xff050510));
            g.fillRoundedRectangle (br, 4.f);
            g.setColour (bands[i].col.withAlpha (0.18f));
            g.drawRoundedRectangle (br.reduced(0.5f), 4.f, 1.f);

            // Threshold line position (map -60..0 dB to vertical)
            float thrDb  = *proc.apvts.getRawParameterValue (bands[i].thrId);
            float thrNorm = juce::jlimit (0.0f, 1.0f, (thrDb + 60.f) / 60.f);
            float thrY   = br.getBottom() - thrNorm * br.getHeight();

            // Signal level
            float sigLin  = proc.bandSignalLevel[i].load();
            float sigDb   = (sigLin > 1e-9f) ? juce::jlimit (-60.f, 0.f, 20.f * std::log10 (sigLin))
                                              : -60.f;
            float sigNorm = (sigDb + 60.f) / 60.f;
            float sigY    = br.getBottom() - sigNorm * br.getHeight();

            // Compression zone (above threshold) — dim highlight when GR active
            float gr = proc.bandGainReduction[i].load();
            if (sigY < thrY && gr < 0.95f)
            {
                auto compZone = br.withTop (sigY).withBottom (thrY);
                g.setColour (bands[i].col.withAlpha (0.15f + 0.25f * (1.f - gr)));
                g.fillRoundedRectangle (compZone, 2.f);
            }

            // Signal level bar (bottom to signal level)
            if (sigNorm > 0.01f)
            {
                auto sigBar = br.withTop (sigY);
                juce::Colour barCol = (sigDb > thrDb)
                    ? juce::Colour(0xffff5566).withAlpha(0.8f)
                    : bands[i].col.withAlpha (0.5f);
                g.setColour (barCol);
                g.fillRoundedRectangle (sigBar, 2.f);
            }

            // Threshold marker line
            g.setColour (juce::Colours::white.withAlpha (0.50f));
            g.drawLine (br.getX(), thrY, br.getRight(), thrY, 1.0f);

            // GR gain reduction bar (from top)
            float grBar = (1.f - gr) * br.getHeight();
            if (grBar > 1.f)
            {
                auto grRect = br.withHeight (grBar);
                g.setColour (juce::Colour(0xffff3333).withAlpha(0.35f));
                g.fillRoundedRectangle (grRect, 2.f);
            }

            // Band label
            g.setColour (bands[i].col);
            g.setFont (juce::FontOptions (9.f, juce::Font::bold));
            g.drawText (bands[i].name, br.withHeight (13.f), juce::Justification::centred);

            // GR dB text
            float grDb2 = 20.f * std::log10 (juce::jmax (1e-6f, gr));
            if (grDb2 < -0.5f)
            {
                g.setColour (bands[i].col.withAlpha (0.8f));
                g.setFont (juce::FontOptions (8.f));
                g.drawText (juce::String((int)grDb2) + "dB",
                            br.withTop (br.getBottom() - 13.f), juce::Justification::centred);
            }
        }

        // ---- Band column label rows ----
        bw = (int)content.getWidth() / 3;
        for (int i = 0; i < 3; ++i)
        {
            auto br = content.withX (content.getX() + i * bw).withWidth ((float)bw).reduced (4.f, 2.f);
            g.setColour (bands[i].col.withAlpha (0.07f));
            g.fillRoundedRectangle (br, 5.f);
            g.setColour (bands[i].col.withAlpha (0.35f));
            g.drawRoundedRectangle (br.reduced(0.5f), 5.f, 1.f);
            g.setColour (bands[i].col);
            g.setFont (juce::FontOptions (9.f, juce::Font::bold));
            g.drawText (bands[i].name, br.withHeight (18.f), juce::Justification::centred);
        }
    }

    // ---- Overdrive header painter (mode name) ----
    void paintDriveHeader (juce::Graphics& g, juce::Rectangle<float> content)
    {
        static const char* modeNames[8] = {
            "SOFT CLIP", "HARD CLIP", "TUBE", "DIODE",
            "LIN FOLD", "SIN FOLD", "BITCRUSH", "DOWNSAMP"
        };
        int mode = (int)*proc.apvts.getRawParameterValue ("driveMode");
        auto headerR = content.removeFromTop (36.f);
        g.setColour (NS_GREEN);
        g.setFont (juce::FontOptions (11.f, juce::Font::bold));
        g.drawText (modeNames[juce::jlimit(0,7,mode)],
                    headerR.withTrimmedLeft(70.f).withTrimmedRight(30.f),
                    juce::Justification::centred);
    }

    void paintComingSoon (juce::Graphics& g, juce::Rectangle<float> content)
    {
        g.setColour (juce::Colour (0xff404060));
        g.setFont (juce::FontOptions (12.f));
        g.drawText ("Coming soon", content, juce::Justification::centred);
    }

    NovaSynthProcessor&   proc;
    int                   selectedFx { 0 };
    juce::TextButton      sideBtn[5];
    // Compressor
    juce::TextButton         compEnableBtn;
    LabelledKnob             compInGain, compOutGain, compMix, compUpward, compDepth;
    LabelledKnob             loThresh, loRatio, loGain;
    LabelledKnob             midThresh, midRatio, midGain;
    LabelledKnob             hiThresh, hiRatio, hiGain;
    // Compressor transfer curve (one per band, shown in selector area)
    int                      compCurveSelectedBand { 0 };
    CompTransferCurveDisplay compTransferCurve;
    juce::TextButton         compBandBtns[3];   // LOW / MID / HIGH band selectors
    // Overdrive
    juce::TextButton      driveEnableBtn;
    juce::TextButton      drivePrev, driveNext;
    juce::TextButton      driveModeBtn[8];
    TransferCurveDisplay  transferCurve;
    LabelledKnob          driveAmt, driveMix;

    void updateDriveModeButtons (juce::AudioProcessorValueTreeState& apvts)
    {
        int cur = (int)*apvts.getRawParameterValue ("driveMode");
        for (int i = 0; i < 8; ++i)
        {
            bool active = (i == cur);
            driveModeBtn[i].setColour (juce::TextButton::buttonColourId,
                active ? NS_GREEN.withAlpha(0.35f) : juce::Colour(0xff0a1a0a));
            driveModeBtn[i].setColour (juce::TextButton::textColourOffId,
                active ? NS_GREEN : NS_GREEN.withAlpha(0.45f));
            driveModeBtn[i].repaint();
        }
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EffectsPanel)
};

// ============================================================
//  NovaSynthEditor
// ============================================================
class NovaSynthEditor : public juce::AudioProcessorEditor,
                        public juce::DragAndDropContainer,
                        private juce::Timer
{
public:
    NovaSynthEditor  (NovaSynthProcessor&);
    ~NovaSynthEditor () override;

    void paint   (juce::Graphics&) override;
    void resized () override;

    bool keyPressed  (const juce::KeyPress& key) override;
    bool keyStateChanged (bool isKeyDown) override;

    void timerCallback() override
    {
        // Maintenir le focus clavier quand le clavier d'ordi est actif
        if (computerKeysEnabled && !hasKeyboardFocus (false))
            grabKeyboardFocus();
    }

private:
    NovaSynthProcessor& processor;

    // ---- Clavier d'ordinateur (style Bitwig) ----
    bool computerKeysEnabled { false };
    int  computerKeyOctave   { 4 };      // octave courante (défaut C4)
    std::set<int> heldComputerKeys;      // keyCodes actifs → évite répétition

    juce::TextButton keysBtn;            // toggle ON/OFF — affiche l'octave courante

    // Retourne le numéro de note MIDI ou -1 si la touche n'est pas mappée
    int computerKeyToNote (int keyCode) const;
    void sendComputerKeyNote (int keyCode, bool noteOn);

    // FX tab state
    bool             showingFX  { false };
    juce::TextButton fxTabBtn;

    // Oscillateurs (3 panneaux côte à côte)
    OscillatorPanel osc1Panel, osc2Panel, osc3Panel;

    // Filter
    FilterCurveDisplay filterCurve;
    LabelledCombo filterType;
    LabelledKnob  filterCutoff, filterRes, filterEnvAmt;
    LabelledKnob  filterDrive, filterFat, filterMix, filterPan;
    juce::TextButton filterEnableBtn;
    juce::TextButton filterRouteOsc1Btn, filterRouteOsc2Btn, filterRouteOsc3Btn;

    // Sub + Mono
    LabelledKnob  subLevel;
    StepDisplay   subOct;
    LabelledCombo subWave;
    LabelledKnob  glideKnob;
    juce::TextButton monoBtn;
    juce::TextButton subEnableBtn;

    // Noise oscillator
    LabelledKnob  noiseLevel, noisePan;
    LabelledCombo noiseTypeCombo;
    juce::TextButton noiseEnableBtn;

    // FX Panel (dedicated EffectsPanel shown when showingFX == true)
    std::unique_ptr<EffectsPanel> fxPanel;

    EnvLfoPanel  envLfoPanel;
    LabelledKnob masterGain;

    // Preset bar
    juce::ComboBox   presetCombo;
    juce::TextButton presetSaveBtn, presetPrevBtn, presetNextBtn;

    void refreshPresetCombo();
    void setSynthVisible (bool visible);

    void drawSection (juce::Graphics& g, juce::Rectangle<int> b,
                      const juce::String& title, juce::Colour colour);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaSynthEditor)
};
