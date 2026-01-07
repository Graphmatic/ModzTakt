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
        midiChannelLabel.setText("Dest. Channel", juce::dontSendNotification);
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
        destinationLabel.setText("Dest. CC", juce::dontSendNotification);

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

        setupSlider(attackSlider, attackLabel, "Attack");

        constexpr int attackModeGroupId = 1001;

        attackFast.setRadioGroupId(attackModeGroupId);
        attackLong.setRadioGroupId(attackModeGroupId);
        attackSnap.setRadioGroupId(attackModeGroupId);

        attackFast.setToggleState(true, juce::dontSendNotification);

        auto updateAttackMode = [this]()
        {
            if (attackFast.getToggleState())
                attackMode = AttackMode::Fast;
            else if (attackLong.getToggleState())
                attackMode = AttackMode::Long;
            else if (attackSnap.getToggleState())
                attackMode = AttackMode::Snap;
        };

        addAndMakeVisible(attackFast);
        addAndMakeVisible(attackSnap);
        addAndMakeVisible(attackLong);

        attackFast.onClick = updateAttackMode;
        attackLong.onClick = updateAttackMode;
        attackSnap.onClick = updateAttackMode;

        setupSlider(decaySlider, decayLabel, "Decay");

        constexpr int decayCurveGroupId = 2001;

        decayLinear.setRadioGroupId(decayCurveGroupId);
        decayExpo.setRadioGroupId(decayCurveGroupId);
        decayLog.setRadioGroupId(decayCurveGroupId);

        decayExpo.setToggleState(true, juce::dontSendNotification);

        auto updateDecayCurve = [this]()
        {
            if (decayLinear.getToggleState())      decayCurveMode = CurveShape::Linear;
            else if (decayExpo.getToggleState())   decayCurveMode = CurveShape::Exponential;
            else if (decayLog.getToggleState())    decayCurveMode = CurveShape::Logarithmic;
        };

        decayLinear.onClick = updateDecayCurve;
        decayExpo.onClick   = updateDecayCurve;
        decayLog.onClick    = updateDecayCurve;

        addAndMakeVisible(decayLinear);
        addAndMakeVisible(decayExpo);
        addAndMakeVisible(decayLog);

        setupSlider(sustainSlider, sustainLabel, "Sustain");

        setupSlider(releaseSlider, releaseLabel, "Release");

        constexpr int releaseCurveGroupId = 2002;

        releaseLinear.setRadioGroupId(releaseCurveGroupId);
        releaseExpo.setRadioGroupId(releaseCurveGroupId);
        releaseLog.setRadioGroupId(releaseCurveGroupId);

        releaseExpo.setToggleState(true, juce::dontSendNotification);

        auto updateReleaseCurve = [this]()
        {
            if (releaseLinear.getToggleState())      releaseCurveMode = CurveShape::Linear;
            else if (releaseExpo.getToggleState())   releaseCurveMode = CurveShape::Exponential;
            else if (releaseLog.getToggleState())    releaseCurveMode = CurveShape::Logarithmic;
        };

        releaseLinear.onClick = updateReleaseCurve;
        releaseExpo.onClick   = updateReleaseCurve;
        releaseLog.onClick    = updateReleaseCurve;

        addAndMakeVisible(releaseLinear);
        addAndMakeVisible(releaseExpo);
        addAndMakeVisible(releaseLog);

        addAndMakeVisible(releaseLong);

        releaseLong.onClick = [this]()
        {
            releaseLongMode = releaseLong.getToggleState();
        };

    }

    void resized() override
    {
        if (getWidth() <= 0 || getHeight() <= 0)
            return;

        auto area = getLocalBounds();
        egGroup.setBounds(area);

        auto content = area.reduced(10, 24);

        constexpr int rowHeight  = 24;
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
        placeRow(attackLabel, attackSlider);

        auto attackOptionsRow = content.removeFromTop(rowHeight + 4);

        juce::FlexBox attackOptions;
        attackOptions.flexDirection = juce::FlexBox::Direction::row;
        attackOptions.alignItems    = juce::FlexBox::AlignItems::flexStart;
        attackOptions.justifyContent= juce::FlexBox::JustifyContent::flexStart;

        attackOptions.items.add(juce::FlexItem(attackSnap)
                                                .withWidth(60)
                                                .withHeight(rowHeight)
                                                .withMargin({ 0, 8, 0, 0 }));
        attackOptions.items.add(juce::FlexItem(attackFast)
                                                .withWidth(60)
                                                .withHeight(rowHeight)
                                                .withMargin({ 0, 8, 0, 0 }));
        attackOptions.items.add(juce::FlexItem(attackLong)
                                                .withWidth(60)
                                                .withHeight(rowHeight)
                                                .withMargin({ 0, 8, 0, 0 }));

        attackOptions.performLayout(attackOptionsRow);

        content.removeFromTop(10);

        placeRow(decayLabel,   decaySlider);

        auto decayCurveRow = content.removeFromTop(rowHeight + 4);

        juce::FlexBox decayCurveBox;
        decayCurveBox.flexDirection  = juce::FlexBox::Direction::row;
        decayCurveBox.alignItems     = juce::FlexBox::AlignItems::flexStart;
        decayCurveBox.justifyContent = juce::FlexBox::JustifyContent::flexStart;

        decayCurveBox.items.add(juce::FlexItem(decayLinear)
                                                .withWidth(60)
                                                .withHeight(rowHeight)
                                                .withMargin({ 0, 8, 0, 0 }));

        decayCurveBox.items.add(juce::FlexItem(decayExpo)
                                                .withWidth(60)
                                                .withHeight(rowHeight)
                                                .withMargin({ 0, 8, 0, 0 }));

        decayCurveBox.items.add(juce::FlexItem(decayLog)
                                                .withWidth(60)
                                                .withHeight(rowHeight)
                                                .withMargin({ 0, 8, 0, 0 }));

        decayCurveBox.performLayout(decayCurveRow);

        content.removeFromTop(10);

        placeRow(sustainLabel, sustainSlider);

        content.removeFromTop(10);

        placeRow(releaseLabel, releaseSlider);

        auto releaseCurveRow = content.removeFromTop(rowHeight + 4);

        juce::FlexBox releaseCurveBox;
        releaseCurveBox.flexDirection  = juce::FlexBox::Direction::row;
        releaseCurveBox.alignItems     = juce::FlexBox::AlignItems::flexStart;
        releaseCurveBox.justifyContent = juce::FlexBox::JustifyContent::flexStart;

        releaseCurveBox.items.add(juce::FlexItem(releaseLinear)
                                                    .withWidth(60)
                                                    .withHeight(rowHeight)
                                                    .withMargin({ 0, 8, 0, 0 }));

        releaseCurveBox.items.add(juce::FlexItem(releaseExpo)
                                                    .withWidth(60)
                                                    .withHeight(rowHeight)
                                                    .withMargin({ 0, 8, 0, 0 }));

        releaseCurveBox.items.add(juce::FlexItem(releaseLog)
                                                    .withWidth(60)
                                                    .withHeight(rowHeight)
                                                    .withMargin({ 0, 8, 0, 0 }));

        releaseCurveBox.items.add(juce::FlexItem(releaseLong)
                                                    .withWidth(70)
                                                    .withHeight(rowHeight)
                                                    .withMargin({ 0, 8, 0, 0 }));

        releaseCurveBox.performLayout(releaseCurveRow);

        content.removeFromTop(20);

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
                attackMsFromSlider(attackSlider.getValue()),
                sliderToMs(decaySlider.getValue()),
                sustainSlider.getValue(),
                releaseSliderToMs(releaseSlider.getValue())
                )
            )
            return false;

        outMidiValue = juce::jlimit(0.0, 1.0, eg.currentValue);

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

private:
   
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

    juce::Slider attackSlider;
    juce::Slider decaySlider;
    juce::Slider sustainSlider;
    juce::Slider releaseSlider;

    enum class AttackMode
    {
        Fast,
        Long,
        Snap
    };

    AttackMode attackMode = AttackMode::Fast;

    juce::ToggleButton attackFast  { "Fast" };
    juce::ToggleButton attackLong  { "Long" };
    juce::ToggleButton attackSnap  { "Snap" };

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

    // EG curves
    enum class CurveShape
    {
        Linear,
        Exponential,
        Logarithmic
    };

    CurveShape decayCurveMode   = CurveShape::Exponential; // sensible default
    CurveShape releaseCurveMode = CurveShape::Exponential;

    // ===== UI buttons =====
    juce::ToggleButton decayLinear   { "Lin" };
    juce::ToggleButton decayExpo     { "Exp" };
    juce::ToggleButton decayLog      { "Log" };

    juce::ToggleButton releaseLinear { "Lin" };
    juce::ToggleButton releaseExpo   { "Exp" };
    juce::ToggleButton releaseLog    { "Log" };

    juce::ToggleButton releaseLong { "Long" };
    bool releaseLongMode = false;

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

                    if (attackMode == AttackMode::Snap)
                    {
                        constexpr double snapAmount = 6.0; // tweakable: 3.0 = soft / 6.0 = 808 / 10.0 = aggressive / >12.0 = click risk
                        t = 1.0 - std::exp(-snapAmount * t);
                    }

                    eg.currentValue = eg.stageStartValue + (1.0 - eg.stageStartValue) * t;
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
                    const double t = juce::jlimit(0.0, 1.0, elapsed / decayMs);

                    constexpr double kDecay = 8.0; // tweak freely
                    const double shapedT =
                        shapeCurve(t, decayCurveMode, kDecay);

                    eg.currentValue =
                        eg.stageStartValue +
                        (sustainLevel - eg.stageStartValue) * shapedT;
                }

                if (elapsed >= decayMs)
                {
                    eg.currentValue = sustainLevel;
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
                    const double t = juce::jlimit(0.0, 1.0, elapsed / releaseMs);

                    constexpr double kRelease = 12.0; // tweak this one a lot
                    const double shapedT =
                        shapeCurve(t, releaseCurveMode, kRelease);

                    eg.currentValue =
                        eg.stageStartValue * (1.0 - shapedT);
                }

                if (eg.currentValue <= 0.0001 || elapsed >= releaseMs)
                {
                    eg.currentValue = 0.0;
                    eg.stage = EnvelopeState::Stage::Idle;
                }

                return true;
            }
        }

        return false;
    }

    inline double shapeCurve(double t, CurveShape mode, double k)
    {
        // t is assumed in [0, 1]
        switch (mode)
        {
            case CurveShape::Linear:
                return t;

            case CurveShape::Exponential:
                // Fast start, slow tail
                // k ≈ 3..8  (musical range)
                return 1.0 - std::exp(-k * t);

            case CurveShape::Logarithmic:
                // Slow start, fast end
                // k ≈ 3..8
                return std::log1p(k * t) / std::log1p(k);
        }

        return t;
    }

    double attackMsFromSlider(double slider) const
    {
        double norm = slider / 10.0; // 0..1

        switch (attackMode)
        {
            case AttackMode::Fast:
            {
                constexpr double minMs = 0.05;
                constexpr double maxMs = 1000.0;
                return minMs * std::pow(maxMs / minMs, norm);
            }

            case AttackMode::Long:
            {
                constexpr double minMs = 5.0;
                constexpr double maxMs = 30000.0;
                return minMs * std::pow(maxMs / minMs, norm);
            }

            case AttackMode::Snap:
            {
                // time is still fast, but curve will be steeper
                constexpr double minMs = 0.02;
                constexpr double maxMs = 300.0;
                return minMs * std::pow(maxMs / minMs, norm);
            }
        }

        return 10.0;
    }

    double sliderToMs(double v) const
    {
        // v ∈ [0..10]
        return 1.0 * std::pow(10.0, v / 3.33); // ≈ 5 ms → 10 s
    }

    double releaseSliderToMs(double v) const
    {
        double ms = sliderToMs(v);

        if (releaseLongMode)
            ms *= 300.0; // up to 30s

        return ms;
    }

    // ==== Helpers =====================================================
    void setupSlider(juce::Slider& slider, juce::Label& label, const juce::String& name)
    {
        addAndMakeVisible(slider);
        addAndMakeVisible(label);

        slider.setSliderStyle(juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 20);

        label.setText(name, juce::dontSendNotification);
        slider.setNumDecimalPlacesToDisplay(2);

        if ( name == "Sustain")
        {
            slider.setRange(0.0, 1.0, 0.001);
        }
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
};
