#pragma once
#include <JuceHeader.h>

class LfoScopeComponent : public juce::Component,
                          private juce::Timer
{
public:
    LfoScopeComponent()
    {
        buffer.fill(0.0f);
        startTimerHz(30); // repaint rate
    }

    // Push a new LFO sample (-1..+1 expected)
    void pushSample(float v)
    {
        v = juce::jlimit(-1.0f, 1.0f, v);
        buffer[writeIndex] = v;
        writeIndex = (writeIndex + 1) % bufferSize;
    }

    void paint(juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        
        auto centre = r.getCentre();
        const float radius = juce::jmin(r.getWidth(), r.getHeight()) * 0.5f - 2.0f;

        // Background
        g.setColour(juce::Colours::darkgrey);
        g.fillEllipse(r);

        // Circular clip
        juce::Path clipPath;
        clipPath.addEllipse(r);

        g.saveState();                  // ⬅ important
        g.reduceClipRegion(clipPath);   // ⬅ clip to circle

        // CRT glow
        g.setColour(juce::Colour(0xff003300));
        g.drawEllipse(r, 2.0f);

        // Waveform
        juce::Path p;
        const int N = bufferSize;

        for (int i = 0; i < N; ++i)
        {
            const int idx = (writeIndex + i) % N;
            const float xNorm = (float)i / (float)(N - 1);
            const float yNorm = buffer[idx];

            const float x = centre.x + (xNorm - 0.5f) * (radius - 8.0f) * 2.0f;
            const float y = centre.y - yNorm * (radius - 8.0f);

            if (i == 0)
                p.startNewSubPath(x, y);
            else
                p.lineTo(x, y);
        }

        // glow pass
        g.setColour(juce::Colour(0xff00ff66).withAlpha(0.2f));
        g.strokePath(p, juce::PathStrokeType(3.5f));

        // core beam
        g.setColour(juce::Colour(0xff33ff99));
        g.strokePath(p, juce::PathStrokeType(1.5f));

        g.restoreState();               // ⬅ restore unclipped state
    }

    void resized() override {}

private:
    void timerCallback() override
    {
        repaint();
    }

    static constexpr int bufferSize = 128;
    std::array<float, bufferSize> buffer;
    int writeIndex = 0;
};
