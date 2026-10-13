#include "PluginEditor.h"

namespace
{
    void setupKnob (juce::Slider& s, bool big, const juce::String& tip)
    {
        s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        s.setRotaryParameters (juce::degreesToRadians (225.0f), juce::degreesToRadians (495.0f), true);
        s.setMouseDragSensitivity (big ? 320 : 260);
        // Ctrl/Cmd/Alt + arrastar = ajuste fino
        s.setVelocityModeParameters (0.6, 1, 0.05, true, juce::ModifierKeys::ctrlAltCommandModifiers);
        s.getProperties().set ("big", big);
        s.setTooltip (tip);
    }
}

//==============================================================================
MainView::MainView (MixspaceProcessor& p, std::function<void (float)> scaleFn)
    : proc (p), setScale (std::move (scaleFn)), display (p),
      advanced (p, [this] { showAdvanced (false); })
{
    addAndMakeVisible (display);
    addChildComponent (advanced);

    setupKnob (space, true, juce::String::fromUTF8 ("SPACE — quanto o MIXSPACE reage: com que facilidade a colisão vira redução"));
    setupKnob (range, false, juce::String::fromUTF8 ("RANGE — redução máxima em qualquer região (teto em dB)"));
    setupKnob (focus, false, juce::String::fromUTF8 ("FOCUS — 0 % reage a todo o espectro do sidechain, 100 % só às regiões dominantes"));
    spaceAtt = std::make_unique<SA> (proc.apvts, ids::space, space);
    rangeAtt = std::make_unique<SA> (proc.apvts, ids::range, range);
    focusAtt = std::make_unique<SA> (proc.apvts, ids::focus, focus);
    for (auto [s, id] : { std::pair<juce::Slider*, juce::String> { &space, ids::space }, { &range, ids::range }, { &focus, ids::focus } })
    {
        if (auto* prm = proc.apvts.getParameter (id))
            s->setDoubleClickReturnValue (true, prm->convertFrom0to1 (prm->getDefaultValue()));
        addAndMakeVisible (*s);
    }

    deltaButton.setClickingTogglesState (true);
    deltaButton.getProperties().set ("accent", (juce::int64) pal::red.getARGB());
    deltaButton.setTooltip ("DELTA — ouvir apenas o que está a ser retirado do beat");
    deltaAtt = std::make_unique<BA> (proc.apvts, ids::delta, deltaButton);

    bypassButton.setClickingTogglesState (true);
    bypassButton.getProperties().set ("accent", (juce::int64) pal::bypass.getARGB());
    bypassButton.setTooltip ("Bypass sem cliques (mantém a latência)");
    bypassAtt = std::make_unique<BA> (proc.apvts, ids::bypass, bypassButton);

    advancedButton.getProperties().set ("accent", (juce::int64) pal::sc.getARGB());
    advancedButton.setTooltip ("Low/High Cut, Attack/Release, Smooth, estéreo, SC Listen e qualidade");
    advancedButton.onClick = [this] { showAdvanced (! advanced.isVisible()); };

    for (auto* b : { &deltaButton, &bypassButton, &advancedButton })
        addAndMakeVisible (b);

    login.onUnlocked = [this] { updateLock(); };
    addChildComponent (login);
    updateLock();

    setSize (kW, kH);
    startTimerHz (4);
}

void MainView::updateLock()
{
    const bool locked = ! lic::License::get().isUnlocked();
    login.setVisible (locked);
    if (locked) login.toFront (true);
    repaint();
}

void MainView::timerCallback()
{
    repaint (headerRight);
    lic::License::get().checkExpiry();
    if (login.isVisible() == lic::License::get().isUnlocked())
        updateLock();
}

void MainView::showAdvanced (bool show)
{
    advanced.setVisible (show);
    display.setVisible (! show);
    advancedButton.setToggleState (show, juce::dontSendNotification);
}

void MainView::resized()
{
    const juce::Rectangle<int> displayArea (20, 52, 780, 278);
    display.setBounds (displayArea);
    advanced.setBounds (displayArea);

    range.setBounds (juce::Rectangle<int> (108, 108).withCentre ({ 210, 428 }));
    space.setBounds (juce::Rectangle<int> (176, 176).withCentre ({ 410, 452 }));
    focus.setBounds (juce::Rectangle<int> (108, 108).withCentre ({ 610, 428 }));

    deltaButton.setBounds (40, 506, 96, 26);
    bypassButton.setBounds (144, 506, 96, 26);
    advancedButton.setBounds (664, 506, 124, 26);

    login.setBounds (getLocalBounds());
}

void MainView::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    g.setGradientFill (juce::ColourGradient (pal::bgTop, 0.0f, 0.0f, pal::bgBottom, 0.0f, b.getBottom(), false));
    g.fillAll();

    // fio de luz no topo (chassis)
    g.setColour (juce::Colours::white.withAlpha (0.06f));
    g.drawHorizontalLine (0, 0.0f, b.getRight());

    // logótipo
    {
        float x = (float) logoArea.getX() + 6.0f;
        const float cy = (float) logoArea.getCentreY();
        // marca: duas barras (main / sidechain) com o "espaço" âmbar entre elas
        g.setColour (pal::main.withAlpha (0.85f));
        g.fillRoundedRectangle (x, cy - 9.0f, 3.0f, 18.0f, 1.5f);
        g.setColour (pal::red);
        g.fillRoundedRectangle (x + 6.0f, cy - 5.0f, 3.0f, 10.0f, 1.5f);
        g.setColour (pal::sc);
        g.fillRoundedRectangle (x + 12.0f, cy - 9.0f, 3.0f, 18.0f, 1.5f);
        x += 26.0f;

        const auto titleFont = uiFont (21.0f, true, 0.30f);
        g.setFont (titleFont);
        g.setColour (pal::text);
        const float tw = (float) juce::GlyphArrangement::getStringWidthInt (titleFont, "MIXSPACE");
        g.drawText ("MIXSPACE", juce::Rectangle<float> (x, cy - 12.0f, tw + 10.0f, 24.0f), juce::Justification::centredLeft);
        g.setFont (uiFont (12.0f, false, 0.06f));
        g.setColour (pal::textDim);
        g.drawText ("by Piradex", juce::Rectangle<float> (x + tw + 10.0f, cy - 6.0f, 90.0f, 16.0f), juce::Justification::centredLeft);
    }

    // estado à direita: qualidade · latência · sample rate
    {
        const double sr = proc.getSampleRate() > 0 ? proc.getSampleRate() : 48000.0;
        const int q = juce::jlimit (0, 2, (int) proc.apvts.getRawParameterValue (ids::quality)->load());
        const juce::String txt = qualityNames[q].toUpperCase() + juce::String::fromUTF8 ("  ·  ")
                               + juce::String (1000.0 * proc.getCurrentLatency() / sr, 1) + " ms" + juce::String::fromUTF8 ("  ·  ")
                               + juce::String (sr / 1000.0, std::fmod (sr, 1000.0) < 0.5 ? 0 : 1) + " kHz";
        g.setFont (uiFont (11.0f, true, 0.12f));
        g.setColour (pal::textFaint.brighter (0.25f));
        g.drawText (txt, headerRight.toFloat(), juce::Justification::centredRight);
    }

    // placa dos controlos
    const auto plateArea = juce::Rectangle<float> (20.0f, 340.0f, 780.0f, 206.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff1a1d21), 0.0f, plateArea.getY(), pal::plate, 0.0f, plateArea.getBottom(), false));
    g.fillRoundedRectangle (plateArea, 10.0f);
    g.setColour (pal::plateEdge);
    g.drawRoundedRectangle (plateArea.reduced (0.5f), 10.0f, 1.0f);
    g.setColour (juce::Colours::white.withAlpha (0.04f));
    g.drawHorizontalLine ((int) plateArea.getY() + 1, plateArea.getX() + 10.0f, plateArea.getRight() - 10.0f);
    for (auto pt : { juce::Point<float> (34.0f, 354.0f), juce::Point<float> (786.0f, 354.0f) })
    {
        g.setColour (juce::Colour (0xff0d0f12));
        g.fillEllipse (juce::Rectangle<float> (8.0f, 8.0f).withCentre (pt));
        g.setColour (juce::Colour (0xff2e343b));
        g.drawEllipse (juce::Rectangle<float> (8.0f, 8.0f).withCentre (pt), 1.0f);
        g.drawLine (pt.x - 2.5f, pt.y - 2.5f, pt.x + 2.5f, pt.y + 2.5f, 1.0f);
    }

    // etiquetas dos 3 knobs
    auto label = [&] (const juce::String& t, juce::Point<int> c, float size, juce::Colour col)
    {
        g.setColour (col);
        g.setFont (uiFont (size, true, 0.32f));
        g.drawText (t, juce::Rectangle<int> (200, 20).withCentre (c).toFloat(), juce::Justification::centred);
    };
    label ("RANGE", { 210, 362 }, 12.5f, pal::textDim);
    label ("SPACE", { 410, 354 }, 14.0f, pal::text);
    label ("FOCUS", { 610, 362 }, 12.5f, pal::textDim);
    label ("MAX REDUCTION", { 210, 490 }, 9.5f, pal::textFaint.brighter (0.15f));
    label ("SELECTIVITY", { 610, 490 }, 9.5f, pal::textFaint.brighter (0.15f));

    g.setFont (uiFont (9.0f, true, 0.6f));
    g.setColour (pal::textFaint.withAlpha (0.8f));
    g.drawText ("PIRADEX", juce::Rectangle<float> (0.0f, 548.0f, b.getWidth(), 10.0f), juce::Justification::centred);
}

void MainView::mouseDown (const juce::MouseEvent& e)
{
    if (logoArea.contains (e.getPosition()))
        showMenu();
}

void MainView::showMenu()
{
    juce::PopupMenu sizes;
    const float current = proc.getUiScale();
    for (int pct : { 80, 100, 125, 150, 200 })
        sizes.addItem (juce::String (pct) + " %", true, std::abs (current - (float) pct / 100.0f) < 0.01f,
                       [this, pct] { if (setScale) setScale ((float) pct / 100.0f); });

    juce::PopupMenu menu;
    menu.addSectionHeader ("MIXSPACE by Piradex " JucePlugin_VersionString);
    if (lic::License::get().isUnlocked())
        menu.addItem (lic::License::get().statusText(), false, false, nullptr);
    menu.addSubMenu ("Tamanho da interface", sizes);
    menu.addSeparator();
    menu.addItem (juce::String::fromUTF8 ("Terminar sessão neste computador"), lic::License::get().isUnlocked(), false, [this]
    {
        lic::License::get().lock();
        updateLock();
    });
    menu.addItem ("Sobre o MIXSPACE", []
    {
        juce::NativeMessageBox::showAsync (juce::MessageBoxOptions()
            .withIconType (juce::MessageBoxIconType::InfoIcon)
            .withTitle ("MIXSPACE by Piradex")
            .withMessage (juce::String::fromUTF8 ("Versão " JucePlugin_VersionString "\n\n"
                                                  "Spectral sidechain: retira do beat só aquilo que está no caminho da voz.\n\n"
                                                  "www.beatfreakstudio.com\n"
                                                  "Suporte: juninhopiradex@hotmail.com"))
            .withButton ("OK"), nullptr);
    });
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withTargetScreenArea (localAreaToGlobal (logoArea)));
}

//==============================================================================
MixspaceEditor::MixspaceEditor (MixspaceProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      view (p, [this] (float s) { setSize (juce::roundToInt (MainView::kW * s), juce::roundToInt (MainView::kH * s)); })
{
    setLookAndFeel (&lnf);
    proc.setEditorOpen (true);
    addAndMakeVisible (view);

    setResizable (true, true);
    setResizeLimits (MainView::kW * 8 / 10, MainView::kH * 8 / 10, MainView::kW * 2, MainView::kH * 2);
    getConstrainer()->setFixedAspectRatio ((double) MainView::kW / (double) MainView::kH);

    const float s = juce::jlimit (0.8f, 2.0f, proc.getUiScale());
    setSize (juce::roundToInt (MainView::kW * s), juce::roundToInt (MainView::kH * s));
}

MixspaceEditor::~MixspaceEditor()
{
    proc.setEditorOpen (false);
    setLookAndFeel (nullptr);
}

void MixspaceEditor::paint (juce::Graphics& g)
{
    g.fillAll (pal::bgBottom);
}

void MixspaceEditor::resized()
{
    const float s = (float) getWidth() / (float) MainView::kW;
    view.setTransform (juce::AffineTransform::scale (s));
    view.setBounds (0, 0, MainView::kW, MainView::kH);
    proc.setUiScale (s);
}
