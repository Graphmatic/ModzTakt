#pragma once
#include <JuceHeader.h>
#include "SyntaktParameterTable.h"

class EnvelopeComponent : public juce::Component
{
public:
    EnvelopeComponent()
    {

        setName("Envelope");

        // Group
        addAndMakeVisible(egGroup);

        egGroup.setText("EG");
        egGroup.setColour(juce::GroupComponent::outlineColourId, juce::Colours::white);
        egGroup.setColour(juce::GroupComponent::textColourId, juce::Colours::white);

        // ---- MIDI note source channel
        noteSourceEgChannelLabel.setText("Note Source", juce::dontSendNotification);
        addAndMakeVisible(noteSourceEgChannelLabel);

        egEnabled = false;

        for (int ch = 1; ch <= 16; ++ch)
            noteSourceEgChannelBox.addItem("Ch " + juce::String(ch), ch);

        noteSourceEgChannelBox.addItem("Off", 17);

        noteSourceEgChannelBox.setSelectedId(17); // default OFF

        addAndMakeVisible(noteSourceEgChannelBox);

        noteSourceEgChannelBox.onChange = [this]()
        {
            const int id = noteSourceEgChannelBox.getSelectedId();
            noteSourceEgChannel.store(id, std::memory_order_relaxed);
            egEnabled.store(id != 17, std::memory_order_relaxed);
        };


        // ---- MIDI Channel ----
        midiChannelLabel.setText("MIDI Channel", juce::dontSendNotification);
        addAndMakeVisible(midiChannelLabel);

        addAndMakeVisible(midiChannelBox);
        for (int ch = 1; ch <= 16; ++ch)
            midiChannelBox.addItem("Ch " + juce::String(ch), ch);
        midiChannelBox.setSelectedId(1);

        midiChannelBox.onChange = [this]()
        {
            egOutChannel = (midiChannelBox.getSelectedId());
        };

        // ---- Destination ----
        destinationLabel.setText("Destination", juce::dontSendNotification);

        addAndMakeVisible(destinationLabel);

        // Populate EG destinations
        addAndMakeVisible(destinationBox);

        populateEgDestinationBox();
        destinationBox.setSelectedItemIndex(15, juce::dontSendNotification);

        egOutParamsId = (destinationBox.getSelectedId() - 1);

        destinationBox.onChange = [this]()
        {
            egOutParamsId = (destinationBox.getSelectedId() - 1);
        };

        // ---- ADSR Labels ----
        attackLabel.setText("Attack", juce::dontSendNotification);
        decayLabel.setText("Decay", juce::dontSendNotification);
        sustainLabel.setText("Sustain", juce::dontSendNotification);
        releaseLabel.setText("Release", juce::dontSendNotification);

        addAndMakeVisible(attackLabel);
        addAndMakeVisible(decayLabel);
        addAndMakeVisible(sustainLabel);
        addAndMakeVisible(releaseLabel);

        // ---- ADSR Sliders ----
        auto setupSlider = [](juce::Slider& s)
        {
            s.setSliderStyle(juce::Slider::LinearHorizontal);
            s.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 20);
        };

        setupSlider(attack);
        setupSlider(decay);
        setupSlider(sustain);
        setupSlider(release);

        addAndMakeVisible(attack);
        addAndMakeVisible(decay);
        addAndMakeVisible(sustain);
        addAndMakeVisible(release);
    }

    void resized() override
    {
        if (getWidth() <= 0 || getHeight() <= 0)
            return;

        auto area = getLocalBounds();
        egGroup.setBounds(area);

        auto content = area.reduced(10, 24);

        constexpr int rowHeight  = 28;
        constexpr int labelWidth = 90;
        constexpr int spacing    = 6;

        auto placeRow = [&](juce::Label& label, juce::Component& comp)
        {
            auto row = content.removeFromTop(rowHeight);
            label.setBounds(row.removeFromLeft(labelWidth));
            row.removeFromLeft(spacing);
            comp.setBounds(row);
            content.removeFromTop(6);
        };

        // ---- routing rows ----
        placeRow(noteSourceEgChannelLabel, noteSourceEgChannelBox);

        content.removeFromTop(10); // visual separation

        // ---- ADSR ----
        placeRow(attackLabel,  attack);
        placeRow(decayLabel,   decay);
        placeRow(sustainLabel, sustain);
        placeRow(releaseLabel, release);


        content.removeFromTop(10); // visual separation

        // ---- routing rows ----
        placeRow(midiChannelLabel, midiChannelBox);
        placeRow(destinationLabel, destinationBox);
    }

    void parentHierarchyChanged() override
    {
        // Forces initial layout once the component is attached & visible
        resized();
    }

    bool isEgEnabled() const noexcept
    {
        return egEnabled;
    }

    int selectedEgOutChannel() const noexcept
    {
        return egOutChannel;
    }

    int selectedEgOutParamsId() const noexcept
    {
        return egOutParamsId;
    }

    bool tick(double& outMidiValue)
    {
        if (!egEnabled)
            return false;

        const double nowMs = juce::Time::getMillisecondCounterHiRes();

        if (!advanceEnvelope(
                eg,
                nowMs,
                attack.getValue(),
                decay.getValue(),
                sustain.getValue(),
                release.getValue()))
            return false;

        const double norm = juce::jlimit(0.0, 1.0, eg.currentValue);
        outMidiValue = norm; //juce::roundToInt(norm * 16383.0);
        return true;
    }

    void noteOn(int ch, int note)
    {
        if (ch == noteSourceEgChannel.load(std::memory_order_relaxed))
        {
            const double now = juce::Time::getMillisecondCounterHiRes();

            eg.stage = EnvelopeState::Stage::Attack;
            eg.stageStartMs = now;
            eg.stageStartValue = eg.currentValue;
            eg.noteHeld = true;
        }     
    }

    void noteOff(int ch, int note)
    {
        if (ch == noteSourceEgChannel.load(std::memory_order_relaxed))
        {
            const double now = juce::Time::getMillisecondCounterHiRes();

            eg.stage = EnvelopeState::Stage::Release;
            eg.stageStartMs = now;
            eg.stageStartValue = eg.currentValue;
            eg.noteHeld = false;
        }
    }

    void noteOff()
    {
        const double now = juce::Time::getMillisecondCounterHiRes();

        eg.stage = EnvelopeState::Stage::Release;
        eg.stageStartMs = now;
        eg.stageStartValue = eg.currentValue;
        eg.noteHeld = false;
    }

private:
    // ==== Helpers =====================================================
    void setupSlider(juce::Slider& slider, juce::Label& label, const juce::String& name)
    {
        addAndMakeVisible(slider);
        addAndMakeVisible(label);

        slider.setSliderStyle(juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 20);

        label.setText(name, juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centredLeft);
        label.attachToComponent(&slider, false); // semantic link only
    }

    //Parameter destinations
    void populateEgDestinationBox()
    {
        destinationBox.clear();

        int itemId = 1;

        for (const auto& p : syntaktParameters) // static or global table
        {
            if (p.egDestination)
            {
                destinationBox.addItem(p.name, itemId);
            }
            ++itemId;
        }

        destinationBox.setSelectedId(15, juce::dontSendNotification); // set a default value
    }
   
   //????
    int getSelectedParameterIndex(const juce::ComboBox& box)
    {
        auto* item = box.getChildComponent(box.getSelectedId());
        if (!item)
            return -1;

        return item->getComponentID().getIntValue();
    }

    // ==== UI ==========================================================
    // ---- Group ----
    juce::GroupComponent egGroup;

    std::atomic<int> noteSourceEgChannel { 17 }; // default OFF
    std::atomic<bool> egEnabled { false };

    int egOutChannel;
    int egOutParamsId;

    // ---- Routing ----
    juce::Label   noteSourceEgChannelLabel;
    juce::ComboBox noteSourceEgChannelBox; // source channel for Note-On listening
    juce::Label   midiChannelLabel;
    juce::ComboBox midiChannelBox;

    // MIDI Input selected in mainComponent
    int currentMidiInput;

    juce::Label   destinationLabel;
    juce::ComboBox destinationBox;

    // ---- ADSR ----
    juce::Label attackLabel;
    juce::Label decayLabel;
    juce::Label sustainLabel;
    juce::Label releaseLabel;

    juce::Slider attack;
    juce::Slider decay;
    juce::Slider sustain;
    juce::Slider release;

    //DEBUG
    #if JUCE_DEBUG
    // Debug: show last Note-On received
    juce::Label noteDebugTitle { {}, "Last Note-On:" };
    juce::Label noteDebugLabel;
    #endif

    std::unique_ptr<juce::MidiInput> noteInput;

    // Destination parameters
    std::vector<const SyntaktParameter*> egDestinations;

    std::unique_ptr<juce::MidiInputCallback> noteInputCallback;

    //EG state
 
    struct EnvelopeState
    {
        enum class Stage
        {
            Idle,
            Attack,
            Decay,
            Sustain,
            Release
        };

        Stage stage = Stage::Idle;

        double currentValue = 0.0;     // 0..1
        double stageStartMs = 0.0;
        double stageStartValue = 0.0;

        bool noteHeld = false;
    };

    EnvelopeState eg;

    struct EgRoute
    {
        int midiChannel = 1;
        int parameterIndex = -1; // index into syntaktParameters
    };

    //EG tick function
    bool advanceEnvelope(
                        EnvelopeState& eg,
                        double nowMs,
                        double attackMs,
                        double decayMs,
                        double sustainLevel,  // 0..1
                        double releaseMs)
    {
        constexpr double epsilon = 1e-6;

        auto elapsed = nowMs - eg.stageStartMs;

        switch (eg.stage)
        {
            case EnvelopeState::Stage::Idle:
                eg.currentValue = 0.0;
                return false;

            case EnvelopeState::Stage::Attack:
            {
                if (attackMs <= epsilon)
                {
                    eg.currentValue = 1.0;
                }
                else
                {
                    double t = juce::jlimit(0.0, 1.0, elapsed / attackMs);
                    eg.currentValue =
                        eg.stageStartValue + (1.0 - eg.stageStartValue) * t;
                }

                if (eg.currentValue >= 0.999)
                {
                    eg.stage = EnvelopeState::Stage::Decay;
                    eg.stageStartMs = nowMs;
                    eg.stageStartValue = 1.0;
                }
                return true;
            }

            case EnvelopeState::Stage::Decay:
            {
                if (decayMs <= epsilon)
                {
                    eg.currentValue = sustainLevel;
                }
                else
                {
                    double t = juce::jlimit(0.0, 1.0, elapsed / decayMs);
                    eg.currentValue =
                        eg.stageStartValue +
                        (sustainLevel - eg.stageStartValue) * t;
                }

                if (elapsed >= decayMs)
                {
                    eg.stage = EnvelopeState::Stage::Sustain;
                    eg.stageStartValue = eg.currentValue;
                }
                return true;
            }

            case EnvelopeState::Stage::Sustain:
            {
                eg.currentValue = sustainLevel;

                if (!eg.noteHeld)
                {
                    eg.stage = EnvelopeState::Stage::Release;
                    eg.stageStartMs = nowMs;
                    eg.stageStartValue = eg.currentValue;
                }
                return true;
            }

            case EnvelopeState::Stage::Release:
            {
                if (releaseMs <= epsilon)
                {
                    eg.currentValue = 0.0;
                }
                else
                {
                    double t = juce::jlimit(0.0, 1.0, elapsed / releaseMs);
                    eg.currentValue =
                        eg.stageStartValue * (1.0 - t);
                }

                if (eg.currentValue <= 0.0001)
                {
                    eg.currentValue = 0.0;
                    eg.stage = EnvelopeState::Stage::Idle;
                }
                return true;
            }
        }

        return false;
    }

};
