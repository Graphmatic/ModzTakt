#pragma once
#include <JuceHeader.h>
#include "SyntaktParameterTable.h"
#include "MidiInput.h"
#include "MidiMonitorWindow.h"
#include "EnvelopeComponent.h"

class MainComponent : public juce::Component,
                      private juce::Timer,
                      public MidiClockListener
{
public:
    MainComponent()
    {

        // Initialize MIDI clock listener
        midiClock.setListener(this);

        // frame
        lfoGroup.setText("LFO");
        lfoGroup.setColour(juce::GroupComponent::outlineColourId, juce::Colours::white);
        lfoGroup.setColour(juce::GroupComponent::textColourId, juce::Colours::white);

        addAndMakeVisible(lfoGroup);

        // Envelop Generator
        envelopeComponent = std::make_unique<EnvelopeComponent>();
        addAndMakeVisible(*envelopeComponent);

        // === MIDI Output ===
        midiOutputLabel.setText("MIDI Output:", juce::dontSendNotification);
        addAndMakeVisible(midiOutputLabel);
        addAndMakeVisible(midiOutputBox);

        // === MIDI Input for Clock ===
        midiInputLabel.setText("MIDI Input (Clock):", juce::dontSendNotification);
        addAndMakeVisible(midiInputLabel);
        addAndMakeVisible(midiInputBox);

        refreshMidiInputs();
        refreshMidiOutputs();

        // === Sync Mode ===
        syncModeLabel.setText("Sync Source:", juce::dontSendNotification);
        addAndMakeVisible(syncModeLabel);
        addAndMakeVisible(syncModeBox);
        syncModeBox.addItem("Free", 1);
        syncModeBox.addItem("MIDI Clock", 2);

        // SET UP CALLBACKS BEFORE POPULATING OR SETTING VALUES
        midiOutputBox.onChange = [this] { openSelectedMidiOutput(); };
        midiInputBox.onChange = [this]() { updateMidiInput(); };
        syncModeBox.onChange = [this]() { updateMidiClockState(); };

        // NOW populate the combo boxes (might trigger callbacks if devices exist)
        refreshMidiOutputs();
        refreshMidiInputs();

        // Set default selections AFTER everything is wired up
        syncModeBox.setSelectedId(1); // Free mode by default      

        // Select first available MIDI output if any
        if (midiOutputBox.getNumItems() > 0)
            midiOutputBox.setSelectedId(1);
    
        // Select first available MIDI input if any  
        if (midiInputBox.getNumItems() > 0)
            midiInputBox.setSelectedId(1);

        // === BPM Display ===
        bpmLabelTitle.setText("Detected BPM:", juce::dontSendNotification);
        addAndMakeVisible(bpmLabelTitle);
        bpmLabel.setText("--", juce::dontSendNotification);
        bpmLabel.setColour(juce::Label::textColourId, juce::Colours::aqua);
        addAndMakeVisible(bpmLabel);

        // === Sync Division ===
        divisionLabel.setText("Tempo Divider:", juce::dontSendNotification);
        addAndMakeVisible(divisionLabel);

        divisionBox.addItem("1/1", 1);
        divisionBox.addItem("1/2", 2);
        divisionBox.addItem("1/4", 3);
        divisionBox.addItem("1/8", 4);
        divisionBox.addItem("1/16", 5);
        divisionBox.addItem("1/32", 6);
        divisionBox.addItem("1/8 dotted", 7);
        divisionBox.addItem("1/16 dotted", 8);
        divisionBox.onChange = [this]() { updateLfoRateFromBpm(rateSlider.getValue()); };
        addAndMakeVisible(divisionBox);

        divisionBox.setSelectedId(3); // default quarter note

         // === Shape ===
        shapeLabel.setText("LFO Shape:", juce::dontSendNotification);
        addAndMakeVisible(shapeLabel);
        addAndMakeVisible(shapeBox);
        shapeBox.addItem("Sine", 1);
        shapeBox.addItem("Triangle", 2);
        shapeBox.addItem("Square", 3);
        shapeBox.addItem("Saw", 4);
        shapeBox.addItem("Random", 5);
        shapeBox.setSelectedId(1);

        // === Rate ===
        rateLabel.setText("Rate:", juce::dontSendNotification);
        addAndMakeVisible(rateLabel);
        addAndMakeVisible(rateSlider);
        rateSlider.setRange(0.1, 20.0, 0.01);
        rateSlider.setValue(2.0);
        rateSlider.setTextValueSuffix(" Hz");

        // === Depth ===
        depthLabel.setText("Depth:", juce::dontSendNotification);
        addAndMakeVisible(depthLabel);
        addAndMakeVisible(depthSlider);
        depthSlider.setRange(0.0, 1.0, 0.01);
        depthSlider.setValue(1.0);

          // === Start Button ===
        addAndMakeVisible(startButton);
        startButton.setButtonText("Start LFO");
        startButton.onClick = [this] { toggleLfo(); };


        // === Note-On Restart Controls ===
        addAndMakeVisible(noteRestartToggle);
        noteRestartToggle.setToggleState(false, juce::dontSendNotification);

        addAndMakeVisible(noteSourceChannelBox);
        noteSourceChannelBox.setTextWhenNothingSelected("Source Channel");
        //noteSourceChannelBox.onChange = [this]() { updateNoteSourceChannel(); };
        noteSourceChannelBox.onChange = [this]()
        {
            noteRestartChannel.store(noteSourceChannelBox.getSelectedId(), std::memory_order_release);
        };

        noteRestartToggle.onClick = [this]()
        {
            const bool enabled = noteRestartToggle.getToggleState();

            for (int i = 0; i < maxRoutes; ++i)
            {
                // Hide One-Shot UI if restart is OFF
                routeOneShotToggles[i].setVisible(enabled);

                if (!enabled)
                {
                    // Hard-disable one-shot state
                    routeOneShotToggles[i].setToggleState(false,
                                                           juce::dontSendNotification);

                    lfoRoutes[i].oneShot = false;
                    lfoRoutes[i].hasFinishedOneShot = false;
                }
            }

            // Optional: layout refresh
            juce::MessageManager::callAsync([this]() { resized(); });
        };

        //noteSourceChannelBox.onChange = [this]() { updateNoteOnListener(); };

        // Debug labels
        #if JUCE_DEBUG
        noteDebugTitle.setText("Detected Note-On:", juce::dontSendNotification);
        addAndMakeVisible(noteDebugTitle);
        noteDebugLabel.setText("--", juce::dontSendNotification);
        noteDebugLabel.setColour(juce::Label::textColourId, juce::Colours::aqua);
        addAndMakeVisible(noteDebugLabel);
        #endif

        //***** === Multi-CC Routing (3 routes) ===
        for (int i = 0; i < maxRoutes; ++i)
        {
            // -------- Label --------
            routeLabels[i].setText("Route " + juce::String(i + 1),
                                    juce::dontSendNotification);
            addAndMakeVisible(routeLabels[i]);

            // -------- Channel box --------
            if (i != 0) {
                routeChannelBoxes[i].addItem("Disabled", 1);
            }
            
            for (int ch = 1; ch <= 16; ++ch)
                routeChannelBoxes[i].addItem("Ch " + juce::String(ch), ch + 1);

            addAndMakeVisible(routeChannelBoxes[i]);

            // -------- Parameter box --------
            for (int p = 0; p < juce::numElementsInArray(syntaktParameters); ++p)
                routeParameterBoxes[i].addItem(syntaktParameters[p].name, p + 1);

            addAndMakeVisible(routeParameterBoxes[i]);

            // -------- Bipolar toggle --------
            routeBipolarToggles[i].setButtonText("+/-");
            addAndMakeVisible(routeBipolarToggles[i]);

            // -------- Invert Phase toggle --------
            routeInvertToggles[i].setButtonText("Inv");
            addAndMakeVisible(routeInvertToggles[i]);

            // -------- One Shot toggle --------
            routeOneShotToggles[i].setButtonText("1-Shot");
            addAndMakeVisible(routeOneShotToggles[i]);


            // -------- Set up callbacks BEFORE setting any values --------
            routeChannelBoxes[i].onChange = [this, i]()
            {
                const int comboId = routeChannelBoxes[i].getSelectedId();
                lfoRoutes[i].midiChannel = (comboId == 1) ? 0 : (comboId - 1);
                const bool enabled = (comboId != 1);
                routeParameterBoxes[i].setVisible(enabled);
                routeBipolarToggles[i].setVisible(enabled);
                routeInvertToggles[i].setVisible(enabled);
                routeOneShotToggles[i].setVisible(enabled);
                updateNoteSourceChannel();
                
                // Defer resized() to avoid blocking during ComboBox interaction
                juce::MessageManager::callAsync([this]() { resized(); });
            };

            routeParameterBoxes[i].onChange = [this, i]()
            {
                lfoRoutes[i].parameterIndex =
                    routeParameterBoxes[i].getSelectedId() - 1;

                const bool paramIsBipolar =
                    syntaktParameters[lfoRoutes[i].parameterIndex].isBipolar;

                // Initialize UI + route state ONCE
                routeBipolarToggles[i].setToggleState(paramIsBipolar,
                                                     juce::dontSendNotification);
                lfoRoutes[i].bipolar = paramIsBipolar;
            };


            routeBipolarToggles[i].onClick = [this, i]()
            {
                lfoRoutes[i].bipolar = routeBipolarToggles[i].getToggleState();
                #if JUCE_DEBUG
                updateLfoRouteDebugLabel();
                #endif
            };

            routeOneShotToggles[i].onClick = [this, i]()
            {
                lfoRoutes[i].oneShot = routeOneShotToggles[i].getToggleState();

                if (!lfoRoutes[i].oneShot)
                    lfoRoutes[i].hasFinishedOneShot = false;
            };

            routeInvertToggles[i].onClick = [this, i]()
            {
                lfoRoutes[i].invertPhase = routeInvertToggles[i].getToggleState();
            };



            routeInvertToggles[i].setToggleState(false, juce::dontSendNotification);
            routeOneShotToggles[i].setToggleState(false, juce::dontSendNotification);


            // -------- NOW set initial values (this will trigger callbacks) --------
            if (i == 0)
                routeChannelBoxes[i].setSelectedId(2, juce::dontSendNotification); // Ch 1
            else
                routeChannelBoxes[i].setSelectedId(1, juce::dontSendNotification); // Disabled
                
            routeParameterBoxes[i].setSelectedId(1, juce::dontSendNotification);
            routeBipolarToggles[i].setToggleState(false, juce::dontSendNotification);

            // -------- Initialize route state --------
            lfoRoutes[i].midiChannel = (routeChannelBoxes[i].getSelectedId() == 1)
                                        ? 0
                                        : routeChannelBoxes[i].getSelectedId() - 1;

            lfoRoutes[i].parameterIndex = routeParameterBoxes[i].getSelectedId() - 1;

            lfoRoutes[i].bipolar = routeBipolarToggles[i].getToggleState();

            lfoRoutes[i].invertPhase = false;
            lfoRoutes[i].oneShot     = false;
            lfoRoutes[i].hasFinishedOneShot = false;

            

            // -------- Set initial visibility --------
            const bool enabled = (routeChannelBoxes[i].getSelectedId() != 1);
            routeParameterBoxes[i].setVisible(enabled);
            routeBipolarToggles[i].setVisible(enabled);
            routeInvertToggles[i].setVisible(enabled);
            routeOneShotToggles[i].setVisible(enabled);

        }

        // Initialize note source channel list
        updateNoteSourceChannel();

        // Initialize the atomic variable with current selection
        noteRestartChannel.store(noteSourceChannelBox.getSelectedId(), std::memory_order_release);

        // === Settings Button ===
        addAndMakeVisible(settingsButton);
        settingsButton.setButtonText("Settings"); // or "Opt" for a plainer look
        settingsButton.setTooltip("Open settings menu");
        settingsButton.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
        settingsButton.setColour(juce::TextButton::textColourOffId, juce::Colours::lightgrey);

        settingsButton.onClick = [this]()
        {
            juce::PopupMenu menu;

            juce::PopupMenu throttleSub;
                            throttleSub.addItem(1, "Off (send every change)", true, changeThreshold == 0);
                            throttleSub.addItem(2, "1 step (fine)",           true, changeThreshold == 1);
                            throttleSub.addItem(3, "2 steps",                 true, changeThreshold == 2);
                            throttleSub.addItem(4, "4 steps",                 true, changeThreshold == 4);
                            throttleSub.addItem(5, "8 steps (coarse)",        true, changeThreshold == 8);

            juce::PopupMenu limiterSub;
                            limiterSub.addItem(6, "Off (send every change)", true, msFloofThreshold == 0.0);
                            limiterSub.addItem(7, "0.5ms",                   true, msFloofThreshold == 0.5);
                            limiterSub.addItem(8, "1.0ms",                   true, msFloofThreshold == 1.0);
                            limiterSub.addItem(9, "1.5ms",                   true, msFloofThreshold == 1.5);
                            limiterSub.addItem(10, "2.0ms",                  true, msFloofThreshold == 2.0);
                            limiterSub.addItem(11, "3.0ms",                  true, msFloofThreshold == 3.0);
                            limiterSub.addItem(12, "5.0ms",                  true, msFloofThreshold == 5.0);


            menu.addSectionHeader("Performance");
            menu.addSubMenu("MIDI Data throttle", throttleSub);
            menu.addSubMenu("MIDI Rate limiter", limiterSub);

            menu.addSeparator();
            menu.addItem(99, "zaoum");


            menu.showMenuAsync(juce::PopupMenu::Options(),
                [this](int result)
                {
                    switch (result)
                    {
                        case 1: changeThreshold = 0; break;
                        case 2: changeThreshold = 1; break;
                        case 3: changeThreshold = 2; break;
                        case 4: changeThreshold = 4; break;
                        case 5: changeThreshold = 8; break;
                        case 6: msFloofThreshold = 0.0; break;
                        case 7: msFloofThreshold = 0.5; break;
                        case 8: msFloofThreshold = 1.0; break;
                        case 9: msFloofThreshold = 1.5; break;
                        case 10: msFloofThreshold = 2.0; break;
                        case 11: msFloofThreshold = 3.0; break;
                        case 12: msFloofThreshold = 5.0; break;
                        default: break;
                    }
                });
        };

        //MIDI MONITOR
        #if JUCE_DEBUG
        addAndMakeVisible(midiMonitorButton);

        midiMonitorButton.setToggleable(true);
        midiMonitorButton.setClickingTogglesState(true);

        midiMonitorButton.onClick = [this]()
        {
            if (midiMonitorButton.getToggleState())
            {
                if (midiMonitorWindow == nullptr)
                    midiMonitorWindow = std::make_unique<MidiMonitorWindow>();

                midiMonitorWindow->setVisible(true);
                midiMonitorWindow->toFront(true);
            }
            else
            {
                if (midiMonitorWindow != nullptr)
                    midiMonitorWindow->setVisible(false);
            }
        };

        //bipolar check
        addAndMakeVisible(lfoRouteDebugLabel);
        lfoRouteDebugLabel.setJustificationType(juce::Justification::topLeft);
        lfoRouteDebugLabel.setFont(juce::Font(juce::FontOptions()
                                                    .withHeight(12.0f)
                                            ));

        lfoRouteDebugLabel.setColour(juce::Label::textColourId, juce::Colours::yellow);

        #endif

        // === Timer ===
        startTimerHz(100); // 100Hz refresh

        // start size
        setSize(1000, 600);

    }

    ~MainComponent() override
    {
        stopTimer();
        midiClock.stop();
        midiOut.reset();
    }

    void resized() override
    {
        // prepare column layout
        constexpr int lfoWidth = 450;
        constexpr int egWidth  = 450;
        constexpr int columnSpacing = 12;

        auto area = getLocalBounds().reduced(12);

        // ---- Horizontal split ----
        auto lfoColumn = area.removeFromLeft(lfoWidth);
        area.removeFromLeft(columnSpacing);
        auto egColumn  = area.removeFromLeft(egWidth);

        // LFO block (fixed-width column)
        auto lfoArea = lfoColumn;

        lfoGroup.setBounds(lfoArea);

        auto lfoAreaContent = lfoArea.reduced(10, 20); // space for title

        auto rowHeight = 28;
        auto labelWidth = 150;
        auto spacing = 6;

        auto placeRow = [&](juce::Label& label, juce::Component& comp)
        {
            auto row = lfoAreaContent.removeFromTop(rowHeight);
            label.setBounds(row.removeFromLeft(labelWidth));
            row.removeFromLeft(spacing);
            comp.setBounds(row);
            lfoAreaContent.removeFromTop(6);
        };

        placeRow(midiOutputLabel, midiOutputBox);
        placeRow(midiInputLabel, midiInputBox);
        placeRow(syncModeLabel, syncModeBox);

        // BPM label pair
        auto bpmRow = lfoAreaContent.removeFromTop(rowHeight);
        bpmLabelTitle.setBounds(bpmRow.removeFromLeft(labelWidth));
        bpmRow.removeFromLeft(spacing);
        bpmLabel.setBounds(bpmRow);
        lfoAreaContent.removeFromTop(6);

        placeRow(divisionLabel, divisionBox);

        // Place route selectors
        for (int i = 0; i < maxRoutes; ++i)
        {
            auto rowArea = lfoAreaContent.removeFromTop(rowHeight);

            juce::FlexBox fb;
            fb.flexDirection = juce::FlexBox::Direction::row;
            fb.alignItems = juce::FlexBox::AlignItems::center;

            fb.items.add(
                juce::FlexItem(routeLabels[i])
                    .withWidth(70)
                    .withHeight(rowHeight)
                    .withMargin({ 0, 8, 0, 0 }) // right margin
            );

            fb.items.add(
                juce::FlexItem(routeChannelBoxes[i])
                    .withWidth(90)
                    .withHeight(rowHeight)
                    .withMargin({ 0, 8, 0, 0 }) // right margin
            );

            if (routeParameterBoxes[i].isVisible())
            {
                fb.items.add(
                    juce::FlexItem(routeParameterBoxes[i])
                        .withWidth(200)
                        .withHeight(rowHeight)
                        .withMargin({ 0, 8, 0, 0 }) // right margin
                );
            }

            if (routeBipolarToggles[i].isVisible())
            {
                fb.items.add(
                    juce::FlexItem(routeBipolarToggles[i])
                        .withWidth(40)
                        .withHeight(rowHeight)
                        .withMargin({ 0, 8, 0, 0 }) // right margin
                );
            }

            if (routeInvertToggles[i].isVisible())
            {
                fb.items.add(
                    juce::FlexItem(routeInvertToggles[i])
                        .withWidth(40)
                        .withHeight(rowHeight)
                        .withMargin({ 0, 8, 0, 0 }) // right margin
                );
            }

            if (routeOneShotToggles[i].isVisible())
            {
                fb.items.add(
                    juce::FlexItem(routeOneShotToggles[i])
                        .withWidth(40)
                        .withHeight(rowHeight)
                        .withMargin({ 0, 8, 0, 0 }) // right margin
                );
            }

            fb.performLayout(rowArea);

            lfoAreaContent.removeFromTop(10);
        }


        placeRow(shapeLabel, shapeBox);

        // auto invertRow = lfoAreaContent.removeFromTop(rowHeight);
        // oneShotToggle.setBounds(invertRow.removeFromRight(100));
        // oneShotToggle.setVisible(noteRestartToggle.getToggleState());
        // invertPhaseToggle.setBounds(invertRow.removeFromRight(140));
        lfoAreaContent.removeFromTop(6);

        placeRow(rateLabel, rateSlider);
        placeRow(depthLabel, depthSlider);

        lfoAreaContent.removeFromTop(10);
        startButton.setBounds(lfoAreaContent.removeFromTop(40));

        lfoAreaContent.removeFromTop(10);
        auto placeRowToggle = [&](juce::ToggleButton& toggleButton, juce::ComboBox& combobox)
        {
            auto row = lfoAreaContent.removeFromTop(rowHeight);
            toggleButton.setBounds(row.removeFromLeft(labelWidth));
            row.removeFromLeft(spacing);
            combobox.setBounds(row);
            lfoAreaContent.removeFromTop(6);area.removeFromTop(10);
        };
        placeRowToggle(noteRestartToggle, noteSourceChannelBox);

        #if JUCE_DEBUG
        auto placeDebugRow = [&](juce::Label& title, juce::Label& midiValues)
        {
            auto row = lfoAreaContent.removeFromTop(rowHeight);
            title.setBounds(row.removeFromLeft(labelWidth));
            row.removeFromLeft(spacing);
            midiValues.setBounds(row);
            lfoAreaContent.removeFromTop(6);
        };
        placeDebugRow(noteDebugTitle, noteDebugLabel);
        #endif

        // === setting button ===
        auto bounds = getLocalBounds();
        auto size = 24; // small square button
        settingsButton.setBounds(bounds.removeFromBottom(10 + size)
                                        .removeFromRight(10 + size)
                                        .removeFromLeft(size)
                                        .removeFromTop(size));

        settingsButton.setColour(juce::TextButton::buttonOnColourId, juce::Colours::darkgrey.withAlpha(0.3f));
        settingsButton.setClickingTogglesState(false);


        //MIDI MONITOR
        #if JUCE_DEBUG
        auto areaMon = getLocalBounds();

        constexpr int buttonWidth  = 110;
        constexpr int buttonHeight = 22;
        constexpr int margin       = 8;
        constexpr int marginLeft   = 500;

        midiMonitorButton.setBounds(
            areaMon.getRight() - buttonWidth  - marginLeft,
            areaMon.getBottom() - buttonHeight - margin,
            buttonWidth,
            buttonHeight
        );

        // bipolar check
        lfoRouteDebugLabel.setBounds(10, getHeight() - 120, 300, 100);
        #endif

        // Envelop generator frame
        if (envelopeComponent != nullptr)
            envelopeComponent->setBounds(egColumn);

    }

    void postJuceInit()
    {
        refreshMidiInputs();
        refreshMidiOutputs();
    }



private:
    // === UI Components ===
    juce::GroupComponent lfoGroup;

    juce::Label midiOutputLabel, midiInputLabel, syncModeLabel;
    juce::Label bpmLabelTitle, bpmLabel, divisionLabel;
    juce::Label parameterLabel, shapeLabel, rateLabel, depthLabel, channelLabel;

    juce::ComboBox midiOutputBox, midiInputBox, syncModeBox, divisionBox;
    juce::ComboBox shapeBox; //parameterBox
    juce::Slider rateSlider, depthSlider; //, channelSlider;

    // juce::ToggleButton oneShotToggle { "One-Shot"};
    // juce::ToggleButton invertPhaseToggle { "Invert Phase" };

    //Note-On retrig on/off and source channel
    juce::ToggleButton noteRestartToggle { "Restart on Note-On" };
    juce::ComboBox noteSourceChannelBox; // source channel for Note-On listening

    juce::TextButton startButton;

    // === MIDI ===
    std::unique_ptr<juce::MidiOutput> midiOut;
    MidiClockHandler midiClock;

    std::unique_ptr<juce::MidiInput> globalMidiInput;
    std::unique_ptr<juce::MidiInputCallback> midiCallback;

    std::atomic<bool> pendingNoteOn { false };
    std::atomic<bool> pendingNoteOff { false };
    std::atomic<int>  pendingNoteChannel { 0 };
    std::atomic<int>  pendingNoteNumber { 0 };

    std::atomic<int> noteRestartChannel { 0 }; // 1–16, 0 = disabled

    std::atomic<bool> requestLfoRestart { false };


    //DEBUG
    #if JUCE_DEBUG
    // Debug: show last Note-On received
    juce::Label noteDebugTitle { {}, "Last Note-On:" };
    juce::Label noteDebugLabel;
    #endif

    // === Multi-CC Routing ===
    static constexpr int maxRoutes = 3;
    struct LfoRoute { 
        int midiChannel = 0;
        int parameterIndex = 0;
        bool bipolar = false;
        bool invertPhase = false;  // NEW
        bool oneShot     = false;  // NEW

        bool hasFinishedOneShot = false; // runtime state
    };
    std::array<LfoRoute, maxRoutes> lfoRoutes;
    std::array<juce::Label, maxRoutes> routeLabels;
    std::array<juce::ComboBox, maxRoutes> routeChannelBoxes;
    std::array<juce::ComboBox, maxRoutes> routeParameterBoxes;
    juce::ToggleButton routeBipolarToggles[maxRoutes];
    juce::ToggleButton routeInvertToggles[maxRoutes];   // NEW
    juce::ToggleButton routeOneShotToggles[maxRoutes];  // NEW

    std::array<double, maxRoutes> lfoPhase { 0.0, 0.0, 0.0 };

    #if JUCE_DEBUG
    std::unique_ptr<MidiMonitorWindow> midiMonitorWindow;
    juce::TextButton midiMonitorButton { "MIDI Monitor" };

    //bipolar check
    juce::Label lfoRouteDebugLabel;

    #endif

    // === Setting Pop-Up ===
    juce::TextButton settingsButton;

    // === LFO State ===
    double phase = 0.0;
    double sampleRate = 100.0;
    juce::Random random;
    bool lfoActive = false;
    // bool oneShotMode = false;
    double routePhase[maxRoutes];

    bool oneShotActive = false;

    double oneShotPhaseAccum = 0.0;

    // === BPM smoothing / throttling ===
    double displayedBpm = 0.0;
    juce::int64 lastBpmUpdateMs = 0;

    // EG
    std::unique_ptr<EnvelopeComponent> envelopeComponent;

    // settings - Dithering and MIDI throttle ===
    std::unordered_map<int, int> lastSentValuePerParam;  // key: param ID or CC number
    int changeThreshold = 1; // difference needed before sending

    // settings - Anti flooding
    std::unordered_map<int, double> lastSendTimePerParam;
    double msFloofThreshold = 0.0; // delay between Midi datas chunk

    //MIDI MONITOR
    #if JUCE_DEBUG
    void settingsButtonClicked()
    {
        if (midiMonitorWindow == nullptr)
        {
            midiMonitorWindow = std::make_unique<MidiMonitorWindow>();
        }

        midiMonitorWindow->setVisible(true);
        midiMonitorWindow->toFront(true);
    }
    #endif

    void updateNoteSourceChannel()
    {
        // Store current selection before clearing
        const int currentSelection = noteSourceChannelBox.getSelectedId();
        
        // Clear without triggering onChange
        noteSourceChannelBox.clear(juce::dontSendNotification);

        juce::Array<int> activeChannels;

        // Collect unique active MIDI channels from routes
        for (const auto& route : lfoRoutes)
        {
            if (route.midiChannel > 0 && !activeChannels.contains(route.midiChannel))
                activeChannels.add(route.midiChannel);
        }

        // Populate selector
        for (int ch : activeChannels)
            noteSourceChannelBox.addItem("Ch " + juce::String(ch), ch);

        // Restore previous selection if possible, otherwise select first
        int newSelection = 0;
        if (activeChannels.contains(currentSelection))
        {
            newSelection = currentSelection;
            noteSourceChannelBox.setSelectedId(currentSelection, juce::dontSendNotification);
        }
        else if (!activeChannels.isEmpty())
        {
            newSelection = activeChannels[0];
            noteSourceChannelBox.setSelectedId(activeChannels[0], juce::dontSendNotification);
        }
        
        // IMPORTANT: Update the atomic variable since onChange won't fire with dontSendNotification
        noteRestartChannel.store(newSelection, std::memory_order_release);
    }

    void refreshMidiOutputs()
    {
        midiOutputBox.clear();
        auto devices = juce::MidiOutput::getAvailableDevices();
        for (int i = 0; i < devices.size(); ++i)
            midiOutputBox.addItem(devices[i].name, i + 1);
    }

    void refreshMidiInputs()
    {
        midiInputBox.clear();
        auto devices = juce::MidiInput::getAvailableDevices();
        for (int i = 0; i < devices.size(); ++i)
            midiInputBox.addItem(devices[i].name, i + 1);
    }

    // MIDI Sync
    void handleIncomingMessage(const juce::MidiMessage& msg)
    {
        const bool syncEnabled = (syncModeBox.getSelectedId() == 2);
        if (!syncEnabled)
        {
            midiClock.stop();
            return;
        }

        // existing clock parsing here  midiInputBox
        int inIndex = midiInputBox.getSelectedId() - 1;
        midiClock.start(inIndex);
        return;

    }

    // Call to start/stop the MidiClockHandler based on UI state
    void updateMidiClockState()
    {
        const bool syncEnabled = (syncModeBox.getSelectedId() == 2);
        
        if (syncEnabled)
        {
            int inIndex = midiInputBox.getSelectedId() - 1;
            if (inIndex >= 0)
            {
                midiClock.start(inIndex);
            }
            else
            {
                // No valid input selected, stop the clock
                midiClock.stop();
            }
        }
        else
        {
            // Free mode - ensure the clock is stopped
            midiClock.stop();
        }
    }
    
    struct GlobalMidiCallback : public juce::MidiInputCallback
    {
        MainComponent& owner;
        GlobalMidiCallback(MainComponent& o) : owner(o) {}

        void handleIncomingMidiMessage(juce::MidiInput*, const juce::MidiMessage& msg) override
        {
            // --- MIDI CLOCK ---
            if (owner.syncModeBox.getSelectedId() == 2)
            {
                owner.handleIncomingMessage(msg); 
            }

            if (msg.isNoteOn())
            {
                owner.pendingNoteChannel.store(msg.getChannel(), std::memory_order_relaxed);
                owner.pendingNoteNumber.store(msg.getNoteNumber(), std::memory_order_relaxed);
                owner.pendingNoteOn.store(true, std::memory_order_release);
            }
            else if (msg.isNoteOff())
            {
                owner.pendingNoteChannel.store(msg.getChannel(), std::memory_order_relaxed);
                owner.pendingNoteOff.store(true, std::memory_order_release);
            }
        }
    };

    void updateMidiInput()
    {
        if (globalMidiInput)
        {
            globalMidiInput->stop();
            globalMidiInput.reset();
        }

        auto inputs = juce::MidiInput::getAvailableDevices();
        int index = midiInputBox.getSelectedId() - 1;
        if (index < 0 || index >= inputs.size())
        {
            // Stop the clock if no valid input selected
            midiClock.stop();
            return;
        }

        midiCallback = std::make_unique<GlobalMidiCallback>(*this);
        globalMidiInput = juce::MidiInput::openDevice(inputs[index].identifier,
                                                       midiCallback.get());
        if (globalMidiInput)
            globalMidiInput->start();
    }

    void toggleLfo()
    {
        if (lfoActive)
        {
            // reset LFO value
            for (int i = 0; i < maxRoutes; ++i)
            {
                auto& route = lfoRoutes[i];
                lfoPhase[i] = getWaveformStartPhase(
                    shapeBox.getSelectedId(),
                    lfoRoutes[i].bipolar,
                    lfoRoutes[i].invertPhase
                );

                lfoRoutes[i].hasFinishedOneShot = false;
            }
            
            lfoActive = false;
            startButton.setButtonText("Start LFO");
        }
        else
        {
            // Clock may still be needed for sync
            updateMidiClockState();
            // reset LFO value
            for (int i = 0; i < maxRoutes; ++i)
            {
                auto& route = lfoRoutes[i];
                lfoPhase[i] = getWaveformStartPhase(
                    shapeBox.getSelectedId(),
                    lfoRoutes[i].bipolar,
                    lfoRoutes[i].invertPhase
                );

                lfoRoutes[i].hasFinishedOneShot = false;
            }
            
            lfoActive = true;
            startButton.setButtonText("Stop LFO");
        }
    }

    // === Timer Callback ===
    void timerCallback() override
    {
        if (!midiOut)
            return;

        // Note messages
        if (pendingNoteOn.exchange(false))
        {
            const int ch   = pendingNoteChannel.load();
            const int note = pendingNoteNumber.load();

            // --- EG ---
            if (envelopeComponent && envelopeComponent->isEgEnabled())
                envelopeComponent->noteOn(ch, note);

            // --- LFO Note Restart ---
            const int restartCh = noteRestartChannel.load(std::memory_order_acquire);

            if (noteRestartToggle.getToggleState()
                && restartCh > 0
                && ch == restartCh)
            {
                requestLfoRestart.store(true, std::memory_order_release);
                
                #if JUCE_DEBUG
                noteDebugLabel.setText("NoteOn: Ch " + juce::String(ch) +
                                       " | Note " + juce::String(note),
                                       juce::dontSendNotification);
               #endif
            }
        }

        if (pendingNoteOff.exchange(false))
        {
            if (envelopeComponent && envelopeComponent->isEgEnabled())
                envelopeComponent->noteOff();
        }

        // LFO
        const bool syncEnabled = (syncModeBox.getSelectedId() == 2);
        const double bpm = midiClock.getCurrentBPM();
        int egValue;

        // Handle pending retrigger from Note-On
        if (requestLfoRestart.exchange(false))
        {
            // reset LFO value
            for (int i = 0; i < maxRoutes; ++i)
            {
                auto& route = lfoRoutes[i];
                lfoPhase[i] = getWaveformStartPhase(
                    shapeBox.getSelectedId(),
                    lfoRoutes[i].bipolar,
                    lfoRoutes[i].invertPhase
                );

                lfoRoutes[i].hasFinishedOneShot = false;
            }

            if (!lfoActive)
            {
                lfoActive = true;
                startButton.setButtonText("Stop LFO");
            }

            // if (oneShotMode)
            //     oneShotActive = true;
            //     oneShotPhaseAccum = 0.0;   // reset accumulator
        }
   

        // === Always update BPM display if sync mode is active ===
        if (syncEnabled)
        {
            const double bpm = midiClock.getCurrentBPM();
            const auto nowMs = juce::Time::getMillisecondCounterHiRes();

            if (bpm > 0.0)
            {
                // Smooth & rate-limit UI updates
                displayedBpm = 0.9 * displayedBpm + 0.1 * bpm;
                if (nowMs - lastBpmUpdateMs > 250.0)
                {
                    bpmLabel.setText(juce::String(displayedBpm, 1), juce::dontSendNotification);
                    lastBpmUpdateMs = nowMs;
                }
            }
            else
            {
                // No clock yet: show placeholder
                if (nowMs - lastBpmUpdateMs > 500.0)
                {
                    bpmLabel.setText("--", juce::dontSendNotification);
                    lastBpmUpdateMs = nowMs;
                }
            }
        }
        else
        {
            // When not in sync mode, just freeze BPM display
        }

        if (lfoActive)
        {
            // === Compute current rate ===
            double rateHz = rateSlider.getValue();

            if (syncEnabled && bpm > 0.0)
            {
                rateHz = updateLfoRateFromBpm(rateHz);
            }                

            // // === Generate and send LFO values ===
            
            // Waveform generation
            // --- Per-route waveform + mapping (compute shape per route) ---
            // We keep a single global 'phase' (time), but compute a per-route
            // routePhase so bipolar/unipolar start positions can differ.
            // --- Per-route waveform + mapping (compute shape per route) ---
            for (int i = 0; i < maxRoutes; ++i)
            {
                auto& route = lfoRoutes[i];

                if (route.midiChannel <= 0)
                    continue;

                if (route.parameterIndex < 0)
                    continue;

                // --- One-shot stop check ---
                if (route.oneShot && route.hasFinishedOneShot)
                    continue;

                // --- Phase advance ---
                lfoPhase[i] += rateHz / sampleRate;

                if (lfoPhase[i] >= 1.0)
                {
                    lfoPhase[i] -= 1.0;

                    if (route.oneShot)
                    {
                        route.hasFinishedOneShot = true;
                        continue; // do NOT generate value this tick
                    }
                }

                double phase = lfoPhase[i];

                // --- Waveform ---
                double shape = 0.0;
                switch (shapeBox.getSelectedId())
                {
                    case 1:
                        shape = std::sin(juce::MathConstants<double>::twoPi * phase);
                        break;

                    case 2:
                        shape = 2.0 * std::abs(2.0 * (phase - std::floor(phase + 0.5))) - 1.0;
                        break;

                    case 3:
                        shape = (phase < 0.5) ? 1.0 : -1.0;
                        break;

                    case 4:
                        shape = 2.0 * (phase - std::floor(phase + 0.5));
                        break;

                    case 5:
                        shape = random.nextDouble() * 2.0 - 1.0;
                        break;
                }

                if (route.invertPhase)
                    shape = -shape;

                // --- Mapping ---
                const auto& param = syntaktParameters[route.parameterIndex];
                const double depth = depthSlider.getValue();

                int midiVal = 0;

                if (route.bipolar)
                {
                    const int center = (param.minValue + param.maxValue) / 2;
                    const int range  = (param.maxValue - param.minValue) / 2;

                    midiVal = center
                            + int(std::round(shape * depth * range));
                            midiVal = juce::jlimit(param.minValue, param.maxValue, midiVal);

                sendThrottledParamValue(i,
                                        route.midiChannel,
                                        param,
                                        midiVal);
                }

                else
                {
                    double uni = (shape + 1.0) * 0.5;
                    uni = juce::jlimit(0.0, 1.0, uni);

                    midiVal = param.minValue
                            + int(std::round(uni * depth
                            * (param.maxValue - param.minValue)));
                            midiVal = juce::jlimit(param.minValue, param.maxValue, midiVal);

                sendThrottledParamValue(i,
                                        route.midiChannel,
                                        param,
                                        midiVal);
                }

            }



        }
        
        if (envelopeComponent && envelopeComponent->isEgEnabled())
        {
            double egMIDIvalue = 0.0;
            const int paramId = envelopeComponent->selectedEgOutParamsId();
            const int egValue = mapEgToMidi(envelopeComponent->tick(egMIDIvalue), paramId);
            const int egCh    = envelopeComponent->selectedEgOutChannel();
            

            if (egCh > 0 && paramId >= 0)
            {
                const auto& param = syntaktParameters[paramId];

                sendThrottledParamValue(
                    0x7FFF,        // fixed EG route index// trigg EG
                    egCh,
                    param,
                    egValue);
            }
        }

        #if JUCE_DEBUG
        updateLfoRouteDebugLabel();
        #endif
    }

    // shared throttling and MIDI send function
    void sendThrottledParamValue(
                                int routeIndex,              // for unique throttle key
                                int midiChannel,
                                const SyntaktParameter& param,
                                int midiValue)
    {
        // -------- Build per-route + per-parameter key --------
        const int paramKey =
            (routeIndex << 16) |
            (param.isCC ? 0x1000 : 0x2000) |
            (param.isCC ? param.ccNumber
                        : ((param.nrpnMsb << 7) | param.nrpnLsb));

        // -------- Value change threshold --------
        const int lastVal = lastSentValuePerParam[paramKey];
        if (std::abs(midiValue - lastVal) < changeThreshold)
            return;

        lastSentValuePerParam[paramKey] = midiValue;

        // -------- Time-based anti-flood --------
        const double now = juce::Time::getMillisecondCounterHiRes();
        if (now - lastSendTimePerParam[paramKey] < msFloofThreshold)
            return;

        lastSendTimePerParam[paramKey] = now;

        // -------- Split value if NRPN --------
        const int valueMSB = (midiValue >> 7) & 0x7F;
        const int valueLSB = midiValue & 0x7F;

        const bool monitorInput = false;

        if (param.isCC)
        {
            auto msg = juce::MidiMessage::controllerEvent(
                midiChannel, param.ccNumber, midiValue);

            midiOut->sendMessageNow(msg);

            #if JUCE_DEBUG
            if (midiMonitorWindow)
                midiMonitorWindow->pushEvent(msg, monitorInput);
            #endif
        }
        else
        {
            auto send = [&](int cc, int val)
            {
                auto msg = juce::MidiMessage::controllerEvent(midiChannel, cc, val);
                midiOut->sendMessageNow(msg);

                #if JUCE_DEBUG
                if (midiMonitorWindow)
                    midiMonitorWindow->pushEvent(msg, monitorInput);
                #endif
            };

            send(99, param.nrpnMsb);
            send(98, param.nrpnLsb);
            send(6,  valueMSB);
            send(38, valueLSB);
        }
    }

    // === MIDI Transport Callbacks ===
    void handleMidiStart() override
    {
        // Reset LFO phase when sequencer starts
        for (int i = 0; i < maxRoutes; ++i)
        {
            auto& route = lfoRoutes[i];
            lfoPhase[i] = getWaveformStartPhase(
            shapeBox.getSelectedId(),
            lfoRoutes[i].bipolar,
            lfoRoutes[i].invertPhase
            );

            lfoRoutes[i].hasFinishedOneShot = false;
        }

        lfoActive = false;
        // passing via the UI start/stop function to avoid decoherence between HW and UI
        toggleLfo();
    }

    void handleMidiStop() override
    {
        // Reset LFO to get a clean start even is the LFO is restarted from the UI
        for (int i = 0; i < maxRoutes; ++i)
            {
                auto& route = lfoRoutes[i];
                lfoPhase[i] = getWaveformStartPhase(
                shapeBox.getSelectedId(),
                    lfoRoutes[i].bipolar,
                    lfoRoutes[i].invertPhase
                );

                lfoRoutes[i].hasFinishedOneShot = false;
            }

        lfoActive = true;
        toggleLfo();
    }

    double updateLfoRateFromBpm(double rateHz)
    {
        const double bpm = midiClock.getCurrentBPM();
        const bool syncEnabled = (syncModeBox.getSelectedId() == 2);
        if (syncEnabled && bpm > 0.0)
        {
            rateHz = bpmToHz(bpm);

            rateSlider.setValue(rateHz, juce::dontSendNotification);
        }
            
        return rateHz;
    }

    // === BPM → Frequency Conversion ===
    double bpmToHz(double bpm)
    {
        if (bpm <= 0.0)
            return 0.0;

        // Division multiplier relative to 1 beat = quarter note
        double multiplier = 1.0;

        switch (divisionBox.getSelectedId())
        {
            case 1: multiplier = 0.25; break;  // whole note (4 beats per cycle)
            case 2: multiplier = 0.5;  break;  // half note
            case 3: multiplier = 1.0;  break;  // quarter note
            case 4: multiplier = 2.0;  break;  // eighth
            case 5: multiplier = 4.0;  break;  // sixteenth
            case 6: multiplier = 8.0;  break;  // thirty-second
            case 7: multiplier = 2.0 / 1.5; break;  // dotted ⅛ (triplet-based)
            case 8: multiplier = 4.0 / 1.5; break;  // dotted 1/16
            default: break;
        }

        // base beat frequency = beats per second
        const double beatsPerSecond = bpm / 60.0;

        // final LFO frequency in Hz
        return beatsPerSecond * multiplier;
    }

    //Map EG value to MIDI
    int mapEgToMidi(double egValue, int paramId)
    {
        const auto& param = syntaktParameters[paramId];

        if (param.isBipolar)
        {
            // centered mapping
            const double center = (param.minValue + param.maxValue) * 0.5;
            const double range  = (param.maxValue - param.minValue) * 0.5;
            return (int)(center + (egValue * 2.0 - 1.0) * range);
        }
        else
        {
            return (int)(param.minValue + egValue * (param.maxValue - param.minValue));
        }
    }


    void openSelectedMidiOutput()
    {
        midiOut.reset();

        auto outputs = juce::MidiOutput::getAvailableDevices();
        const int outIndex = midiOutputBox.getSelectedId() - 1;

        if (outIndex >= 0 && outIndex < outputs.size())
        {
            midiOut = juce::MidiOutput::openDevice(outputs[outIndex].identifier);
        }
    }

    // ensure that LFO Waveforms start from 0 when unipolar
    double getWaveformStartPhase (int shapeId,
                              bool bipolar,
                              bool invert) const
    {
        double phase = 0.0;

        if (!bipolar)
        {
            // Unipolar → start at minimum
            switch (shapeId)
            {
                case 1: phase = 0.75; break; // sine → -1
                case 2: phase = 0.0;  break; // triangle → -1
                case 3: phase = 0.5;  break; // square → -1
                case 4: phase = 0.5;  break; // saw → min
                default: phase = 0.0; break;
            }
        }

        if (invert)
        {
            phase += 0.5;
            if (phase >= 1.0)
                phase -= 1.0;
        }

        return phase;
    }



    #if JUCE_DEBUG
    void updateLfoRouteDebugLabel()
    {
        juce::String text;

        text << "LFO Routes bipolar state:\n";

        for (int i = 0; i < maxRoutes; ++i)
        {
            const auto& r = lfoRoutes[i];

            text << "Route " << i
                 << " | ch=" << r.midiChannel
                 << " | param=" << r.parameterIndex
                 << " | bipolar=" << (r.bipolar ? "TRUE" : "FALSE")
                 << "\n";
        }

        lfoRouteDebugLabel.setText(text, juce::dontSendNotification);
    }
    #endif




};
