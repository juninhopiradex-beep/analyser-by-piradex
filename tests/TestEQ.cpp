// Plugin mínimo usado só nos testes do modo Host: peak EQ +6 dB @ 5 kHz (Q 1) e 64 amostras de atraso.
#include <juce_audio_processors/juce_audio_processors.h>

class TestEQ : public juce::AudioProcessor
{
public:
    TestEQ() : AudioProcessor (BusesProperties().withInput ("In", juce::AudioChannelSet::stereo())
                                                .withOutput ("Out", juce::AudioChannelSet::stereo())) {}
    void prepareToPlay (double sr, int) override
    {
        const double A = std::pow (10.0, 6.0 / 40.0), w = 2 * juce::MathConstants<double>::pi * 5000.0 / sr, al = std::sin (w) / 2.0;
        const double a0 = 1 + al / A;
        b0 = (1 + al * A) / a0; b1 = -2 * std::cos (w) / a0; b2 = (1 - al * A) / a0; a1 = b1; a2 = (1 - al / A) / a0;
        for (auto& z : st) z = {};
        for (auto& d : dl) d.assign (64, 0.0f);
        dp = 0;
        setLatencySamples (64);
    }
    void releaseResources() override {}
    void processBlock (juce::AudioBuffer<float>& b, juce::MidiBuffer&) override
    {
        for (int ch = 0; ch < juce::jmin (2, b.getNumChannels()); ++ch)
        {
            auto* x = b.getWritePointer (ch);
            auto& z = st[(size_t) ch];
            size_t p = dp;
            for (int n = 0; n < b.getNumSamples(); ++n)
            {
                const double y = b0 * x[n] + z[0];
                z[0] = b1 * x[n] - a1 * y + z[1];
                z[1] = b2 * x[n] - a2 * y;
                const float out = dl[(size_t) ch][p];
                dl[(size_t) ch][p] = (float) y;
                x[n] = out;
                p = (p + 1) % 64;
            }
            if (ch == juce::jmin (2, b.getNumChannels()) - 1) dp = p;
        }
    }
    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }
    const juce::String getName() const override { return "TestEQ"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override {}
    void setStateInformation (const void*, int) override {}
private:
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    std::array<std::array<double, 2>, 2> st {};
    std::array<std::vector<float>, 2> dl;
    size_t dp = 0;
};

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new TestEQ(); }
