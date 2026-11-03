// EQMotion — gaveta das Smart Ops: Operação → Intervalo e intensidade → Aprender → Rever → Aplicar
#pragma once
#include "Widgets.h"
#include "../PluginProcessor.h"

class SmartOpsPanel : public juce::Component, private juce::Timer
{
public:
    explicit SmartOpsPanel (EQMotionProcessor& p) : proc (p)
    {
        lang.bind ([this]
        {
            eqm::smartEnglish().store (! piradex::i18n::isPortuguese());
            rebuildOps();
            reference.setTooltip (tr ("De onde vem a referência: entrada sidechain ou o espetro médio de outra instância do EQMotion",
                                      "Where the reference comes from: the sidechain input or the average spectrum of another EQMotion instance"));
            revert.setTooltip (tr ("Remove só as bandas criadas pela última operação — as suas outras edições ficam",
                                   "Removes only the bands created by the last operation — your other edits stay"));
            from.setLabel (tr ("De", "From"));
            to.setLabel (tr ("Até", "To"));
            amount.setLabel (tr ("Intensidade", "Amount"));
            apply.setButtonText (tr ("Aplicar", "Apply"));
            revert.setButtonText (tr ("Reverter", "Revert"));
            refIds.clear();          // reconstrói a lista de referências no idioma novo
        });
        lang.onChange = [this] { proc.smartRecompute(); refresh(); };
        op.onChange = [this] { proc.smartSetOp ((eqm::SmartOp) (op.getSelectedId() - 1)); refresh(); };
        addAndMakeVisible (op);

        auto setupBar = [this] (juce::Slider& s, double lo, double hi, bool logScale)
        {
            s.setRange (lo, hi);
            if (logScale) s.setSkewFactorFromMidPoint (std::sqrt (lo * hi));
            s.setVelocityModeParameters (0.6, 1, 0.08, true, juce::ModifierKeys::commandModifier);
            addAndMakeVisible (s);
        };
        setupBar (from, 20, 20000, true);
        setupBar (to, 20, 20000, true);
        setupBar (amount, 0, 100, false);
        from.onValueChange = [this] { proc.smart.fromHz = juce::jmin (from.getValue(), proc.smart.toHz / 1.2); proc.smartRangeChanged(); };
        to.onValueChange = [this] { proc.smart.toHz = juce::jmax (to.getValue(), proc.smart.fromHz * 1.2); proc.smartRangeChanged(); };
        amount.onValueChange = [this] { proc.smart.amount = amount.getValue() * 0.01; proc.smartRecompute(); };
        from.setDoubleClickReturnValue (true, 20); to.setDoubleClickReturnValue (true, 20000); amount.setDoubleClickReturnValue (true, 100);

        reference.onChange = [this]
        {
            proc.smart.referenceInstance = refIds[(size_t) juce::jmax (0, reference.getSelectedItemIndex())];
            proc.smartRecompute();
        };
        addAndMakeVisible (reference);

        learn.getProperties().set ("radius", 7.0f);
        learn.onClick = [this]
        {
            if (proc.smart.learning) proc.smartStopLearning(); else proc.smartStartLearning();
            refresh();
        };
        addAndMakeVisible (learn);

        apply.getProperties().set ("primary", true);
        apply.onClick = [this] { proc.smartApply(); refresh(); };
        revert.onClick = [this] { proc.smartRevert(); refresh(); };
        addAndMakeVisible (apply); addAndMakeVisible (revert);

        close.onClick = [this] { if (onClose) onClose(); };
        addAndMakeVisible (close);
        refresh();
        startTimerHz (10);
    }

    std::function<void()> onClose;

    void refresh()
    {
        auto& s = proc.smart;
        op.setSelectedId ((int) s.op + 1, juce::dontSendNotification);
        from.setValue (s.fromHz, juce::dontSendNotification);
        to.setValue (s.toHz, juce::dontSendNotification);
        amount.setValue (s.amount * 100.0, juce::dontSendNotification);
        const bool isMatch = s.op == eqm::SmartOp::MatchReference;
        reference.setVisible (isMatch);
        if (isMatch) rebuildReferences();
        const double secs = s.learning ? proc.smartLearnSeconds() : 0.0;
        learn.setButtonText (s.learning ? tr ("Parar  ·  ", "Stop  ·  ") + decimal (secs, 1) + " s" : tr ("Aprender", "Learn"));
        learn.setToggleState (s.learning, juce::dontSendNotification);
        learn.setEnabled (! s.learning || secs >= 2.0);   // pelo menos 2 s
        apply.setEnabled (s.suggestion.ok && ! s.learning);
        revert.setEnabled (proc.state.hasSmartBands (s.lastAppliedOp));
        resized();
        repaint();
    }

    // altura mínima confortável (o editor garante pelo menos isto na altura mínima da janela)
    static constexpr int kComfortH = 470;

    void resized() override
    {
        const bool compact = getHeight() < kComfortH;
        const int gap = compact ? 6 : 12, sub = compact ? 4 : 6, stepH = compact ? 18 : 20, ctl = compact ? 24 : 26;
        auto r = getLocalBounds().reduced (16, compact ? 8 : 12);
        auto head = r.removeFromTop (compact ? 24 : 28);
        close.setBounds (head.removeFromRight (24).withSizeKeepingCentre (24, 24));
        titleArea = head;
        r.removeFromTop (compact ? 4 : 8);

        // 5 · Aplicar — fixo no fundo, nunca é empurrado para fora do painel
        auto row = r.removeFromBottom (compact ? 28 : 30);
        apply.setBounds (row.removeFromLeft (row.getWidth() / 2 - 3)); row.removeFromLeft (6); revert.setBounds (row);
        r.removeFromBottom (sub);
        step5 = r.removeFromBottom (stepH);
        r.removeFromBottom (gap);

        step1 = r.removeFromTop (stepH); r.removeFromTop (sub);
        op.setBounds (r.removeFromTop (ctl + 2));
        if (reference.isVisible()) { r.removeFromTop (sub); reference.setBounds (r.removeFromTop (ctl)); }
        r.removeFromTop (gap);
        step2 = r.removeFromTop (stepH); r.removeFromTop (sub);
        row = r.removeFromTop (ctl);
        from.setBounds (row.removeFromLeft (row.getWidth() / 2 - 3)); row.removeFromLeft (6); to.setBounds (row);
        r.removeFromTop (sub);
        amount.setBounds (r.removeFromTop (ctl));
        r.removeFromTop (gap);
        step3 = r.removeFromTop (stepH); r.removeFromTop (sub);
        learn.setBounds (r.removeFromTop (compact ? 28 : 32));
        progress = r.removeFromTop (6).reduced (0, 1);
        r.removeFromTop (compact ? 2 : 6);
        step4 = r.removeFromTop (stepH); r.removeFromTop (4);
        review = r;                         // o que sobra (o texto é cortado à altura disponível)
    }

    void paint (juce::Graphics& g) override
    {
        g.setColour (pal::bg1);
        g.fillRect (getLocalBounds());
        g.setColour (pal::line);
        g.drawVerticalLine (getWidth() - 1, 0, (float) getHeight());
        auto t = titleArea;
        juce::Path sp = icons::spark();
        sp.applyTransform (juce::AffineTransform::scale (16, 16).translated ((float) t.getX(), (float) t.getCentreY() - 8));
        g.setColour (pal::accent); g.fillPath (sp);
        g.setColour (pal::text); g.setFont (fonts::ui (16.0f, 2));
        g.drawText ("Smart Ops", t.withTrimmedLeft (24), juce::Justification::centredLeft);

        auto& s = proc.smart;
        drawStep (g, step1, 1, tr ("Operação", "Operation"), true);
        drawStep (g, step2, 2, tr ("Intervalo e intensidade", "Range and amount"), true);
        drawStep (g, step3, 3, tr ("Aprender", "Learn"), s.learning || s.hasResult);
        drawStep (g, step4, 4, tr ("Rever", "Review"), s.hasResult);
        drawStep (g, step5, 5, tr ("Aplicar", "Apply"), s.suggestion.ok);

        // progresso: barra até aos 2 s mínimos e depois contínua
        if (s.learning)
        {
            const double secs = proc.smartLearnSeconds();
            g.setColour (pal::bg3); g.fillRoundedRectangle (progress.toFloat(), 2);
            const float frac = (float) juce::jmin (1.0, secs / 2.0);
            g.setColour (secs >= 2.0 ? pal::ok : pal::accent);
            g.fillRoundedRectangle (progress.toFloat().withWidth (progress.getWidth() * frac), 2);
        }

        // rever — tudo dentro da área disponível, nunca por cima do passo 5
        {
            juce::Graphics::ScopedSaveState ss (g);
            g.reduceClipRegion (review);
            auto rv = review;
            juce::String status, detail;
            juce::Colour dot = pal::textFaint;
            if (s.learning) { status = tr ("A aprender", "Learning"); detail = tr ("Ainda a mudar. Pare quando a passagem acabar (mínimo 2 s).", "Still changing. Stop when the passage ends (minimum 2 s)."); dot = pal::accent; }
            else if (s.suggestion.ok) { const int nb = (int) s.suggestion.bands.size(); status = tr ("Sugestão: ", "Suggestion: ") + juce::String (nb) + (nb == 1 ? tr (" banda", " band") : tr (" bandas", " bands")); detail = s.message; dot = pal::accent; }
            else if (s.appliedLock) { status = tr ("Aplicado", "Applied"); detail = s.message; dot = pal::ok; }
            else if (s.message.isNotEmpty()) { status = s.hasResult ? tr ("Sem alterações propostas", "No changes proposed") : tr ("Pronto", "Ready"); detail = s.message; dot = s.hasResult ? pal::textDim : pal::ok; }
            else { status = tr ("À espera", "Waiting"); detail = hint (s.op); }
            g.setColour (dot); g.fillEllipse ((float) rv.getX(), (float) rv.getY() + 5, 8, 8);
            g.setColour (pal::text); g.setFont (fonts::ui (13.5f, 2));
            g.drawText (status, rv.removeFromTop (18).withTrimmedLeft (16), juce::Justification::centredLeft);
            rv = rv.withTrimmedLeft (16).withTrimmedTop (2);

            const float lineH = 15.0f;
            auto paragraph = [&] (const juce::String& text, juce::Colour c, float size)
            {
                if (text.isEmpty() || rv.getHeight() < (int) lineH) return;
                juce::AttributedString as; as.setJustification (juce::Justification::topLeft); as.setWordWrap (juce::AttributedString::byWord);
                as.append (text, fonts::ui (size), c); as.setLineSpacing (1.0f);
                juce::TextLayout tl; tl.createLayout (as, (float) rv.getWidth());
                // só linhas inteiras: o que não couber fica de fora em vez de se sobrepor
                float used = 0; int lines = 0;
                for (int i = 0; i < tl.getNumLines(); ++i) { const float h = tl.getLine (i).ascent + tl.getLine (i).descent + 1.0f; if (used + h > (float) rv.getHeight()) break; used += h; ++lines; }
                if (lines == 0) return;
                juce::Graphics::ScopedSaveState s2 (g);
                g.reduceClipRegion (rv.withHeight ((int) std::ceil (used)));
                tl.draw (g, rv.toFloat());
                rv.removeFromTop ((int) std::ceil (used) + 4);
            };
            paragraph (detail, pal::textDim, 12.0f);
            if (s.suggestion.ok)
            {
                for (auto& b : s.suggestion.bands)
                {
                    if (rv.getHeight() < (int) lineH) break;
                    auto line = rv.removeFromTop ((int) lineH);
                    g.setColour (pal::textDim); g.setFont (fonts::mono (11.0f));
                    g.drawText (bandLine (b), line, juce::Justification::centredLeft);
                }
                paragraph (tr ("Tracejado no gráfico. A Intensidade ajusta a força em tempo real.", "Dashed on the graph. Amount adjusts the strength in real time."), pal::textFaint, 11.5f);
            }
        }
    }

private:
    static juce::String hint (eqm::SmartOp op)
    {
        switch (op)
        {
            case eqm::SmartOp::Deresonate:     return tr ("Encontra ressonâncias estreitas e persistentes e propõe cortes dinâmicos limitados.", "Finds narrow, persistent resonances and proposes limited dynamic cuts.");
            case eqm::SmartOp::DeEss:          return tr ("Procura a sibilância na zona escolhida e propõe 1–2 bandas dinâmicas.", "Looks for sibilance in the chosen range and proposes 1–2 dynamic bands.");
            case eqm::SmartOp::AutoCut:        return tr ("Propõe cortes de graves/agudos onde só há ruído de fundo. Sub e ar musicais são protegidos.", "Proposes low/high cuts where there is only background noise. Musical sub and air are protected.");
            case eqm::SmartOp::MatchReference: return tr ("Toque este sinal e a referência (no sidechain, ou outra instância) e aprenda.", "Play this signal and the reference (on the sidechain, or another instance) and learn.");
            case eqm::SmartOp::Unmask:         return tr ("Com a voz (ou outro sinal) no sidechain, abre espaço só quando ele toca.", "With the vocal (or another signal) on the sidechain, makes room only while it plays.");
            case eqm::SmartOp::TonalBalance:   return tr ("Compara com o equilíbrio médio de misturas e propõe correções largas e suaves (±4 dB).", "Compares with the average balance of mixes and proposes broad, gentle corrections (±4 dB).");
            case eqm::SmartOp::Mud:            return tr ("Procura acumulação nos graves-médios e corta-a só quando aparece.", "Looks for low-mid build-up and cuts it only when it shows up.");
            case eqm::SmartOp::Boxy:           return tr ("Procura o som de caixa ou nasal nos médios e propõe cortes dinâmicos.", "Looks for boxy or nasal tone in the mids and proposes dynamic cuts.");
            case eqm::SmartOp::Harsh:          return tr ("Procura dureza nos médios-agudos (2–8 kHz) e doma-a nos momentos fortes.", "Looks for harshness in the upper mids (2–8 kHz) and tames it on the loud moments.");
            case eqm::SmartOp::LowEnd:         return tr ("Encontra o 'boom' dos graves e segura-o quando sobe, sem tirar o sub.", "Finds the low-end boom and holds it when it rises, without removing the sub.");
            case eqm::SmartOp::Presence:       return tr ("Compara com o equilíbrio médio e realça a presença (2–5 kHz) se faltar. Máx. +4 dB.", "Compares with the average balance and lifts presence (2–5 kHz) if it is lacking. Max +4 dB.");
            case eqm::SmartOp::Air:            return tr ("Realça o ar acima de 8 kHz se faltar e houver conteúdo. Máx. +5 dB.", "Lifts the air above 8 kHz if it is lacking and there is content. Max +5 dB.");
            case eqm::SmartOp::Warmth:         return tr ("Realça o corpo à volta de 150–300 Hz se o som estiver fino. Máx. +3 dB.", "Lifts the body around 150–300 Hz if the sound is thin. Max +3 dB.");
            case eqm::SmartOp::Hum:            return tr ("Deteta zumbido elétrico de 50 ou 60 Hz e os harmónicos e propõe notches estreitos.", "Detects 50 or 60 Hz electrical hum and its harmonics and proposes narrow notches.");
            case eqm::SmartOp::AutoDynamic:    return tr ("Encontra as zonas que saltam mais do que o resto do sinal e controla-as dinamicamente.", "Finds the ranges that jump more than the rest of the signal and controls them dynamically.");
            default: return {};
        }
    }

    void rebuildOps()
    {
        op.clear (juce::dontSendNotification);
        for (auto& gr : eqm::smartOpGroups())
        {
            op.addSectionHeading (tr (gr.pt, gr.en));
            for (auto o : gr.ops) op.addItem (juce::String::fromUTF8 (eqm::smartOpName (o)), (int) o + 1);
        }
        op.setSelectedId ((int) proc.smart.op + 1, juce::dontSendNotification);
    }

    static juce::String bandLine (const eqm::BandParams& b)
    {
        using eqm::FilterType;
        juce::String type;
        switch (b.type)
        {
            case FilterType::LowShelf:  type = tr ("Shelf grave", "Low shelf"); break;
            case FilterType::HighShelf: type = tr ("Shelf agudo", "High shelf"); break;
            case FilterType::LowCut:    type = tr ("Corte grave", "Low cut"); break;
            case FilterType::HighCut:   type = tr ("Corte agudo", "High cut"); break;
            case FilterType::Notch:     type = "Notch"; break;
            default:                    type = tr ("Sino", "Bell"); break;
        }
        juce::String s = juce::String::formatted ("%-9s ", hzText (b.freq).toRawUTF8()) + type;
        const bool cut = b.type == FilterType::LowCut || b.type == FilterType::HighCut;
        if (b.isDynamic()) s << "  " << decimal (b.rangeDb, 1) << tr (" dB din.", " dB dyn.");
        else if (! cut)    s << "  " << (b.gainDb > 0 ? "+" : "") << decimal (b.gainDb, 1) << " dB";
        if (! cut && b.type != FilterType::LowShelf && b.type != FilterType::HighShelf) s << "  Q " << decimal (b.q, 1);
        return s;
    }

    void rebuildReferences()
    {
        juce::StringArray namesList { "Sidechain" };
        std::vector<int> ids { 0 };
        for (auto& inst : InstanceRegistry::get().all())
            if (inst != proc.shared) { const juce::SpinLock::ScopedLockType lk (inst->lock); namesList.add (tr ("Instância: ", "Instance: ") + inst->name); ids.push_back (inst->id); }
        if (ids != refIds)
        {
            refIds = ids;
            reference.clear (juce::dontSendNotification);
            reference.addItemList (namesList, 1);
        }
        int sel = 0;
        for (size_t i = 0; i < refIds.size(); ++i) if (refIds[i] == proc.smart.referenceInstance) sel = (int) i;
        reference.setSelectedItemIndex (sel, juce::dontSendNotification);
    }

    void timerCallback() override { if (proc.smart.learning || ++tick % 10 == 0) refresh(); }

    EQMotionProcessor& proc;
    juce::ComboBox op, reference;
    std::vector<int> refIds;
    static juce::String decimal (double v, int d) { auto t = juce::String (v, d); return piradex::i18n::isPortuguese() ? t.replace (".", ",") : t; }
    static juce::String hzText (double f) { return f >= 1000 ? decimal (f / 1000.0, 1) + " kHz" : juce::String ((int) std::round (f)) + " Hz"; }
    ValueBar from { "De", hzText }, to { juce::String::fromUTF8 ("Até"), hzText },
             amount { "Amount", [] (double v) { return juce::String ((int) std::round (v)) + " %"; } };
    juce::TextButton learn { "Aprender" }, apply { "Aplicar" }, revert { "Reverter" };
    IconButton close { "Fechar", [] { juce::Path p; p.startNewSubPath (0.25f, 0.25f); p.lineTo (0.75f, 0.75f); p.startNewSubPath (0.75f, 0.25f); p.lineTo (0.25f, 0.75f); return p; }() };
    juce::Rectangle<int> titleArea, step1, step2, step3, step4, step5, progress, review;
    int tick = 0;
    piradex::i18n::Binder lang;
};
