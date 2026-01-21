//* UI & Graphics related stuff *//

#pragma once
#include <JuceHeader.h>

// ==========================================
// UI constants
// ==========================================
namespace SetupUI
{
	const juce::Colour background = juce::Colour (0xff0F0F0F);
	const juce::Colour labelsColor = juce::Colour (0xffB0B0B0);

    constexpr int toggleSize = 22;
}

// ==========================================
// Image based toggle button
// ==========================================
class ElektronToggleButton : public juce::DrawableButton
{
public:
    ElektronToggleButton (const juce::String& name,
                          std::unique_ptr<juce::Drawable> offImage,
                          std::unique_ptr<juce::Drawable> onImage)
        : juce::DrawableButton (name, juce::DrawableButton::ImageStretched),
          offDrawable (std::move (offImage)),
          onDrawable  (std::move (onImage))
    {
        setClickingTogglesState (true);

        setImages (offDrawable.get(),  nullptr, nullptr, nullptr,
                   onDrawable.get(),   nullptr, nullptr, nullptr);
    }

private:
    std::unique_ptr<juce::Drawable> offDrawable;
    std::unique_ptr<juce::Drawable> onDrawable;
};

inline std::unique_ptr<juce::Drawable> loadSvgFromBinary (const void* data, size_t size)
{
    auto xml = juce::XmlDocument::parse (
        juce::String::fromUTF8 ((const char*) data, (int) size));

    return juce::Drawable::createFromSVG (*xml);
}
