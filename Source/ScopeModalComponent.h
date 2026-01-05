#pragma once
#include <JuceHeader.h>


template <size_t N>

class ScopeModalComponent : public juce::Component,
                            private juce::Timer
{
public:

    using LFOValuesArray = std::array<std::atomic<float>, N>;
    using RoutesEnabledArray = std::array<bool, N>;

    ScopeModalComponent(LFOValuesArray& lfoValuesRefs, RoutesEnabledArray& lfoRoutesEnabled)
        : lfoValues(lfoValuesRefs), lfoRoutesEnabled(lfoRoutesEnabled)
    {
        for (size_t i = 0; i < N; ++i)
        {
            addAndMakeVisible(routeButtons[i]);

            routeButtons[i].setToggleState(lfoRoutesEnabled[i],
                                           juce::dontSendNotification);

            routeButtons[i].onClick = [this, i]
            {
                this->lfoRoutesEnabled[i] =
                    routeButtons[i].getToggleState();
            };
        }

        startTimerHz(60);
    }

    void resized()
    {
        auto area = getLocalBounds();

        auto controlArea = area.removeFromBottom(40);

        for (auto& b : routeButtons)
        {
            b.setBounds(controlArea.removeFromLeft(40).reduced(4));
        }

    }

    void paint(juce::Graphics& g) override
    {

        for (size_t i = 0; i < lfoValues.size(); ++i)
        {
            if (!lfoRoutesEnabled[i])
                continue;

            auto r = getLocalBounds().toFloat();
            auto centre = r.getCentre();
            const float radius = juce::jmin(r.getWidth(), r.getHeight()) * 0.5f - 2.0f;

            // Background and CRT glow (once)
            g.setColour(juce::Colours::darkgrey);
            g.fillEllipse(r);
            g.saveState();
            {
                juce::Path clipPath;
                clipPath.addEllipse(r);
                g.reduceClipRegion(clipPath);

                g.setColour(juce::Colour(0xff003300));
                g.drawEllipse(r, 2.0f);

                // Draw each active LFO waveform
                for (size_t i = 0; i < lfoValues.size(); ++i)
                {
                    if (!lfoRoutesEnabled[i])
                        continue;

                    juce::Path p;
                    for (int j = 0; j < bufferSize; ++j)
                    {
                        const int idx = (writeIndices[i] + j) % bufferSize;
                        const float xNorm = float(j) / (bufferSize - 1);
                        const float yNorm = buffers[i][idx];

                        const float x = centre.x + (xNorm - 0.5f) * (radius - 8.0f) * 2.0f;
                        const float y = centre.y - yNorm * (radius - 8.0f);

                        if (j == 0)
                            p.startNewSubPath(x, y);
                        else
                            p.lineTo(x, y);
                    }

                    // glow pass
                    g.setColour(juce::Colour::fromHSV(i / float(N), 0.8f, 0.9f, 0.2f));
                    g.strokePath(p, juce::PathStrokeType(3.5f));

                    // core beam
                    g.setColour(juce::Colour::fromHSV(i / float(N), 0.8f, 0.9f, 1.0f));
                    g.strokePath(p, juce::PathStrokeType(1.5f));
                }
            }
            g.restoreState(); // ⬅ restore unclipped state
        }
    }

private:
    void timerCallback() override
    {
        for (size_t i = 0; i < lfoValues.size(); ++i)
        {
            if (lfoRoutesEnabled[i])
                pushSample(lfoValues[i].load(std::memory_order_relaxed), i);
        }
        repaint();
    }

    // Push a new LFO sample (-1..+1 expected)
    void pushSample(float v, size_t lfoIndex)
    {
        v = juce::jlimit(-1.0f, 1.0f, v);
        buffers[lfoIndex][writeIndices[lfoIndex]] = v;
        writeIndices[lfoIndex] = (writeIndices[lfoIndex] + 1) % bufferSize;
    }

    LFOValuesArray& lfoValues;
    RoutesEnabledArray& lfoRoutesEnabled;

    static constexpr int bufferSize = 128;

    std::array<std::array<float, bufferSize>, N> buffers;
    std::array<int, N> writeIndices = {};

    // toggles to display routes
    std::array<juce::ToggleButton, N> routeButtons;


};




