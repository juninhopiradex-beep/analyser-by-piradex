#include "BandPanel.h"

using namespace eqm;

// ============================================================================= barra de bandas
BandStrip::BandStrip (EQMotionProcessor& p) : proc (p)
{
    add.getProperties().set ("radius", 5.0f);
    add.onClick = [this] { if (onAdd) onAdd(); };
    clear.getProperties().set ("flat", true);
    clear.onClick = [this]
    {
        proc.state.performAction ([this] { proc.state.clearBands(); });
        proc.setListenBand (-1);
        if (onSelect) onSelect (-1);
    };
    addAndMakeVisible (add);
    addAndMakeVisible (clear);
    lang.bind ([this]
    {
        add.setTooltip (tr ("Adicionar banda", "Add band"));
        clear.setButtonText (tr ("Limpar bandas", "Clear bands"));
        clear.setTooltip (tr ("Remover todas as bandas (pode desfazer)", "Remove all bands (can be undone)"));
    });
    lang.onChange = [this] { repaint(); };
    startTimerHz (10);
}

void BandStrip::timerCallback()
{
    std::vector<int> v;
    for (int b = 0; b < kMaxBands; ++b) if (proc.apvts.getRawParameterValue (ids::band (b, ids::B::On))->load() > 0.5f) v.push_back (b);
    if (v != visible) { visible = v; resized(); }
    repaint();
}

juce::Rectangle<int> BandStrip::chipBounds (int i) const
{
    const int w = visible.size() > 16 ? 34 : 44;
    return { 96 + i * (w + 4), 6, w, getHeight() - 12 };
}

void BandStrip::resized()
{
    auto r = getLocalBounds().reduced (8, 6);
    clear.setBounds (r.removeFromRight (110));
    const int x = chipBounds ((int) visible.size()).getX();
    add.setBounds (x, 6, 26, getHeight() - 12);
    add.setEnabled (visible.size() < (size_t) kMaxBands);
}

void BandStrip::paint (juce::Graphics& g)
{
    g.setColour (pal::bg1);
    g.fillRect (getLocalBounds());
    g.setColour (pal::line);
    g.drawHorizontalLine (0, 0, (float) getWidth());
    g.setColour (pal::textDim);
    g.setFont (fonts::ui (12.0f, 1));
    g.drawText (tr ("Bandas", "Bands"), juce::Rectangle<int> (12, 0, 50, getHeight()), juce::Justification::centredLeft);
    g.setColour (pal::textFaint);
    g.setFont (fonts::mono (10.5f));
    g.drawText (juce::String ((int) visible.size()) + "/24", juce::Rectangle<int> (58, 0, 36, getHeight()), juce::Justification::centredLeft);

    const int listen = proc.listenBandIndex();
    for (size_t i = 0; i < visible.size(); ++i)
    {
        const int b = visible[i];
        auto r = chipBounds ((int) i).toFloat();
        const bool sel = b == selected;
        const bool en = proc.apvts.getRawParameterValue (ids::band (b, ids::B::En))->load() > 0.5f;
        const bool dyn = proc.apvts.getRawParameterValue (ids::band (b, ids::B::Dyn))->load() > 0.5f;
        const auto col = pal::band (b);
        g.setColour (sel ? pal::bg3 : pal::bg2);
        g.fillRoundedRectangle (r, 5.0f);
        if (sel) { g.setColour (col.withAlpha (0.7f)); g.drawRoundedRectangle (r.reduced (0.5f), 5.0f, 1.0f); }
        auto sq = r.withWidth (10).withSizeKeepingCentre (9, 9).translated (8, 0);
        g.setColour (en ? col : pal::textFaint);
        if (en) g.fillRoundedRectangle (sq, 2.0f); else g.drawRoundedRectangle (sq, 2.0f, 1.2f);
        g.setColour (sel ? pal::text : pal::textDim);
        g.setFont (fonts::ui (12.0f, 1));
        g.drawText (juce::String (b + 1), r.withTrimmedLeft (18), juce::Justification::centredLeft);
        if (dyn) { g.setColour (col); g.fillEllipse (r.getRight() - 8, r.getY() + 4, 4, 4); }
        if (b == listen) { g.setColour (pal::accent2); g.fillEllipse (r.getRight() - 8, r.getBottom() - 8, 4, 4); }
    }
}

void BandStrip::mouseDown (const juce::MouseEvent& e)
{
    for (size_t i = 0; i < visible.size(); ++i)
        if (chipBounds ((int) i).contains (e.getPosition()))
        {
            if (e.mods.isPopupMenu()) { if (onContextMenu) onContextMenu (visible[i]); }
            else if (onSelect) onSelect (visible[i]);
            return;
        }
}

// ============================================================================= inspetor
namespace
{
class TypeButton : public juce::Button
{
public:
    explicit TypeButton (int t) : juce::Button (names::types()[t]), type (t) {}
    juce::String getTooltip() override { return uinames::types()[type]; }
    juce::Colour colour = pal::accent;
    void paintButton (juce::Graphics& g, bool over, bool) override
    {
        auto r = getLocalBounds().toFloat().reduced (1.0f);
        const bool on = getToggleState();
        g.setColour (on ? colour.withAlpha (0.16f) : over ? pal::bg3 : pal::bg2);
        g.fillRoundedRectangle (r, 5.0f);
        if (on) { g.setColour (colour.withAlpha (0.65f)); g.drawRoundedRectangle (r, 5.0f, 1.0f); }
        auto p = icons::filterShape (type, r.reduced (8, 7));
        g.setColour (! isEnabled() ? pal::textFaint : on ? colour : over ? pal::text : pal::textDim);
        g.strokePath (p, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
private:
    int type;
};
}

BandInspector::BandInspector (EQMotionProcessor& p) : proc (p)
{
    for (int t = 0; t < (int) FilterType::NumTypes; ++t)
    {
        auto* b = typeButtons.add (new TypeButton (t));
        b->onClick = [this, t]
        {
            if (current < 0) return;
            proc.state.performAction ([this, t]
            {
                proc.state.setBandParam (current, ids::B::Type, (float) t);
                // valores de partida coerentes com o tipo
                if (typeIsCut ((FilterType) t)) proc.state.setBandParam (current, ids::B::Q, 0.7071f);
                if (! typeSupportsDynamics ((FilterType) t)) proc.state.setBandParam (current, ids::B::Dyn, 0.0f);
            });
        };
        addAndMakeVisible (b);
    }
    slope.addItemList (names::slopes(), 1);
    place.addItemList (uinames::placements(), 1);
    for (auto* c : { &slope, &place }) addAndMakeVisible (c);

    for (auto* k : { &freq, &gain, &q, &thr, &range, &ratio, &atk, &hold, &rel, &knee }) addAndMakeVisible (k);

    listen.setClickingTogglesState (false);
    listen.getProperties().set ("accent", (juce::int64) pal::accent2.getARGB());
    listen.onClick = [this] { if (current >= 0) proc.setListenBand (proc.listenBandIndex() == current ? -1 : current); };
    bypass.getProperties().set ("accent", (juce::int64) pal::accent2.getARGB());
    bypass.onClick = [this]
    {
        if (current < 0) return;
        const bool en = proc.apvts.getRawParameterValue (ids::band (current, ids::B::En))->load() > 0.5f;
        proc.state.performAction ([this, en] { proc.state.setBandParam (current, ids::B::En, en ? 0.0f : 1.0f); });
    };
    reset.getProperties().set ("flat", true);
    reset.onClick = [this]
    {
        if (current < 0) return;
        const auto b = proc.state.band (current);
        proc.state.performAction ([this, b]
        {
            BandParams d; d.active = true; d.type = b.type; d.freq = b.freq; d.placement = b.placement; d.slope = b.slope;
            if (typeIsCut (b.type)) d.q = 0.7071; else d.q = 1.0;
            proc.state.setBand (current, d);
        });
    };
    remove.getProperties().set ("flat", true);
    remove.onClick = [this]
    {
        if (current < 0) return;
        const int b = current;
        proc.state.performAction ([this, b] { proc.state.removeBand (b); });
        if (proc.listenBandIndex() == b) proc.setListenBand (-1);
        if (onBandRemoved) onBandRemoved();
    };
    for (auto* b : { &listen, &bypass, &reset, &remove }) addAndMakeVisible (b);

    addAndMakeVisible (dynOn);
    for (auto* s : { &dir, &src, &det }) addAndMakeVisible (s);
    lang.bind ([this]
    {
        const int sp = place.getSelectedId();
        const auto pl = uinames::placements();
        for (int i = 0; i < pl.size(); ++i) place.changeItemText (i + 1, pl[i]);
        if (sp > 0) place.setSelectedId (sp, juce::dontSendNotification);
        slope.setTooltip (tr ("Inclinação do corte", "Cut slope"));
        place.setTooltip (tr ("Onde a banda atua: estéreo, só esquerda/direita, só Mid ou só Side", "Where the band acts: stereo, left/right only, Mid only or Side only"));
        freq.setText (tr ("FREQUÊNCIA", "FREQUENCY"));
        gain.setText (tr ("GANHO", "GAIN"));
        thr.setText (tr ("LIMIAR", "THRESHOLD"));
        ratio.setText ("RATIO");
        atk.setText (tr ("ATAQUE", "ATTACK"));
        rel.setText (tr ("LIBERTAÇÃO", "RELEASE"));
        knee.setText (tr ("JOELHO", "KNEE"));
        listen.setButtonText (tr ("Ouvir", "Listen"));
        listen.setTooltip (tr ("Ouvir só a zona desta banda", "Hear only this band's range"));
        bypass.setTooltip (tr ("Tirar esta banda do processamento sem a apagar", "Take this band out of processing without deleting it"));
        reset.setButtonText (tr ("Repor", "Reset"));
        reset.setTooltip (tr ("Repor os valores desta banda", "Reset this band's values"));
        remove.setButtonText (tr ("Remover", "Remove"));
        dynOn.setButtonText (tr ("Dinâmica", "Dynamic"));
        dir.setLabels ({ tr ("Acima", "Above"), tr ("Abaixo", "Below") });
        dir.setTooltips ({ tr ("Atua quando o detetor sobe acima do limiar", "Acts when the detector rises above the threshold"),
                           tr ("Atua quando o detetor desce abaixo do limiar", "Acts when the detector falls below the threshold") });
        src.setLabels ({ tr ("Pista", "Track"), "Sidechain" });
        src.setTooltips ({ tr ("Detetor ouve esta pista", "Detector listens to this track"),
                           tr ("Detetor ouve a entrada sidechain (o sinal principal não muda)", "Detector listens to the sidechain input (the main signal is unchanged)") });
        det.setTooltips ({ tr ("Picos (rápido)", "Peaks (fast)"), tr ("Média quadrática (mais suave)", "RMS (smoother)") });
    });
    lang.onChange = [this] { repaint(); };
    thr.setColour (pal::accent);
    setBand (-1);
    startTimerHz (30);
}

void BandInspector::setBand (int b)
{
    current = b;
    const bool has = b >= 0;
    for (auto* c : getChildren()) c->setVisible (has);
    if (! has)
    {
        slopeAtt.reset(); placeAtt.reset(); dynAtt.reset();
        for (auto* k : { &freq, &gain, &q, &thr, &range, &ratio, &atk, &hold, &rel, &knee }) k->attach (nullptr);
        for (auto* s : { &dir, &src, &det }) s->attach (nullptr);
        repaint();
        return;
    }
    using ids::B;
    freq.attach (bp (B::Freq)); gain.attach (bp (B::Gain)); q.attach (bp (B::Q));
    thr.attach (bp (B::Thr)); range.attach (bp (B::Range)); ratio.attach (bp (B::Ratio));
    atk.attach (bp (B::Atk)); hold.attach (bp (B::Hold)); rel.attach (bp (B::Rel)); knee.attach (bp (B::Knee));
    slopeAtt = std::make_unique<juce::ComboBoxParameterAttachment> (*bp (B::Slope), slope);
    placeAtt = std::make_unique<juce::ComboBoxParameterAttachment> (*bp (B::Place), place);
    dynAtt = std::make_unique<juce::ButtonParameterAttachment> (*bp (B::Dyn), dynOn);
    dir.attach (bp (B::Dir)); src.attach (bp (B::Src)); det.attach (bp (B::Det));
    const auto col = pal::band (b);
    for (auto* k : { &freq, &gain, &q, &thr, &range, &ratio, &atk, &hold, &rel, &knee }) k->setColour (col);
    for (auto* t : typeButtons) static_cast<TypeButton*> (t)->colour = col;
    dynOn.getProperties().set ("accent", (juce::int64) col.getARGB());
    for (auto* s : { &dir, &src, &det }) s->setAccent (col);
    shown = proc.state.band (b);
    refreshEnablement();
    resized();
    repaint();
}

void BandInspector::refreshEnablement()
{
    if (current < 0) return;
    const auto& p = shown;
    for (int t = 0; t < typeButtons.size(); ++t) typeButtons[t]->setToggleState ((int) p.type == t, juce::dontSendNotification);
    slope.setEnabled (typeIsCut (p.type));
    gain.setEnabled (typeHasGain (p.type));
    q.setEnabled (p.type != FilterType::Tilt && ! (typeIsCut (p.type) && (p.slope == Slope::dB6 || p.slope == Slope::Brickwall)));
    const bool canDyn = typeSupportsDynamics (p.type);
    dynOn.setEnabled (canDyn);
    const bool dynActive = canDyn && p.dynamic;
    for (auto* k : { &thr, &range, &ratio, &atk, &hold, &rel, &knee }) k->setEnabled (dynActive);
    for (auto* s : { &dir, &src, &det }) s->setEnabled (dynActive);
    bypass.setToggleState (! p.enabled, juce::dontSendNotification);
    listen.setToggleState (proc.listenBandIndex() == current, juce::dontSendNotification);
}

void BandInspector::timerCallback()
{
    if (current < 0) return;
    const auto b = proc.state.band (current);
    if (! b.active) return;
    if (b.type != shown.type || b.slope != shown.slope || b.dynamic != shown.dynamic || b.enabled != shown.enabled
        || listen.getToggleState() != (proc.listenBandIndex() == current))
    { shown = b; refreshEnablement(); repaint(); }
    shown = b;
    if (b.isDynamic())
    {
        liveGr = (float) proc.bandDynamicDb (current);
        liveLevel = (float) proc.bandDetectorDb (current);
        repaint (meterArea);
    }
}

void BandInspector::resized()
{
    // layout compacto quando a janela é estreita (portáteis)
    const bool compact = getWidth() < 1060;
    const int typeW = compact ? 60 : 68, knobW = compact ? 70 : 78, utilW = compact ? 118 : 128;
    auto r = getLocalBounds().reduced (compact ? 10 : 12, 8);
    // coluna do tipo
    auto left = r.removeFromLeft (typeW * 3 + 4);
    headerArea = left.removeFromTop (22);
    left.removeFromTop (4);
    auto grid = left.removeFromTop (88);
    for (int t = 0; t < typeButtons.size(); ++t)
    {
        const int col = t % 3, row = t / 3;
        typeButtons[t]->setBounds (grid.getX() + col * (typeW + 2), grid.getY() + row * 29, typeW, 27);
    }
    left.removeFromTop (6);
    slope.setBounds (left.removeFromTop (26));

    r.removeFromLeft (compact ? 8 : 14);
    auto knobs = r.removeFromLeft (3 * knobW);
    for (auto* k : { &freq, &gain, &q }) k->setBounds (knobs.removeFromLeft (knobW).withTrimmedBottom (4));

    r.removeFromLeft (compact ? 4 : 8);
    auto util = r.removeFromLeft (utilW);
    util.removeFromTop (16);
    place.setBounds (util.removeFromTop (26));
    util.removeFromTop (8);
    auto row = util.removeFromTop (26);
    const int half = (utilW - 4) / 2;
    listen.setBounds (row.removeFromLeft (half)); row.removeFromLeft (4); bypass.setBounds (row);
    util.removeFromTop (6);
    row = util.removeFromTop (24);
    reset.setBounds (row.removeFromLeft (half)); row.removeFromLeft (4); remove.setBounds (row);

    r.removeFromLeft (compact ? 14 : 18);
    dynArea = r;
    auto top = r.removeFromTop (26);
    dynOn.setBounds (top.removeFromLeft (compact ? 100 : 112));
    const int segW = (top.getWidth() - 12) / 3;
    dir.setBounds (top.removeFromLeft (segW).reduced (1, 1)); top.removeFromLeft (6);
    src.setBounds (top.removeFromLeft (segW).reduced (1, 1)); top.removeFromLeft (6);
    det.setBounds (top.removeFromLeft (segW).reduced (1, 1));
    r.removeFromTop (6);
    meterArea = r.removeFromRight (compact ? 38 : 46);
    const int kw = juce::jmin (74, r.getWidth() / 7);
    for (auto* k : { &thr, &range, &ratio, &atk, &hold, &rel, &knee }) k->setBounds (r.removeFromLeft (kw).withTrimmedBottom (4));
}

void BandInspector::paint (juce::Graphics& g)
{
    g.setColour (pal::bg1);
    g.fillRect (getLocalBounds());
    if (current < 0)
    {
        g.setColour (pal::textFaint);
        g.setFont (fonts::ui (13.0f));
        g.drawText (tr ("Selecione uma banda — ou faça duplo clique no gráfico para criar uma.", "Select a band — or double-click the graph to create one."),
                    getLocalBounds(), juce::Justification::centred);
        return;
    }
    const auto col = pal::band (current);
    auto headerArea = this->headerArea;
    auto meterArea = this->meterArea;
    g.setColour (col);
    g.fillRoundedRectangle (headerArea.removeFromLeft (12).withSizeKeepingCentre (10, 10).toFloat(), 2.5f);
    g.setColour (pal::text);
    g.setFont (fonts::ui (15.0f, 2));
    g.drawText (tr ("Banda ", "Band ") + juce::String (current + 1), headerArea.withTrimmedLeft (8), juce::Justification::centredLeft);
    g.setColour (pal::textDim);
    g.setFont (fonts::ui (12.5f));
    g.drawText (uinames::types()[(int) shown.type], headerArea.withTrimmedLeft (90), juce::Justification::centredLeft);
    if (proc.state.bandTag (current) > 0)
    {
        g.setColour (pal::accent);
        g.setFont (fonts::ui (10.5f, 2));
        g.drawText ("SMART OPS", headerArea.withTrimmedLeft (150), juce::Justification::centredLeft);
    }

    // separador antes da dinâmica
    g.setColour (pal::line);
    g.drawVerticalLine (dynArea.getX() - 10, (float) dynArea.getY() + 2, (float) dynArea.getBottom() - 2);
    g.setColour (pal::textFaint);
    g.setFont (fonts::ui (10.5f, 1, 0.04f));
    g.drawText (tr ("COLOCAÇÃO", "PLACEMENT"), place.getBounds().translated (0, -16).withHeight (14), juce::Justification::centredLeft);

    // medidor dinâmico: nível do detetor vs limiar e redução atual
    if (shown.isDynamic())
    {
        auto m = meterArea.reduced (4, 14).toFloat();
        auto lvl = m.removeFromLeft (m.getWidth() * 0.45f);
        m.removeFromLeft (4);
        auto yFor = [&] (float db, juce::Rectangle<float> b) { return b.getBottom() - juce::jlimit (0.0f, 1.0f, (db + 80.0f) / 80.0f) * b.getHeight(); };
        g.setColour (pal::bg0); g.fillRoundedRectangle (lvl, 2); g.fillRoundedRectangle (m, 2);
        g.setColour (col.withAlpha (0.65f));
        g.fillRect (lvl.withTop (yFor (liveLevel, lvl)));
        g.setColour (pal::text);
        g.fillRect (lvl.getX() - 2, yFor ((float) shown.thresholdDb, lvl) - 0.5f, lvl.getWidth() + 4, 1.5f);
        const float span = juce::jmax (1.0f, std::abs ((float) shown.rangeDb));
        const float frac = juce::jlimit (0.0f, 1.0f, std::abs (liveGr) / span);
        g.setColour (col);
        if (shown.rangeDb < 0) g.fillRect (m.withHeight (m.getHeight() * frac));
        else g.fillRect (m.withTop (m.getBottom() - m.getHeight() * frac));
        g.setColour (pal::textDim);
        g.setFont (fonts::mono (9.5f));
        g.drawText (juce::String (liveGr, 1), meterArea.removeFromBottom (13), juce::Justification::centred);
        g.setFont (fonts::ui (9.0f, 1));
        g.drawText ("DET  GR", meterArea.removeFromTop (12), juce::Justification::centred);
    }
}
