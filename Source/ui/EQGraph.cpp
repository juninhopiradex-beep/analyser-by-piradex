#include "EQGraph.h"
#include "Names.h"

using namespace eqm;

static juce::String fmtHz (double f)
{
    if (f >= 1000.0) return juce::String (f / 1000.0, f >= 10000.0 ? 1 : 2).replace (".", piradex::i18n::isPortuguese() ? "," : ".") + " kHz";
    return juce::String (f, f < 100.0 ? 1 : 0).replace (".", piradex::i18n::isPortuguese() ? "," : ".") + " Hz";
}
static juce::String fmtDb (double d) { return (d > 0.05 ? "+" : "") + juce::String (d, 1).replace (".", piradex::i18n::isPortuguese() ? "," : ".") + " dB"; }
static juce::String noteName (double f)
{
    static const char* pt[] = { "Dó", "Dó#", "Ré", "Ré#", "Mi", "Fá", "Fá#", "Sol", "Sol#", "Lá", "Lá#", "Si" };
    static const char* en[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    const auto* n = piradex::i18n::isPortuguese() ? pt : en;
    const double midi = 69.0 + 12.0 * std::log2 (f / 440.0);
    const int m = (int) std::round (midi);
    const int cents = (int) std::round ((midi - m) * 100.0);
    return juce::String::fromUTF8 (n[(m % 12 + 12) % 12]) + juce::String (m / 12 - 1)
         + (cents == 0 ? "" : (cents > 0 ? " +" : " ") + juce::String (cents) + "c");
}

EQGraph::EQGraph (EQMotionProcessor& p) : proc (p), st (p.state)
{
    setWantsKeyboardFocus (true);
    lang.bind ([this]
    {
        setTitle (tr ("Gráfico do equalizador", "Equalizer graph"));
        setDescription (tr ("Duplo clique para adicionar uma banda; arraste os nós para mudar frequência e ganho.",
                            "Double-click to add a band; drag the nodes to change frequency and gain."));
    });
    lang.onChange = [this] { repaint(); };
    dbRangeVal = (float) (double) proc.uiState.getProperty ("dbRange", 12.0);
    viewLo = proc.uiState.getProperty ("viewLo", 10.0);
    viewHi = proc.uiState.getProperty ("viewHi", 30000.0);
    halo.fill (0.0f);
    startTimerHz (30);
}

EQGraph::~EQGraph() { stopTimer(); }

// ----------------------------------------------------------------------------- coordenadas
juce::Rectangle<float> EQGraph::plotArea() const { return getLocalBounds().toFloat().withTrimmedTop (26).withTrimmedBottom (20).withTrimmedRight (30); }
float EQGraph::xForFreq (double f) const
{
    const auto r = plotArea();
    return r.getX() + (float) (std::log (f / viewLo) / std::log (viewHi / viewLo)) * r.getWidth();
}
double EQGraph::freqForX (float x) const
{
    const auto r = plotArea();
    return viewLo * std::pow (viewHi / viewLo, (double) ((x - r.getX()) / r.getWidth()));
}
float EQGraph::yForDb (double db) const
{
    const auto r = plotArea();
    const float half = r.getHeight() * 0.5f - 14.0f;
    return r.getCentreY() - (float) (db / dbRangeVal) * half;
}
double EQGraph::dbForY (float y) const
{
    const auto r = plotArea();
    const float half = r.getHeight() * 0.5f - 14.0f;
    return (r.getCentreY() - y) / half * dbRangeVal;
}
float EQGraph::yForSpec (double db) const
{
    const auto r = plotArea();
    const double top = 0.0, bottom = -96.0;
    return r.getY() + (float) ((top - db) / (top - bottom)) * r.getHeight();
}

void EQGraph::setDbRange (float r) { dbRangeVal = r; proc.uiState.setProperty ("dbRange", r, nullptr); lastHash = 0; repaint(); }
void EQGraph::resetZoom() { viewLo = 10.0; viewHi = 30000.0; proc.uiState.setProperty ("viewLo", viewLo, nullptr); proc.uiState.setProperty ("viewHi", viewHi, nullptr); rebuildFreqs(); }

void EQGraph::resized() { rebuildFreqs(); }

void EQGraph::rebuildFreqs()
{
    const int n = juce::jlimit (128, 900, (int) plotArea().getWidth() / 2);
    freqs = curve::logFreqs (n, viewLo, viewHi);
    lastHash = 0;
    rebuildCurves (true);
    repaint();
}

// ----------------------------------------------------------------------------- dados
juce::RangedAudioParameter* EQGraph::bp (int b, ids::B which) const { return proc.apvts.getParameter (ids::band (b, which)); }
void EQGraph::setBp (int b, ids::B which, float v)
{
    auto* p = bp (b, which);
    p->setValueNotifyingHost (p->convertTo0to1 (v));
}

void EQGraph::timerCallback()
{
    if (proc.copySpectrum (spec)) repaint();
    rebuildCurves (false);
    // halos dinâmicos (suavizados na UI)
    bool moving = false;
    for (int b = 0; b < kMaxBands; ++b)
    {
        const float target = bands[(size_t) b].isProcessing() && bands[(size_t) b].isDynamic() ? (float) std::abs (proc.bandDynamicDb (b)) : 0.0f;
        const float prev = halo[(size_t) b];
        halo[(size_t) b] = prev + (target - prev) * 0.35f;
        moving |= std::abs (halo[(size_t) b] - prev) > 0.01f;
    }
    if (moving || smartOpen) repaint();
}

void EQGraph::rebuildCurves (bool force)
{
    bands = proc.currentBands();
    const double fs = proc.currentSampleRate();
    const auto mode = (PhaseMode) (int) proc.apvts.getRawParameterValue (ids::phase)->load();
    const double scale = proc.apvts.getRawParameterValue (ids::gainScale)->load() * 0.01;

    juce::int64 h = (juce::int64) mode * 31 + (juce::int64) (fs) + (juce::int64) (scale * 1000) + (juce::int64) freqs.size() * 7
                  + (juce::int64) (viewLo * 13) + (juce::int64) (viewHi);
    for (int b = 0; b < kMaxBands; ++b)
    {
        const auto& p = bands[(size_t) b];
        live[(size_t) b] = p.isProcessing() && p.isDynamic() ? std::round (proc.bandDynamicDb (b) * 20.0) / 20.0 : 0.0;
        h = h * 1000003 + (juce::int64) p.active + 2 * p.enabled + 4 * (int) p.type + 64 * (int) p.slope + 1024 * (int) p.placement
          + (juce::int64) (p.freq * 100) * 7 + (juce::int64) (p.gainDb * 1000) * 11 + (juce::int64) (p.q * 10000) * 13
          + (juce::int64) (p.isDynamic() ? (p.rangeDb * 100) : 0) * 17 + (juce::int64) (live[(size_t) b] * 100) * 19;
    }
    const auto& sug = proc.smart.suggestion;
    h = h * 31 + (juce::int64) sug.bands.size();
    for (auto& s : sug.bands) h = h * 131 + (juce::int64) (s.freq * 10) + (juce::int64) (s.gainDb * 100) + (juce::int64) (s.rangeDb * 100);
    if (! force && h == lastHash) return;
    lastHash = h;

    std::array<double, kMaxBands> gains {};
    hasLR = hasMS = false;
    for (int b = 0; b < kMaxBands; ++b)
    {
        const auto& p = bands[(size_t) b];
        if (! p.active) { bandStatic[(size_t) b].clear(); bandLive[(size_t) b].clear(); bandRange[(size_t) b].clear(); continue; }
        const double g0 = effectiveGain (p, scale, 0.0);
        curve::bandResponse (p, g0, mode, fs, freqs, bandStatic[(size_t) b]);
        if (p.isDynamic())
        {
            curve::bandResponse (p, effectiveGain (p, scale, live[(size_t) b]), mode, fs, freqs, bandLive[(size_t) b]);
            curve::bandResponse (p, effectiveGain (p, scale, p.rangeDb), mode, fs, freqs, bandRange[(size_t) b]);
        }
        else { bandLive[(size_t) b].clear(); bandRange[(size_t) b].clear(); }
        gains[(size_t) b] = p.isDynamic() ? effectiveGain (p, scale, live[(size_t) b]) : g0;
        if (p.isProcessing())
        {
            hasLR |= p.placement == Placement::Left || p.placement == Placement::Right;
            hasMS |= p.placement == Placement::Mid || p.placement == Placement::Side;
        }
    }
    auto sumDomain = [&] (std::vector<float>& out, int dom)   // 0 estéreo, 1 L, 2 R, 3 M, 4 S
    {
        out.assign (freqs.size(), 0.0f);
        for (int b = 0; b < kMaxBands; ++b)
        {
            const auto& p = bands[(size_t) b];
            if (! p.isProcessing()) continue;
            const int pl = (int) p.placement;
            if (! (pl == 0 || pl == dom)) continue;
            const auto& v = p.isDynamic() ? bandLive[(size_t) b] : bandStatic[(size_t) b];
            for (size_t i = 0; i < out.size() && i < v.size(); ++i) out[i] += v[i];
        }
    };
    sumDomain (totalStereo, 0);
    if (hasLR) { sumDomain (totalL, 1); sumDomain (totalR, 2); }
    if (hasMS) { sumDomain (totalM, 3); sumDomain (totalS, 4); }

    previewTotal.clear();
    if (! sug.bands.empty())
    {
        previewTotal = totalStereo;
        for (auto s : sug.bands)
        {
            const double g = s.dynamic ? s.rangeDb : s.gainDb;
            curve::bandResponse (s, effectiveGain (s, 1.0, s.dynamic ? s.rangeDb : 0.0) + (s.dynamic ? 0.0 : 0.0), mode, fs, freqs, tmp);
            juce::ignoreUnused (g);
            for (size_t i = 0; i < previewTotal.size(); ++i) previewTotal[i] += tmp[i];
        }
    }
    repaint();
}

// ----------------------------------------------------------------------------- desenho
juce::Path EQGraph::curvePath (const std::vector<float>& db, juce::Rectangle<float> r) const
{
    juce::Path p;
    if (db.empty()) return p;
    const float yTop = r.getY() - 40, yBot = r.getBottom() + 40;
    for (size_t i = 0; i < db.size() && i < freqs.size(); ++i)
    {
        const float x = xForFreq (freqs[i]), y = juce::jlimit (yTop, yBot, yForDb (db[i]));
        if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
    }
    return p;
}

juce::Path EQGraph::specPath (const std::vector<float>& db, const std::vector<double>& f, juce::Rectangle<float> r, bool closed) const
{
    juce::Path p;
    if (db.empty() || f.size() != db.size()) return p;
    bool started = false;
    float lastX = 0;
    for (size_t i = 0; i < db.size(); ++i)
    {
        if (f[i] < viewLo * 0.98 || f[i] > viewHi * 1.02) continue;
        const float x = xForFreq (f[i]), y = juce::jlimit (r.getY(), r.getBottom(), yForSpec (db[i]));
        if (! started) { if (closed) p.startNewSubPath (x, r.getBottom()), p.lineTo (x, y); else p.startNewSubPath (x, y); started = true; }
        else p.lineTo (x, y);
        lastX = x;
    }
    if (closed && started) { p.lineTo (lastX, r.getBottom()); p.closeSubPath(); }
    return p;
}

void EQGraph::paint (juce::Graphics& g)
{
    const auto r = plotArea();
    g.setGradientFill (juce::ColourGradient (pal::bg1.brighter (0.02f), 0, r.getY(), pal::bg0, 0, r.getBottom(), false));
    g.fillRect (getLocalBounds());
    drawGrid (g, r);
    {
        juce::Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (r.toNearestInt());
        drawSpectrum (g, r);
        drawInstances (g, r);
        drawSmart (g, r);
        drawBands (g, r);
    }
    drawNodes (g);
    drawReadouts (g, r);
}

void EQGraph::drawGrid (juce::Graphics& g, juce::Rectangle<float> r)
{
    static const double minor[] = { 20, 30, 40, 50, 60, 70, 80, 90, 200, 300, 400, 500, 600, 700, 800, 900, 2000, 3000, 4000, 5000, 6000, 7000, 8000, 9000, 20000, 30000 };
    static const double major[] = { 10, 100, 1000, 10000 };
    for (double f : minor) if (f > viewLo && f < viewHi) { g.setColour (pal::grid.withAlpha (0.55f)); g.drawVerticalLine ((int) xForFreq (f), r.getY(), r.getBottom()); }
    for (double f : major) if (f > viewLo && f < viewHi) { g.setColour (pal::grid.interpolatedWith (pal::textFaint, 0.18f)); g.drawVerticalLine ((int) xForFreq (f), r.getY(), r.getBottom()); }

    g.setFont (fonts::mono (10.5f));
    static const double labels[] = { 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000 };
    for (double f : labels)
        if (f > viewLo && f < viewHi)
        {
            g.setColour (pal::textFaint);
            const auto txt = f >= 1000 ? juce::String ((int) (f / 1000)) + "k" : juce::String ((int) f);
            g.drawText (txt, juce::Rectangle<float> (xForFreq (f) - 20, r.getBottom() + 3, 40, 14), juce::Justification::centred);
        }

    // nomes das notas quando o zoom é grande (menos de ~4 oitavas visíveis)
    const double octs = std::log2 (viewHi / viewLo);
    if (octs < 4.2)
    {
        static const char* nn[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        g.setFont (fonts::mono (9.5f));
        const int m0 = (int) std::ceil (69 + 12 * std::log2 (viewLo / 440.0)), m1 = (int) std::floor (69 + 12 * std::log2 (viewHi / 440.0));
        for (int m = m0; m <= m1; ++m)
        {
            const bool natural = std::string ("CDEFGAB").find (nn[m % 12][0]) != std::string::npos && nn[m % 12][1] == 0;
            if (octs > 2.2 && (m % 12) != 0) continue;   // só os Dó
            if (octs > 1.2 && ! natural) continue;
            const double f = 440.0 * std::pow (2.0, (m - 69) / 12.0);
            const float x = xForFreq (f);
            g.setColour (pal::grid.withAlpha (0.8f));
            g.drawVerticalLine ((int) x, r.getY(), r.getY() + 10);
            g.setColour ((m % 12) == 0 ? pal::textDim : pal::textFaint);
            g.drawText (juce::String (nn[m % 12]) + juce::String (m / 12 - 1), juce::Rectangle<float> (x - 16, r.getY() + 10, 32, 12), juce::Justification::centred);
        }
    }

    const float step = dbRangeVal <= 3 ? 1.0f : dbRangeVal <= 6 ? 2.0f : dbRangeVal <= 12 ? 3.0f : dbRangeVal <= 24 ? 6.0f : 10.0f;
    for (float d = -dbRangeVal; d <= dbRangeVal + 0.01f; d += step)
    {
        const float y = yForDb (d);
        g.setColour (std::abs (d) < 0.01f ? pal::grid.interpolatedWith (pal::textFaint, 0.45f) : pal::grid.withAlpha (0.7f));
        g.drawHorizontalLine ((int) y, r.getX(), r.getRight());
        g.setColour (std::abs (d) < 0.01f ? pal::textDim : pal::textFaint);
        g.drawText ((d > 0 ? "+" : "") + juce::String ((int) d), juce::Rectangle<float> (r.getRight() + 2, y - 7, 28, 14), juce::Justification::centredLeft);
    }
}

void EQGraph::drawSpectrum (juce::Graphics& g, juce::Rectangle<float> r)
{
    if (spec.freqs.empty()) return;
    const auto& az = proc.analyzer;
    if (az.showPost.load() && ! spec.post.empty())
    {
        auto fill = specPath (spec.post, spec.freqs, r, true);
        g.setGradientFill (juce::ColourGradient (pal::postFill.brighter (0.25f).withAlpha (0.85f), 0, r.getY() + r.getHeight() * 0.3f,
                                                 pal::postFill.withAlpha (0.25f), 0, r.getBottom(), false));
        g.fillPath (fill);
        g.setColour (pal::pre.brighter (0.4f).withAlpha (0.55f));
        g.strokePath (specPath (spec.post, spec.freqs, r, false), juce::PathStrokeType (1.0f));
        if (! spec.peak.empty())
        {
            g.setColour (pal::textFaint.withAlpha (0.45f));
            g.strokePath (specPath (spec.peak, spec.freqs, r, false), juce::PathStrokeType (0.8f));
        }
        if (spec.split != 0 && ! spec.postA.empty() && ! spec.postB.empty())
        {
            g.setColour ((spec.split == 2 ? pal::mid : pal::curve).withAlpha (0.55f));
            g.strokePath (specPath (spec.postA, spec.freqs, r, false), juce::PathStrokeType (1.0f));
            g.setColour ((spec.split == 2 ? pal::side : pal::sc).withAlpha (0.55f));
            g.strokePath (specPath (spec.postB, spec.freqs, r, false), juce::PathStrokeType (1.0f));
        }
    }
    if (az.showPre.load() && ! spec.pre.empty())
    {
        g.setColour (pal::pre.withAlpha (0.7f));
        g.strokePath (specPath (spec.pre, spec.freqs, r, false), juce::PathStrokeType (1.0f));
    }
    if (az.showSc.load() && spec.hasSc && ! spec.sc.empty())
    {
        g.setColour (pal::sc.withAlpha (0.75f));
        g.strokePath (specPath (spec.sc, spec.freqs, r, false), juce::PathStrokeType (1.2f));
    }
}

void EQGraph::drawInstances (juce::Graphics& g, juce::Rectangle<float> r)
{
    const auto visible = juce::StringArray::fromTokens (proc.uiState.getProperty ("instancesShown").toString(), ",", "");
    if (visible.isEmpty()) return;
    static const auto commonF = curve::logFreqs (480, 10.0, 30000.0);
    int k = 0;
    for (auto& inst : InstanceRegistry::get().all())
    {
        if (inst == proc.shared || ! visible.contains (juce::String (inst->id))) continue;
        std::vector<float> s, c; juce::Colour col; juce::String name;
        {
            const juce::SpinLock::ScopedLockType lk (inst->lock);
            s = inst->spectrumDb; c = inst->curveDb; col = inst->colour; name = inst->name;
        }
        if (col.isTransparent() || col == juce::Colour()) col = pal::band (k + 5);
        col = col.withSaturation (juce::jmax (0.45f, col.getSaturation())).withBrightness (juce::jmax (0.75f, col.getBrightness()));
        if (! s.empty())
        {
            g.setColour (col.withAlpha (0.55f));
            g.strokePath (specPath (s, commonF, r, false), juce::PathStrokeType (1.0f));
        }
        if (! c.empty())
        {
            juce::Path p; bool first = true;
            for (size_t i = 0; i < c.size(); ++i)
            {
                if (commonF[i] < viewLo || commonF[i] > viewHi) continue;
                const float x = xForFreq (commonF[i]), y = yForDb (c[i]);
                if (first) { p.startNewSubPath (x, y); first = false; } else p.lineTo (x, y);
            }
            juce::Path dashed;
            const float dash[] = { 5.0f, 4.0f };
            juce::PathStrokeType (1.4f).createDashedStroke (dashed, p, dash, 2);
            g.setColour (col.withAlpha (0.9f));
            g.fillPath (dashed);
            g.setFont (fonts::ui (11.0f, 1));
            g.drawText (name, juce::Rectangle<float> (r.getRight() - 160, r.getY() + 6 + 16 * k, 150, 14), juce::Justification::centredRight);
        }
        ++k;
    }
}

void EQGraph::drawSmart (juce::Graphics& g, juce::Rectangle<float> r)
{
    if (smartOpen)
    {
        // zona fora do intervalo escurecida + pegas
        const float x0 = xForFreq (proc.smart.fromHz), x1 = xForFreq (proc.smart.toHz);
        g.setColour (pal::bg0.withAlpha (0.45f));
        g.fillRect (juce::Rectangle<float> (r.getX(), r.getY(), juce::jmax (0.0f, x0 - r.getX()), r.getHeight()));
        g.fillRect (juce::Rectangle<float> (x1, r.getY(), juce::jmax (0.0f, r.getRight() - x1), r.getHeight()));
        for (float x : { x0, x1 })
        {
            g.setColour (pal::accent.withAlpha (0.7f));
            g.drawVerticalLine ((int) x, r.getY(), r.getBottom());
            g.fillRoundedRectangle (x - 4, r.getY() + 6, 8, 22, 3);
        }
    }
    const auto& sug = proc.smart.suggestion;
    if (sug.bands.empty()) return;
    // zonas problemáticas
    for (auto& s : sug.bands)
    {
        const double bw = s.type == FilterType::Bell ? std::max (0.08, 2.0 * std::asinh (1.0 / (2.0 * s.q)) / std::log (2.0)) : 0.6;
        const float xa = xForFreq (s.freq * std::pow (2.0, -bw * 0.5)), xb = xForFreq (s.freq * std::pow (2.0, bw * 0.5));
        g.setGradientFill (juce::ColourGradient (pal::accent2.withAlpha (0.0f), 0, r.getY(), pal::accent2.withAlpha (0.13f), 0, r.getBottom(), false));
        g.fillRect (juce::Rectangle<float> (xa, r.getY(), juce::jmax (2.0f, xb - xa), r.getHeight()));
    }
    if (! previewTotal.empty())
    {
        auto p = curvePath (previewTotal, r);
        juce::Path dashed;
        const float dash[] = { 6.0f, 4.0f };
        juce::PathStrokeType (1.8f).createDashedStroke (dashed, p, dash, 2);
        g.setColour (pal::accent);
        g.fillPath (dashed);
    }
}

void EQGraph::drawBands (juce::Graphics& g, juce::Rectangle<float> r)
{
    const float y0 = yForDb (0.0);
    // preenchimento de cada banda
    for (int b = 0; b < kMaxBands; ++b)
    {
        const auto& p = bands[(size_t) b];
        if (! p.active || bandStatic[(size_t) b].empty()) continue;
        const bool sel = selected.contains (b);
        const auto col = pal::band (b);
        const auto& v = p.isDynamic() ? bandLive[(size_t) b] : bandStatic[(size_t) b];
        auto path = curvePath (v, r);
        juce::Path fill (path);
        fill.lineTo (xForFreq (freqs.back()), y0);
        fill.lineTo (xForFreq (freqs.front()), y0);
        fill.closeSubPath();
        g.setColour (col.withAlpha (! p.enabled ? 0.03f : sel ? 0.20f : 0.08f));
        g.fillPath (fill);
        if (sel || hover == b)
        {
            g.setColour (col.withAlpha (p.enabled ? 0.85f : 0.35f));
            g.strokePath (path, juce::PathStrokeType (1.2f));
        }
        // área dinâmica: entre a curva estática e a curva com o range inteiro
        if (p.isDynamic() && ! bandRange[(size_t) b].empty())
        {
            juce::Path area;
            const auto& a = bandStatic[(size_t) b];
            const auto& c = bandRange[(size_t) b];
            for (size_t i = 0; i < a.size(); ++i) { const float x = xForFreq (freqs[i]); if (i == 0) area.startNewSubPath (x, yForDb (a[i])); else area.lineTo (x, yForDb (a[i])); }
            for (size_t i = c.size(); i-- > 0;) area.lineTo (xForFreq (freqs[i]), yForDb (c[i]));
            area.closeSubPath();
            g.setColour (col.withAlpha (sel ? 0.16f : 0.08f));
            g.fillPath (area);
            juce::Path rangeLine = curvePath (c, r), dashed;
            const float dash[] = { 3.0f, 3.0f };
            juce::PathStrokeType (1.0f).createDashedStroke (dashed, rangeLine, dash, 2);
            g.setColour (col.withAlpha (0.55f));
            g.fillPath (dashed);
        }
    }
    // curvas totais
    auto stroke = [&] (const std::vector<float>& v, juce::Colour c, float w, const juce::String& tag)
    {
        auto p = curvePath (v, r);
        g.setColour (c.withAlpha (0.25f));
        g.strokePath (p, juce::PathStrokeType (w + 3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (c);
        g.strokePath (p, juce::PathStrokeType (w, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        if (tag.isNotEmpty() && ! v.empty())
        {
            g.setFont (fonts::ui (11.0f, 2));
            g.drawText (tag, juce::Rectangle<float> (r.getRight() - 22, yForDb (v.back()) - 16, 18, 14), juce::Justification::centred);
        }
    };
    const bool byp = proc.apvts.getRawParameterValue (ids::bypass)->load() > 0.5f;
    const float alpha = byp ? 0.35f : 1.0f;
    if (hasMS) { stroke (totalS, pal::side.withMultipliedAlpha (alpha), 1.8f, "S"); stroke (totalM, pal::mid.withMultipliedAlpha (alpha), 1.8f, "M"); }
    if (hasLR) { stroke (totalR, pal::curve.withAlpha (0.6f * alpha), 1.6f, "R"); stroke (totalL, pal::curve.withMultipliedAlpha (alpha), 1.6f, "L"); }
    if (! hasMS && ! hasLR) stroke (totalStereo, pal::curve.withMultipliedAlpha (alpha), 2.0f, {});
}

juce::Point<float> EQGraph::nodePos (int b) const
{
    const auto& p = bands[(size_t) b];
    const double scale = proc.apvts.getRawParameterValue (ids::gainScale)->load() * 0.01;
    const double g = typeHasGain (p.type) ? p.gainDb * scale : 0.0;
    return { xForFreq (clampv (p.freq, viewLo, viewHi)), yForDb (clampv (g, (double) -dbRangeVal, (double) dbRangeVal)) };
}

int EQGraph::nodeAt (juce::Point<float> pt) const
{
    int best = -1; float bd = 13.0f;
    for (int b = 0; b < kMaxBands; ++b)
    {
        if (! bands[(size_t) b].active) continue;
        const float d = nodePos (b).getDistanceFrom (pt);
        if (d < bd) { bd = d; best = b; }
    }
    return best;
}

void EQGraph::drawNodes (juce::Graphics& g)
{
    const int listen = proc.listenBandIndex();
    const bool dragging = drag == Drag::Node;
    for (int pass = 0; pass < 2; ++pass)   // selecionados por cima
        for (int b = 0; b < kMaxBands; ++b)
        {
            const auto& p = bands[(size_t) b];
            if (! p.active) continue;
            const bool sel = selected.contains (b);
            if ((pass == 1) != sel) continue;
            const auto c = nodePos (b);
            const auto col = pal::band (b);
            const float rad = sel ? 10.5f : 9.0f;

            if (p.isDynamic() && p.enabled)
            {
                const float hz = halo[(size_t) b];
                // posição viva do ganho (o que a dinâmica está a fazer agora)
                const float yLive = yForDb (clampv ((double) p.gainDb + proc.bandDynamicDb (b), (double) -dbRangeVal, (double) dbRangeVal));
                if (std::abs (yLive - c.y) > 2.0f)
                {
                    g.setColour (col.withAlpha (0.6f));
                    g.drawLine (c.x, c.y, c.x, yLive, 1.5f);
                    g.fillEllipse (c.x - 3.0f, yLive - 3.0f, 6.0f, 6.0f);
                }
                const float hr = rad + 3.0f + hz * 1.6f;
                g.setColour (col.withAlpha (juce::jlimit (0.12f, 0.45f, 0.12f + hz * 0.05f)));
                g.drawEllipse (c.x - hr, c.y - hr, hr * 2, hr * 2, 2.0f);
                g.setColour (col.withAlpha (0.08f + juce::jmin (0.2f, hz * 0.03f)));
                g.fillEllipse (c.x - hr, c.y - hr, hr * 2, hr * 2);
            }
            if (b == listen)
            {
                g.setColour (pal::accent2.withAlpha (0.5f));
                g.drawEllipse (c.x - rad - 5, c.y - rad - 5, (rad + 5) * 2, (rad + 5) * 2, 1.5f);
            }
            if (hover == b || (sel && dragging))
            {
                g.setColour (col.withAlpha (0.18f));
                g.fillEllipse (c.x - rad - 6, c.y - rad - 6, (rad + 6) * 2, (rad + 6) * 2);
            }
            const auto fillC = ! p.enabled ? pal::bg2 : sel ? col : pal::bg0.withAlpha (0.85f);
            g.setColour (fillC);
            g.fillEllipse (c.x - rad, c.y - rad, rad * 2, rad * 2);
            g.setColour (p.enabled ? col : pal::textFaint);
            g.drawEllipse (c.x - rad, c.y - rad, rad * 2, rad * 2, sel ? 2.0f : 1.6f);
            g.setColour (sel && p.enabled ? pal::bg0 : p.enabled ? col.brighter (0.3f) : pal::textFaint);
            g.setFont (fonts::ui (10.5f, 2));
            g.drawText (juce::String (b + 1), juce::Rectangle<float> (c.x - rad, c.y - rad, rad * 2, rad * 2), juce::Justification::centred);
            if (p.placement != Placement::Stereo)
            {
                g.setFont (fonts::ui (9.0f, 2));
                g.setColour (p.placement == Placement::Mid ? pal::mid : p.placement == Placement::Side ? pal::side : pal::curve);
                g.drawText (uinames::placementShort()[(int) p.placement], juce::Rectangle<float> (c.x + rad - 2, c.y - rad - 10, 16, 10), juce::Justification::centredLeft);
            }
        }

    // nós sugeridos pelas Smart Ops (tracejado)
    for (auto& s : proc.smart.suggestion.bands)
    {
        const double gdb = s.dynamic ? s.rangeDb : (typeHasGain (s.type) ? s.gainDb : 0.0);
        const float x = xForFreq (s.freq), y = yForDb (clampv (gdb, (double) -dbRangeVal, (double) dbRangeVal));
        juce::Path ring; ring.addEllipse (x - 7, y - 7, 14, 14);
        juce::Path dashed; const float dash[] = { 3.0f, 3.0f };
        juce::PathStrokeType (1.4f).createDashedStroke (dashed, ring, dash, 2);
        g.setColour (pal::accent);
        g.fillPath (dashed);
        g.fillEllipse (x - 2, y - 2, 4, 4);
    }
}

void EQGraph::drawReadouts (juce::Graphics& g, juce::Rectangle<float> r)
{
    // cabeçalho: legenda do analisador
    g.setFont (fonts::ui (11.0f, 1));
    float lx = r.getX() + 8;
    auto legend = [&] (juce::Colour c, const juce::String& t)
    {
        g.setColour (c); g.fillRoundedRectangle (lx, 9, 10, 3, 1.5f);
        g.setColour (pal::textDim); g.drawText (t, juce::Rectangle<float> (lx + 14, 3, 80, 16), juce::Justification::centredLeft);
        lx += 18 + juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), t) + 12;
    };
    if (proc.analyzer.showPre.load()) legend (pal::pre, tr ("Entrada", "Input"));
    if (proc.analyzer.showPost.load()) legend (pal::postFill.brighter (0.8f), tr ("Saída", "Output"));
    if (spec.hasSc && proc.analyzer.showSc.load()) legend (pal::sc, "Sidechain");
    if (spec.split == 2) { legend (pal::mid, "Mid"); legend (pal::side, "Side"); }
    if (proc.analyzer.freeze.load()) { g.setColour (pal::accent); g.drawText (tr ("CONGELADO", "FROZEN"), juce::Rectangle<float> (lx, 3, 90, 16), juce::Justification::centredLeft); }
    if (! proc.smart.suggestion.bands.empty())
    {
        g.setColour (pal::accent);
        g.drawText (tr ("- - pré-visualização, ainda não aplicada", "- - preview, not applied yet"), juce::Rectangle<float> (r.getRight() - 300, 3, 296, 16), juce::Justification::centredRight);
    }

    // dica
    g.setColour (pal::textFaint);
    g.setFont (fonts::ui (10.5f));
    if (! mouseInside || hover < 0)
        g.drawText (tr ("Duplo clique: adicionar  ·  arrastar: mover  ·  roda/Alt+arrastar: Q  ·  Shift: fino  ·  duplo clique no nó: remover", "Double-click: add  ·  drag: move  ·  wheel/Alt+drag: Q  ·  Shift: fine  ·  double-click a node: remove"),
                    juce::Rectangle<float> (r.getX() + 8, r.getBottom() - 18, r.getWidth() - 16, 14), juce::Justification::centredLeft);

    // cursor
    if (mouseInside && drag == Drag::None && hover < 0 && r.contains (mouse))
    {
        const double f = freqForX (mouse.x);
        const auto txt = fmtHz (f) + juce::String::fromUTF8 ("  ·  ") + noteName (f) + juce::String::fromUTF8 ("  ·  ") + fmtDb (dbForY (mouse.y));
        g.setFont (fonts::mono (11.0f));
        g.setColour (pal::textDim);
        g.drawText (txt, juce::Rectangle<float> (r.getRight() - 300, r.getBottom() - 18, 292, 14), juce::Justification::centredRight);
        g.setColour (pal::textFaint.withAlpha (0.4f));
        g.drawVerticalLine ((int) mouse.x, r.getY(), r.getBottom());
    }

    // caixa de informação da banda sob o rato / em arrasto
    const int b = drag == Drag::Node ? primary : hover;
    if (b >= 0 && bands[(size_t) b].active)
    {
        const auto& p = bands[(size_t) b];
        juce::String l1 = juce::String (b + 1) + "  " + uinames::types()[(int) p.type];
        if (typeIsCut (p.type)) l1 << "  " << names::slopes()[(int) p.slope];
        if (p.placement != Placement::Stereo) l1 << juce::String::fromUTF8 ("  ·  ") << uinames::placements()[(int) p.placement];
        juce::String l2 = fmtHz (p.freq);
        if (typeHasGain (p.type)) l2 << "   " << fmtDb (p.gainDb);
        l2 << "   Q " << juce::String (p.q, 2).replace (".", piradex::i18n::isPortuguese() ? "," : ".");
        juce::String l3;
        if (p.isDynamic())
            l3 = tr ("Dinâmica ", "Dynamic ") + fmtDb (proc.bandDynamicDb (b)) + "  (range " + fmtDb (p.rangeDb) + ")";
        const auto c = nodePos (b);
        const float w = 200, h = l3.isEmpty() ? 40.0f : 56.0f;
        auto box = juce::Rectangle<float> (c.x + 16, c.y - h - 10, w, h);
        if (box.getRight() > r.getRight()) box.setX (c.x - w - 16);
        if (box.getY() < r.getY()) box.setY (c.y + 16);
        g.setColour (pal::bg2.withAlpha (0.95f));
        g.fillRoundedRectangle (box, 6);
        g.setColour (pal::band (b).withAlpha (0.6f));
        g.drawRoundedRectangle (box, 6, 1);
        g.setColour (pal::text);
        g.setFont (fonts::ui (12.0f, 2));
        g.drawText (l1, box.reduced (10, 6).removeFromTop (14), juce::Justification::centredLeft);
        g.setFont (fonts::mono (11.5f));
        g.setColour (pal::textDim);
        g.drawText (l2, box.reduced (10, 6).withTrimmedTop (17).removeFromTop (14), juce::Justification::centredLeft);
        if (l3.isNotEmpty())
        {
            g.setColour (pal::band (b));
            g.drawText (l3, box.reduced (10, 6).withTrimmedTop (33).removeFromTop (14), juce::Justification::centredLeft);
        }
    }

    if (isZoomed())
    {
        g.setColour (pal::accent);
        g.setFont (fonts::ui (10.5f, 1));
        g.drawText ("ZOOM", juce::Rectangle<float> (r.getRight() - 60, r.getY() + 4, 54, 14), juce::Justification::centredRight);
    }
}

// ----------------------------------------------------------------------------- interação
void EQGraph::selectBand (int b, bool notify)
{
    selected.clearQuick();
    if (b >= 0) selected.add (b);
    primary = b;
    if (notify) notifySelection();
    repaint();
}

void EQGraph::notifySelection()
{
    proc.uiState.setProperty ("selected", primary, nullptr);
    if (onSelectionChanged) onSelectionChanged (primary);
}

void EQGraph::mouseMove (const juce::MouseEvent& e)
{
    mouse = e.position; mouseInside = true;
    const int h = nodeAt (e.position);
    if (h != hover) hover = h;
    const auto r = plotArea();
    bool overHandle = false;
    if (smartOpen)
        for (double f : { proc.smart.fromHz, proc.smart.toHz })
            overHandle |= std::abs (e.position.x - xForFreq (f)) < 6 && e.position.y < r.getY() + 34;
    setMouseCursor (hover >= 0 ? juce::MouseCursor::PointingHandCursor
                    : overHandle ? juce::MouseCursor::LeftRightResizeCursor : juce::MouseCursor::CrosshairCursor);
    repaint();
}

void EQGraph::mouseExit (const juce::MouseEvent&) { mouseInside = false; hover = -1; repaint(); }

void EQGraph::beginDrag (const juce::MouseEvent& e)
{
    dragStart = e.position;
    starts.clear();
    gestureParams.clear();
    for (int b : selected)
    {
        const auto& p = bands[(size_t) b];
        starts.push_back ({ b, p.freq, p.gainDb, p.q });
        for (auto w : { ids::B::Freq, ids::B::Gain, ids::B::Q })
        {
            auto* prm = bp (b, w);
            prm->beginChangeGesture();
            gestureParams.push_back (prm);
        }
    }
    st.markUiTouched();
}

void EQGraph::endGestures()
{
    for (auto* p : gestureParams) p->endChangeGesture();
    gestureParams.clear();
}

void EQGraph::mouseDown (const juce::MouseEvent& e)
{
    grabKeyboardFocus();
    const auto r = plotArea();
    drag = Drag::None;
    if (smartOpen && e.position.y < r.getY() + 34)
    {
        if (std::abs (e.position.x - xForFreq (proc.smart.fromHz)) < 7) { drag = Drag::RangeFrom; return; }
        if (std::abs (e.position.x - xForFreq (proc.smart.toHz)) < 7)   { drag = Drag::RangeTo; return; }
    }
    const int n = nodeAt (e.position);
    if (n >= 0)
    {
        if (e.mods.isShiftDown() && ! e.mods.isPopupMenu())
        {
            if (selected.contains (n)) selected.removeFirstMatchingValue (n); else selected.add (n);
            primary = selected.isEmpty() ? -1 : n;
            notifySelection();
        }
        else if (! selected.contains (n)) selectBand (n);
        else { primary = n; notifySelection(); }
        if (e.mods.isPopupMenu()) { showBandMenu (n); return; }
        drag = Drag::Node;
        beginDrag (e);
        repaint();
        return;
    }
    if (e.mods.isPopupMenu())
    {
        juce::PopupMenu m;
        const double f = freqForX (e.position.x), gdb = dbForY (e.position.y);
        m.addSectionHeader (fmtHz (f));
        m.addItem (tr ("Adicionar sino aqui", "Add bell here"), st.firstFreeBand() >= 0, false, [this, f, gdb] { addBandAt (f, gdb); });
        m.addItem (tr ("Colar bandas", "Paste bands"), StateManager::clipboardHasBands(), false, [this]
        {
            auto added = st.pasteBands();
            if (! added.isEmpty()) { selected = added; primary = added.getFirst(); notifySelection(); }
        });
        m.addSeparator();
        juce::PopupMenu range;
        for (float v : { 3.0f, 6.0f, 12.0f, 24.0f, 30.0f })
            range.addItem (juce::String::fromUTF8 ("±") + juce::String ((int) v) + " dB", true, std::abs (dbRangeVal - v) < 0.01f, [this, v] { setDbRange (v); });
        m.addSubMenu (tr ("Escala do gráfico", "Graph scale"), range);
        m.addItem (tr ("Repor zoom", "Reset zoom"), isZoomed(), false, [this] { resetZoom(); });
        m.addSeparator();
        m.addItem (tr ("Limpar todas as bandas", "Clear all bands"), st.activeBandCount() > 0, false, [this] { st.performAction ([this] { st.clearBands(); }); selectBand (-1); });
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMousePosition());
        return;
    }
    if (! e.mods.isShiftDown()) selectBand (-1);
    if (isZoomed() || e.mods.isAltDown()) { drag = Drag::Pan; panLo = viewLo; panHi = viewHi; dragStart = e.position; }
}

void EQGraph::mouseDrag (const juce::MouseEvent& e)
{
    mouse = e.position;
    const auto r = plotArea();
    if (drag == Drag::RangeFrom || drag == Drag::RangeTo)
    {
        const double f = clampv (freqForX (e.position.x), 20.0, 20000.0);
        if (drag == Drag::RangeFrom) proc.smart.fromHz = std::min (f, proc.smart.toHz / 1.2);
        else                         proc.smart.toHz = std::max (f, proc.smart.fromHz * 1.2);
        proc.smart.rangeTouched = true;
        proc.smartRecompute();
        repaint();
        return;
    }
    if (drag == Drag::Pan)
    {
        const double k = std::log (panHi / panLo) * (e.position.x - dragStart.x) / r.getWidth();
        double lo = panLo * std::exp (-k), hi = panHi * std::exp (-k);
        if (lo < 10.0) { hi *= 10.0 / lo; lo = 10.0; }
        if (hi > 30000.0) { lo *= 30000.0 / hi; hi = 30000.0; }
        viewLo = lo; viewHi = hi;
        rebuildFreqs();
        return;
    }
    if (drag != Drag::Node) return;
    const float fine = e.mods.isShiftDown() ? 0.15f : 1.0f;
    const float dx = (e.position.x - dragStart.x) * fine, dy = (e.position.y - dragStart.y) * fine;
    const double dLog = dx / r.getWidth() * std::log (viewHi / viewLo);
    const double dDb = -dy / (r.getHeight() * 0.5f - 14.0f) * dbRangeVal;
    for (auto& s : starts)
    {
        const auto& p = bands[(size_t) s.band];
        if (e.mods.isAltDown())
        {
            setBp (s.band, ids::B::Q, (float) clampv (s.q * std::exp (-dy * 0.02), kMinQ, kMaxQ));
            continue;
        }
        setBp (s.band, ids::B::Freq, (float) clampv (s.freq * std::exp (dLog), kMinFreq, kMaxFreq));
        if (typeHasGain (p.type)) setBp (s.band, ids::B::Gain, (float) clampv (s.gain + dDb, -30.0, 30.0));
        else if (typeIsCut (p.type) || p.type == FilterType::BandPass || p.type == FilterType::Notch)
            setBp (s.band, ids::B::Q, (float) clampv (s.q * std::exp (-dy * 0.012), 0.1, 30.0));
    }
    rebuildCurves (true);
}

void EQGraph::mouseUp (const juce::MouseEvent&)
{
    if (drag == Drag::Node) { endGestures(); st.markUiTouched(); }
    if (drag == Drag::Pan) { proc.uiState.setProperty ("viewLo", viewLo, nullptr); proc.uiState.setProperty ("viewHi", viewHi, nullptr); }
    drag = Drag::None;
    repaint();
}

void EQGraph::addBandAt (double f, double gdb)
{
    const auto r = plotArea();
    BandParams b;
    b.active = true; b.freq = clampv (f, 10.0, 30000.0); b.q = 1.0;
    const float x = xForFreq (f);
    if (x < r.getX() + r.getWidth() * 0.035f) { b.type = FilterType::LowCut; b.slope = Slope::dB24; b.q = 0.7071; }
    else if (x > r.getRight() - r.getWidth() * 0.035f) { b.type = FilterType::HighCut; b.slope = Slope::dB24; b.q = 0.7071; }
    else { b.type = FilterType::Bell; b.gainDb = std::round (clampv (gdb, -30.0, 30.0) * 10.0) / 10.0; }
    const int mode = (int) proc.apvts.getRawParameterValue (ids::procMode)->load();
    b.placement = mode == 2 ? Placement::Mid : mode == 1 ? Placement::Left : Placement::Stereo;
    int idx = -1;
    st.performAction ([&] { idx = st.addBand (b); });
    if (idx >= 0) selectBand (idx);
}

void EQGraph::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (! plotArea().contains (e.position)) return;
    const int n = nodeAt (e.position);
    if (n >= 0)
    {
        if (drag == Drag::Node) endGestures();
        drag = Drag::None;
        st.performAction ([this, n] { st.removeBand (n); });
        if (proc.listenBandIndex() == n) proc.setListenBand (-1);
        selectBand (-1);
        return;
    }
    addBandAt (freqForX (e.position.x), dbForY (e.position.y));
}

void EQGraph::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    const int n = hover >= 0 ? hover : (selected.size() == 1 ? -1 : -1);
    if (n >= 0 && ! e.mods.isCommandDown())
    {
        auto* p = bp (n, ids::B::Q);
        const float q = p->convertFrom0to1 (p->getValue());
        const float k = std::exp ((w.deltaY + w.deltaX) * (e.mods.isShiftDown() ? 0.4f : 1.6f));
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (juce::jlimit ((float) kMinQ, (float) kMaxQ, q * k)));
        p->endChangeGesture();
        st.markUiTouched();
        rebuildCurves (true);
        return;
    }
    if (e.mods.isCommandDown() || e.mods.isCtrlDown())
    {
        // zoom horizontal à volta do rato
        const double fc = freqForX (e.position.x);
        const double z = std::exp (-w.deltaY * 1.2);
        double lo = fc * std::pow (viewLo / fc, z), hi = fc * std::pow (viewHi / fc, z);
        if (std::log (hi / lo) < std::log (2.0)) return;   // máximo: uma oitava
        lo = std::max (10.0, lo); hi = std::min (30000.0, hi);
        viewLo = lo; viewHi = hi;
        proc.uiState.setProperty ("viewLo", viewLo, nullptr); proc.uiState.setProperty ("viewHi", viewHi, nullptr);
        rebuildFreqs();
    }
}

bool EQGraph::keyPressed (const juce::KeyPress& k)
{
    const bool cmd = k.getModifiers().isCommandDown();
    if (cmd && k.getKeyCode() == 'Z') { if (k.getModifiers().isShiftDown()) st.redo(); else st.undo(); return true; }
    if (cmd && k.getKeyCode() == 'Y') { st.redo(); return true; }
    if (cmd && k.getKeyCode() == 'C' && ! selected.isEmpty()) { st.copyBands (selected); return true; }
    if (cmd && k.getKeyCode() == 'V')
    {
        auto added = st.pasteBands();
        if (! added.isEmpty()) { selected = added; primary = added.getFirst(); notifySelection(); }
        return true;
    }
    if ((k == juce::KeyPress::deleteKey || k == juce::KeyPress::backspaceKey) && ! selected.isEmpty())
    {
        const auto sel = selected;
        st.performAction ([&] { for (int b : sel) st.removeBand (b); });
        selectBand (-1);
        return true;
    }
    if (k == juce::KeyPress::tabKey)
    {
        int start = primary;
        for (int i = 1; i <= kMaxBands; ++i)
        {
            const int b = ((start < 0 ? -1 : start) + (k.getModifiers().isShiftDown() ? -i : i) + kMaxBands * 2) % kMaxBands;
            if (bands[(size_t) b].active) { selectBand (b); return true; }
        }
        return true;
    }
    if (primary >= 0 && (k.isKeyCode (juce::KeyPress::leftKey) || k.isKeyCode (juce::KeyPress::rightKey)
                         || k.isKeyCode (juce::KeyPress::upKey) || k.isKeyCode (juce::KeyPress::downKey)))
    {
        const bool fine = k.getModifiers().isShiftDown();
        st.markUiTouched();
        if (k.isKeyCode (juce::KeyPress::leftKey) || k.isKeyCode (juce::KeyPress::rightKey))
        {
            auto* p = bp (primary, ids::B::Freq);
            const float f = p->convertFrom0to1 (p->getValue());
            const float m = std::pow (2.0f, (fine ? 1.0f / 48.0f : 1.0f / 12.0f) * (k.isKeyCode (juce::KeyPress::rightKey) ? 1.0f : -1.0f));
            p->beginChangeGesture(); p->setValueNotifyingHost (p->convertTo0to1 (juce::jlimit (10.0f, 30000.0f, f * m))); p->endChangeGesture();
        }
        else
        {
            auto* p = bp (primary, ids::B::Gain);
            const float g = p->convertFrom0to1 (p->getValue());
            const float d = (fine ? 0.1f : 0.5f) * (k.isKeyCode (juce::KeyPress::upKey) ? 1.0f : -1.0f);
            p->beginChangeGesture(); p->setValueNotifyingHost (p->convertTo0to1 (juce::jlimit (-30.0f, 30.0f, g + d))); p->endChangeGesture();
        }
        return true;
    }
    return false;
}

// ----------------------------------------------------------------------------- menu da banda
void EQGraph::showBandMenu (int b)
{
    if (b < 0) return;
    const auto p = st.band (b);
    juce::PopupMenu m, types, place, slopes;
    m.addSectionHeader (tr ("Banda ", "Band ") + juce::String (b + 1));
    for (int t = 0; t < (int) FilterType::NumTypes; ++t)
        types.addItem (uinames::types()[t], true, (int) p.type == t, [this, b, t] { st.performAction ([this, b, t] { st.setBandParam (b, ids::B::Type, (float) t); }); });
    m.addSubMenu (tr ("Tipo", "Type"), types);
    if (typeIsCut (p.type))
    {
        for (int s = 0; s < (int) Slope::NumSlopes; ++s)
            slopes.addItem (names::slopes()[s], true, (int) p.slope == s, [this, b, s] { st.performAction ([this, b, s] { st.setBandParam (b, ids::B::Slope, (float) s); }); });
        m.addSubMenu (tr ("Inclinação", "Slope"), slopes);
    }
    for (int s = 0; s < (int) Placement::NumPlacements; ++s)
        place.addItem (uinames::placements()[s], true, (int) p.placement == s, [this, b, s] { st.performAction ([this, b, s] { st.setBandParam (b, ids::B::Place, (float) s); }); });
    m.addSubMenu (tr ("Colocação", "Placement"), place);
    m.addSeparator();
    m.addItem (tr ("Dinâmica", "Dynamic"), typeSupportsDynamics (p.type), p.dynamic, [this, b, p] { st.performAction ([this, b, p] { st.setBandParam (b, ids::B::Dyn, p.dynamic ? 0.0f : 1.0f); }); });
    m.addItem (tr ("Ouvir (só a zona da banda)", "Listen (band range only)"), true, proc.listenBandIndex() == b, [this, b] { proc.setListenBand (proc.listenBandIndex() == b ? -1 : b); });
    m.addItem (tr ("Bypass da banda", "Band bypass"), true, ! p.enabled, [this, b, p] { st.performAction ([this, b, p] { st.setBandParam (b, ids::B::En, p.enabled ? 0.0f : 1.0f); }); });
    m.addSeparator();
    m.addItem (tr ("Copiar", "Copy"), [this, b] { st.copyBands (selected.contains (b) ? selected : juce::Array<int> { b }); });
    m.addItem (tr ("Colar", "Paste"), StateManager::clipboardHasBands(), false, [this] { auto a = st.pasteBands(); if (! a.isEmpty()) { selected = a; primary = a.getFirst(); notifySelection(); } });
    m.addItem (tr ("Repor banda", "Reset band"), [this, b, p] { st.performAction ([this, b, p] { auto d = BandParams(); d.active = true; d.type = p.type; d.freq = p.freq; d.placement = p.placement;
                                                                        if (typeIsCut (p.type)) d.q = 0.7071; st.setBand (b, d); }); });
    m.addItem (tr ("Remover", "Remove"), [this, b] { st.performAction ([this, b] { st.removeBand (b); }); if (proc.listenBandIndex() == b) proc.setListenBand (-1); selectBand (-1); });
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMousePosition());
}
