// EQMotion — gaveta das Smart Ops: Operação → Intervalo e intensidade → Aprender → Rever → Aplicar
#pragma once
#include "Widgets.h"
#include "../PluginProcessor.h"

class SmartOpsPanel : public juce::Component, private juce::Timer
{
public:
    explicit SmartOpsPanel (EQMotionProcessor& p) : proc (p)
    {
        for (int i = 0; i < (int) eqm::SmartOp::NumOps; ++i) op.addItem (juce::String::fromUTF8 (eqm::smartOpName ((eqm::SmartOp) i)), i + 1);
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
        from.onValueChange = [this] { proc.smart.fromHz = juce::jmin (from.getValue(), proc.smart.toHz / 1.2); proc.smart.rangeTouched = true; proc.smartRecompute(); };
        to.onValueChange = [this] { proc.smart.toHz = juce::jmax (to.getValue(), proc.smart.fromHz * 1.2); proc.smart.rangeTouched = true; proc.smartRecompute(); };
        amount.onValueChange = [this] { proc.smart.amount = amount.getValue() * 0.01; proc.smartRecompute(); };
        from.setDoubleClickReturnValue (true, 20); to.setDoubleClickReturnValue (true, 20000); amount.setDoubleClickReturnValue (true, 100);

        reference.onChange = [this]
        {
            proc.smart.referenceInstance = refIds[(size_t) juce::jmax (0, reference.getSelectedItemIndex())];
            proc.smartRecompute();
        };
        reference.setTooltip (juce::String::fromUTF8 ("De onde vem a referência: entrada sidechain ou o espetro médio de outra instância do EQMotion"));
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
        revert.setTooltip (juce::String::fromUTF8 ("Remove só as bandas criadas pela última operação — as suas outras edições ficam"));
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
        learn.setButtonText (s.learning ? juce::String::fromUTF8 ("Parar  ·  ") + juce::String (secs, 1).replace (".", ",") + " s" : juce::String ("Aprender"));
        learn.setToggleState (s.learning, juce::dontSendNotification);
        learn.setEnabled (! s.learning || secs >= 2.0);   // pelo menos 2 s
        apply.setEnabled (s.suggestion.ok && ! s.learning);
        revert.setEnabled (proc.state.hasSmartBands (s.lastAppliedOp));
        resized();
        repaint();
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (16, 12);
        auto head = r.removeFromTop (28);
        close.setBounds (head.removeFromRight (24).withSizeKeepingCentre (24, 24));
        titleArea = head;
        r.removeFromTop (8);
        step1 = r.removeFromTop (20); r.removeFromTop (6);
        op.setBounds (r.removeFromTop (28));
        if (reference.isVisible()) { r.removeFromTop (6); reference.setBounds (r.removeFromTop (26)); }
        r.removeFromTop (12);
        step2 = r.removeFromTop (20); r.removeFromTop (6);
        auto row = r.removeFromTop (26);
        from.setBounds (row.removeFromLeft (row.getWidth() / 2 - 3)); row.removeFromLeft (6); to.setBounds (row);
        r.removeFromTop (6);
        amount.setBounds (r.removeFromTop (26));
        r.removeFromTop (12);
        step3 = r.removeFromTop (20); r.removeFromTop (6);
        learn.setBounds (r.removeFromTop (32));
        progress = r.removeFromTop (6).reduced (0, 1);
        r.removeFromTop (8);
        step4 = r.removeFromTop (20); r.removeFromTop (4);
        review = r.removeFromTop (juce::jmax (40, r.getHeight() - 70));
        r.removeFromTop (6);
        step5 = r.removeFromTop (20); r.removeFromTop (6);
        row = r.removeFromTop (30);
        apply.setBounds (row.removeFromLeft (row.getWidth() / 2 - 3)); row.removeFromLeft (6); revert.setBounds (row);
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
        drawStep (g, step1, 1, juce::String::fromUTF8 ("Operação"), true);
        drawStep (g, step2, 2, "Intervalo e intensidade", true);
        drawStep (g, step3, 3, "Aprender", s.learning || s.hasResult);
        drawStep (g, step4, 4, "Rever", s.hasResult);
        drawStep (g, step5, 5, "Aplicar", s.suggestion.ok);

        // progresso: barra até aos 2 s mínimos e depois contínua
        if (s.learning)
        {
            const double secs = proc.smartLearnSeconds();
            g.setColour (pal::bg3); g.fillRoundedRectangle (progress.toFloat(), 2);
            const float frac = (float) juce::jmin (1.0, secs / 2.0);
            g.setColour (secs >= 2.0 ? pal::ok : pal::accent);
            g.fillRoundedRectangle (progress.toFloat().withWidth (progress.getWidth() * frac), 2);
        }

        // rever
        auto rv = review;
        juce::String status, detail;
        juce::Colour dot = pal::textFaint;
        if (s.learning) { status = "A aprender"; detail = juce::String::fromUTF8 ("Ainda a mudar. Pare quando a passagem acabar (mínimo 2 s)."); dot = pal::accent; }
        else if (s.suggestion.ok) { status = juce::String::fromUTF8 ("Sugestão: ") + juce::String ((int) s.suggestion.bands.size()) + ((int) s.suggestion.bands.size() == 1 ? " banda" : " bandas"); detail = s.message + juce::String::fromUTF8 ("\nTracejado no gráfico. O Amount ajusta a força em tempo real."); dot = pal::accent; }
        else if (s.message.isNotEmpty()) { status = s.hasResult ? juce::String::fromUTF8 ("Sem alterações propostas") : juce::String ("Pronto"); detail = s.message; dot = s.hasResult ? pal::textDim : pal::ok; }
        else { status = juce::String::fromUTF8 ("À espera"); detail = hint (s.op); }
        g.setColour (dot); g.fillEllipse ((float) rv.getX(), (float) rv.getY() + 5, 8, 8);
        g.setColour (pal::text); g.setFont (fonts::ui (13.5f, 2));
        g.drawText (status, rv.removeFromTop (18).withTrimmedLeft (16), juce::Justification::centredLeft);
        g.setColour (pal::textDim); g.setFont (fonts::ui (12.0f));
        g.drawFittedText (detail, rv.withTrimmedLeft (16).withTrimmedTop (2), juce::Justification::topLeft, 6, 1.0f);
    }

private:
    static juce::String hint (eqm::SmartOp op)
    {
        switch (op)
        {
            case eqm::SmartOp::Deresonate:     return juce::String::fromUTF8 ("Encontra ressonâncias estreitas e persistentes e propõe cortes dinâmicos limitados.");
            case eqm::SmartOp::DeEss:          return juce::String::fromUTF8 ("Procura a sibilância na zona escolhida e propõe 1–2 bandas dinâmicas.");
            case eqm::SmartOp::AutoCut:        return juce::String::fromUTF8 ("Propõe cortes de graves/agudos onde só há ruído de fundo. Sub e ar musicais são protegidos.");
            case eqm::SmartOp::MatchReference: return juce::String::fromUTF8 ("Toque este sinal e a referência (no sidechain, ou outra instância) e aprenda.");
            case eqm::SmartOp::Unmask:         return juce::String::fromUTF8 ("Com a voz (ou outro sinal) no sidechain, abre espaço só quando ele toca.");
            case eqm::SmartOp::TonalBalance:   return juce::String::fromUTF8 ("Compara com o equilíbrio médio de misturas e propõe correções largas e suaves (±4 dB).");
            default: return {};
        }
    }

    void rebuildReferences()
    {
        juce::StringArray namesList { "Sidechain" };
        std::vector<int> ids { 0 };
        for (auto& inst : InstanceRegistry::get().all())
            if (inst != proc.shared) { const juce::SpinLock::ScopedLockType lk (inst->lock); namesList.add (juce::String::fromUTF8 ("Instância: ") + inst->name); ids.push_back (inst->id); }
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
    static juce::String hzText (double f) { return f >= 1000 ? juce::String (f / 1000.0, 1).replace (".", ",") + " kHz" : juce::String ((int) std::round (f)) + " Hz"; }
    ValueBar from { "De", hzText }, to { juce::String::fromUTF8 ("Até"), hzText },
             amount { "Amount", [] (double v) { return juce::String ((int) std::round (v)) + " %"; } };
    juce::TextButton learn { "Aprender" }, apply { "Aplicar" }, revert { "Reverter" };
    IconButton close { "Fechar", [] { juce::Path p; p.startNewSubPath (0.25f, 0.25f); p.lineTo (0.75f, 0.75f); p.startNewSubPath (0.75f, 0.25f); p.lineTo (0.25f, 0.75f); return p; }() };
    juce::Rectangle<int> titleArea, step1, step2, step3, step4, step5, progress, review;
    int tick = 0;
};
