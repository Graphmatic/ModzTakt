#pragma once
#include <JuceHeader.h>

class ScopeWindow : public juce::DocumentWindow
{
public:
    ScopeWindow(juce::Component* content,
                             std::function<void()> onClose)
        : juce::DocumentWindow("",
                               juce::Colours::transparentBlack,
                               juce::DocumentWindow::closeButton)
    {
        setUsingNativeTitleBar(false);   // ⬅ remove OS title bar
        setTitleBarHeight(0);            // ⬅ no JUCE title bar
        setResizable(false, false);
        setOpaque(false);                // ⬅ allow transparency
        setDropShadowEnabled(true);

        setContentOwned(content, true);

        centreWithSize(240, 240);
        setVisible(true);
    }

    // make window circular
    void resized()
    {
        DocumentWindow::resized();

        if (auto* peer = getPeer())
        {
            auto area = getLocalBounds().toFloat();
            juce::Path circle;
            circle.addEllipse(area);
        }
    }

    void closeButtonPressed() override
    {
        setVisible(false);
    }


private:
    std::function<void()> onClose;
};
