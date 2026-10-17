#include "PluginEditor.h"

EQMotionEditor::EQMotionEditor (EQMotionProcessor& p)
    : AudioProcessorEditor (&p), proc (p), top (p), abar (p), graph (p), strip (p), inspector (p), master (p), smartPanel (p), touch (p.state)
{
    setLookAndFeel (&lnf);
    juce::LookAndFeel::setDefaultLookAndFeel (&lnf);
    piradex::i18n::reload();          // outro plugin Piradex pode ter mudado o idioma

    // preferências do analisador guardadas na sessão
    auto& ui = proc.uiState;
    auto& az = proc.analyzer;
    az.fftSize.store ((int) ui.getProperty ("fft", 8192));
    az.speed.store ((int) ui.getProperty ("speed", 1));
    az.slope.store ((float) (double) ui.getProperty ("slope", 4.5));

    addAndMakeVisible (top);
    addAndMakeVisible (abar);
    addAndMakeVisible (graph);
    addAndMakeVisible (strip);
    addAndMakeVisible (inspector);
    addAndMakeVisible (master);
    addChildComponent (smartPanel);
    addChildComponent (login);

    graph.onSelectionChanged = [this] (int b) { strip.setSelected (b); inspector.setBand (b); };
    strip.onSelect = [this] (int b) { select (b); };
    strip.onContextMenu = [this] (int b) { select (b); graph.showBandMenu (b); };
    strip.onAdd = [this]
    {
        // nova banda numa zona livre, a meio do espetro
        const double f = 200.0 * std::pow (2.0, (proc.state.activeBandCount() % 7));
        graph.addBandAt (f, 0.0);
    };
    inspector.onBandRemoved = [this] { select (-1); };
    abar.onResetZoom = [this] { graph.resetZoom(); };

    top.onSmartToggle = [this] { openSmartOps (! smartOpen); };
    smartPanel.onClose = [this] { openSmartOps (false); };
    top.onPresetBrowser = [this] (juce::Component& anchor)
    {
        auto b = std::make_unique<PresetBrowser> (proc);
        b->onLoaded = [this] { select (-1); };
        showCallout (std::move (b), anchor);
    };
    top.onInstances = [this] (juce::Component& anchor) { showCallout (std::make_unique<InstancesPanel> (proc), anchor); };
    top.onSettings = [this] (juce::Component& anchor)
    {
        auto s = std::make_unique<SettingsPanel> (proc, [this] (float sc) { setUiScale (sc); }, [this] (float r) { graph.setDbRange (r); }, graph.dbRange());
        s->onLogout = [this] { updateLock(); };
        showCallout (std::move (s), anchor);
    };
    proc.onSmartChanged = [safe = juce::Component::SafePointer<EQMotionEditor> (this)]
    {
        if (safe != nullptr) { safe->smartPanel.refresh(); safe->graph.repaint(); }
    };
    proc.state.onChange = [safe = juce::Component::SafePointer<EQMotionEditor> (this)]
    {
        if (safe != nullptr) safe->repaint();
    };
    login.onUnlocked = [this] { updateLock(); };

    addMouseListener (&touch, true);
    InstanceRegistry::get().editorOpened();

    const int sel = (int) ui.getProperty ("selected", -1);
    if (sel >= 0 && proc.state.band (sel).active) graph.selectBand (sel);

    setResizable (true, true);
    setResizeLimits (1100, 680, 2700, 1700);
    const float sc = (float) (double) ui.getProperty ("scale", 1.0);
    setSize ((int) ui.getProperty ("w", juce::roundToInt (kBaseW * sc)), (int) ui.getProperty ("h", juce::roundToInt (kBaseH * sc)));
    if ((bool) ui.getProperty ("smartOpen", false)) openSmartOps (true);
    updateLock();
    startTimerHz (15);
}

EQMotionEditor::~EQMotionEditor()
{
    stopTimer();
    proc.onSmartChanged = nullptr;
    proc.state.onChange = nullptr;
    removeMouseListener (&touch);
    InstanceRegistry::get().editorClosed();
    juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
}

void EQMotionEditor::paint (juce::Graphics& g) { g.fillAll (pal::bg0); }

void EQMotionEditor::resized()
{
    auto r = getLocalBounds();
    top.setBounds (r.removeFromTop (52));
    master.setBounds (r.removeFromRight (152));
    abar.setBounds (r.removeFromTop (32));
    inspector.setBounds (r.removeFromBottom (172));
    strip.setBounds (r.removeFromBottom (38));
    if (smartOpen) smartPanel.setBounds (r.removeFromLeft (304));
    graph.setBounds (r);
    login.setBounds (getLocalBounds());
    proc.uiState.setProperty ("w", getWidth(), nullptr);
    proc.uiState.setProperty ("h", getHeight(), nullptr);
}

void EQMotionEditor::openSmartOps (bool open)
{
    smartOpen = open;
    smartPanel.setVisible (open);
    graph.setSmartPanelOpen (open);
    top.setSmartOpen (open);
    proc.uiState.setProperty ("smartOpen", open, nullptr);
    if (open) smartPanel.refresh();
    resized();
}

void EQMotionEditor::select (int band)
{
    graph.selectBand (band);
    graph.grabKeyboardFocus();
}

void EQMotionEditor::updateLock()
{
    const bool locked = ! lic::License::get().isUnlocked();
    login.setVisible (locked);
    if (locked) login.toFront (true);
}

void EQMotionEditor::setUiScale (float s)
{
    proc.uiState.setProperty ("scale", s, nullptr);
    setSize (juce::roundToInt (kBaseW * s), juce::roundToInt (kBaseH * s));
}

void EQMotionEditor::showCallout (std::unique_ptr<juce::Component> c, juce::Component& anchor)
{
    juce::CallOutBox::launchAsynchronously (std::move (c), getLocalArea (&anchor, anchor.getLocalBounds()), this);
}

void EQMotionEditor::timerCallback()
{
    lic::License::get().checkExpiry();
    if (login.isVisible() == lic::License::get().isUnlocked()) updateLock();
    abar.setZoomed (graph.isZoomed());
}

void EQMotionEditor::mouseDown (const juce::MouseEvent&) { proc.state.markUiTouched(); }

bool EQMotionEditor::keyPressed (const juce::KeyPress& k)
{
    return graph.keyPressed (k);
}
