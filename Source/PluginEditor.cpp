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
    abar.onScaleKnob = [this] (float v) { graph.setDbRange (v); };
    abar.onScaleAuto = [this] (bool on) { graph.setAutoScale (on); };
    graph.onScaleChanged = [this] { abar.showScale (graph.dbRange(), graph.dbRangeTarget(), graph.isAutoScale()); };
    graph.onScaleChanged();

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
        auto s = std::make_unique<SettingsPanel> (proc, [this] (float sc) { setUiScale (sc); }, [this] (float r) { if (r <= 0) graph.setAutoScale (true); else graph.setDbRange (r); },
                                                     graph.isAutoScale() ? 0.0f : graph.dbRangeTarget());
        s->onLogout = [this] { updateLock(); };
        s->onTutorial = [safe = juce::Component::SafePointer<EQMotionEditor> (this)] { juce::MessageManager::callAsync ([safe] { if (safe != nullptr) safe->showWelcome(); }); };
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
    if (piradex::kit::Welcome::shouldShow ("eqmotion", JucePlugin_VersionString)) showWelcome();   // 1.0.2: 3 primeiras aberturas de cada versao
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
    if (welcome != nullptr) welcome->setBounds (getLocalBounds());
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
    if (lic::License::get().claimOnlineRecheck())                    // licença online: revalida no PCC 1x por dia
        juce::Thread::launch ([] { lic::License::get().revalidateOnline(); });
    if (login.isVisible() == lic::License::get().isUnlocked()) updateLock();
    abar.setZoomed (graph.isZoomed());
}

void EQMotionEditor::mouseDown (const juce::MouseEvent&) { proc.state.markUiTouched(); }

bool EQMotionEditor::keyPressed (const juce::KeyPress& k)
{
    return graph.keyPressed (k);
}

void EQMotionEditor::showWelcome()
{
    using W = piradex::kit::WelcomeItem;
    welcome = std::make_unique<piradex::kit::Welcome> ("EQMotion", pal::accent, JucePlugin_VersionString, std::vector<W> {
        W { "Criar e moldar bandas", "Create and shape bands",
            "Duplo clique no gr\xc3\xa1" "fico cria uma banda; arraste para mover, roda do rato para o Q, duplo clique no n\xc3\xb3 para apagar.",
            "Double-click the graph to create a band; drag to move, mouse wheel for Q, double-click a node to delete it." },
        W { "Bandas din\xc3\xa2micas", "Dynamic bands",
            "Qualquer sino, shelf ou tilt pode ser din\xc3\xa2mico: limiar, range, ratio e detetor da pista ou do sidechain.",
            "Any bell, shelf or tilt can be dynamic: threshold, range, ratio and a track or sidechain detector." },
        W { "Smart Ops e Igualar refer\xc3\xaancia", "Smart Ops and Match reference",
            "Abra Smart Ops, escolha a opera\xc3\xa7\xc3\xa3o (ex.: Igualar refer\xc3\xaancia com o sidechain ou outra inst\xc3\xa2ncia), Aprender, Rever e Aplicar.",
            "Open Smart Ops, pick an operation (e.g. Match reference via sidechain or another instance), Learn, Review and Apply." },
        W { "Painel de inst\xc3\xa2ncias", "Instances panel",
            "Veja todas as inst\xc3\xa2ncias do EQMotion do projeto num s\xc3\xb3 painel, com curvas e espetros sobrepostos.",
            "See every EQMotion instance in the project in one panel, with overlaid curves and spectra." },
        W { "Novo na 1.0.2", "New in 1.0.2",
            "Idioma PT / EN / ZH nas Defini\xc3\xa7\xc3\xb5" "es, ecr\xc3\xa3 de login Piradex unificado e este tutorial nas 3 primeiras aberturas de cada vers\xc3\xa3o.",
            "PT / EN / ZH language in Settings, the unified Piradex login screen and this tutorial on the first 3 opens of each version." } });
    welcome->onClose = [safe = juce::Component::SafePointer<EQMotionEditor> (this)] { if (safe != nullptr) juce::MessageManager::callAsync ([safe] { if (safe != nullptr) safe->welcome.reset(); }); };
    addAndMakeVisible (*welcome);
    welcome->setBounds (getLocalBounds());
    welcome->toFront (true);
}
