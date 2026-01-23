//* UI & Graphics related stuff *//

#pragma once
#include <JuceHeader.h>

// ==========================================
// UI constants
// ==========================================
namespace SetupUI
{
    const juce::Colour background = juce::Colour (0xff222326);
    const juce::Colour labelsColor = juce::Colour (0xffB0B0B0);

    constexpr int toggleSize = 22;

    enum class LedColour
    {
        Red,
        Green,
        Orange,
        Purple
    };
}

inline std::unique_ptr<juce::Drawable> loadSvgFromBinary (const void* data, size_t size)
{
    auto xml = juce::XmlDocument::parse (
        juce::String::fromUTF8 ((const char*) data, (int) size));

    return juce::Drawable::createFromSVG (*xml);
}

inline const void* getOnSvgData (SetupUI::LedColour c)
{
    switch (c)
    {
        case SetupUI::LedColour::Red:    return BinaryData::checkbox_on_red_svg;
        case SetupUI::LedColour::Green:  return BinaryData::checkbox_on_green_svg;
        case SetupUI::LedColour::Orange: return BinaryData::checkbox_on_orange_svg;
        case SetupUI::LedColour::Purple: return BinaryData::checkbox_on_purple_svg;
    }

    return nullptr;
}

inline int getOnSvgSize (SetupUI::LedColour c)
{
    switch (c)
    {
        case SetupUI::LedColour::Red:    return BinaryData::checkbox_on_red_svgSize;
        case SetupUI::LedColour::Green:  return BinaryData::checkbox_on_green_svgSize;
        case SetupUI::LedColour::Orange: return BinaryData::checkbox_on_orange_svgSize;
        case SetupUI::LedColour::Purple: return BinaryData::checkbox_on_purple_svgSize;
    }

    return 0;
}

inline std::unique_ptr<juce::Drawable> loadOffSvg()
{
    return loadSvgFromBinary (BinaryData::checkbox_off_svg,
                              BinaryData::checkbox_off_svgSize);
}

// ==========================================
// Image based toggle button
// ==========================================
class LedToggleButton : public juce::DrawableButton
{
    public:
        LedToggleButton (const juce::String& name,
                         SetupUI::LedColour colour)
            : juce::DrawableButton (name, juce::DrawableButton::ImageStretched),
              offDrawable (loadOffSvg()),
              onDrawable  (loadSvgFromBinary (getOnSvgData (colour),
                                              getOnSvgSize (colour)))
        {
            jassert (offDrawable && onDrawable);

            setClickingTogglesState (true);

            setImages (offDrawable.get(), nullptr, nullptr, nullptr,
                       onDrawable.get(),  nullptr, nullptr, nullptr);
        }

    private:
        std::unique_ptr<juce::Drawable> offDrawable;
        std::unique_ptr<juce::Drawable> onDrawable;
};