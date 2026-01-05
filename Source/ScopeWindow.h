#pragma once
#include <JuceHeader.h>

class ScopeWindow : public juce::DocumentWindow
{
public:
    ScopeWindow(juce::Component* content,
                std::function<void()> onCloseCallback)
        : juce::DocumentWindow("LFO Scope",
                               juce::Colours::black,
                               juce::DocumentWindow::closeButton),
          onClose(std::move(onCloseCallback))
    {
        setUsingNativeTitleBar(false);
        setContentOwned(content, true);
        setResizable(false, false);

        centreWithSize(160, 180);

        setVisible(true);
        setAlwaysOnTop(true);
    }

    void closeButtonPressed() override
    {
        if (onClose)
            onClose();
    }

private:
    std::function<void()> onClose;
};
