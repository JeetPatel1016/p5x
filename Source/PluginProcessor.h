// P5X — Copyright (c) 2026 Jeet Patel. Licensed under GPL-3.0-or-later.
#pragma once

#include "debug/LogService.h"
#include "debug/Telemetry.h"
#include "dsp/Smoother.h"
#include "dsp/Voice.h"
#include "dsp/VoiceAllocator.h"
#include "midi/MidiHandler.h"
#include "midi/MidiLearn.h"
#include "midi/MidiMonitor.h"
#include "params/ParameterIDs.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <array>
#include <atomic>
#include <deque>
#include <functional>

class P5XAudioProcessor final : public juce::AudioProcessor,
                                private juce::AudioProcessorParameter::Listener,
                                private juce::Timer,
                                private p5x::midi::MidiSink,
                                private p5x::midi::ParamValueSource
{
public:
    static constexpr int kMaxVoices = 10;
    static constexpr int kMonitorHistory = 200;

    P5XAudioProcessor();
    ~P5XAudioProcessor() override;

    //==============================================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==============================================================================================
    // Message-thread API for the editor, settings dialog, debug console and Standalone app.
    juce::AudioProcessorValueTreeState& getValueTreeState() noexcept { return apvts; }
    p5x::midi::MidiLearn& getMidiLearn() noexcept { return midiLearn; }
    p5x::debug::LogService& getLogService() noexcept { return *logService; }
    uint16_t getInstanceId() const noexcept { return instanceId; }

    juce::RangedAudioParameter* getParameterByIndex (int index) const noexcept;
    int getParameterIndex (const juce::String& paramId) const noexcept;

    // Latest telemetry (returns the previous snapshot when nothing new arrived).
    p5x::debug::TelemetrySnapshot getTelemetry();
    uint32_t getMidiActivityCount() const noexcept { return midiActivity.load (std::memory_order_relaxed); }
    bool isNoteHeld (int note) const noexcept;
    const std::deque<p5x::midi::MonitorEvent>& getMonitorHistory() const noexcept { return monitorHistory; }
    uint64_t getMonitorEventCount() const noexcept { return monitorEventCount; }

    // On-screen keyboard, wheels and computer keyboard: sent on the active MIDI channel so the
    // channel filter never drops them.
    void injectMidi (const juce::MidiMessage& message);
    void injectNoteOn (int note, int velocity);
    void injectNoteOff (int note);

    void panic() noexcept { panicRequested.store (true); }
    void startTestTone() noexcept { testToneRequested.store (true); }

    // State-only settings (01-parameters.md § State-only).
    int getMidiChannel() const noexcept { return midiChannel.load(); }
    void setMidiChannel (int channel);
    p5x::midi::Takeover getTakeover() const noexcept { return midiLearn.getTakeover(); }
    void setTakeover (p5x::midi::Takeover mode) { midiLearn.setTakeover (mode); }
    bool isHqMode() const noexcept { return hqMode.load(); }
    void setHqMode (bool on) { hqMode.store (on); }
    bool isProgramChangeEnabled() const noexcept { return programChangeEnabled.load(); }
    void setProgramChangeEnabled (bool on) { programChangeEnabled.store (on); }
    float getWindowScale() const noexcept { return windowScale.load(); }
    uint64_t getSeed() const noexcept { return seed.load(); }

    // Global files in %APPDATA%/P5X (09-presets-and-state.md).
    void saveSettingsAsDefault();
    bool saveMidiMapAsDefault();
    void clearMidiMap();

    // MIDI Learn events for the editor (message thread).
    std::function<void (const p5x::midi::MidiLearn::LearnCommit&)> onLearnCommitted;

    // The 60 Hz message-thread work: MIDI Learn commits and host updates, MIDI monitor history.
    // Called by the timer; public so tests can run it without a message loop.
    void serviceMessageThread();

private:
    //==============================================================================================
    // MidiSink (audio thread)
    void noteOn (int note, int velocity) override;
    void noteOff (int note) override;
    void sustainPedal (bool down) override;
    void allSoundOff() override;
    void allNotesOff() override;
    void resetAllControllers() override;
    void pitchBend (int value14) override;
    void modWheel (int value) override;
    void channelPressure (int value) override;
    void polyPressure (int note, int value) override;
    void controlChange (int cc, int value) override;
    void programChange (int program) override;

    // ParamValueSource (audio thread)
    float getNormalised (int paramIndex) const noexcept override;

    // AudioProcessorParameter::Listener (any thread)
    void parameterValueChanged (int parameterIndex, float newValue) override;
    void parameterGestureChanged (int, bool) override {}

    void timerCallback() override { serviceMessageThread(); }

    //==============================================================================================
    void handleMidiEvent (const uint8_t* data, int size);
    void render (juce::AudioBuffer<float>& buffer, int start, int end);
    float effectiveValue (int paramIndex) noexcept; // real units, honouring MIDI Learn overrides
    void applyVoiceCountIfIdle();
    void setHeld (int note, bool held) noexcept;

    void resetToDefaults();
    void readSettingsElement (const juce::XmlElement& settings, bool includeScale);
    std::unique_ptr<juce::XmlElement> createSettingsElement (bool includeScale) const;
    void loadGlobalDefaults();

    //==============================================================================================
    juce::SharedResourcePointer<p5x::debug::LogService> logService;
    const uint16_t instanceId;

    juce::AudioProcessorValueTreeState apvts;
    std::array<juce::RangedAudioParameter*, p5x::params::kNumParameters> params {};
    int idxMasterTune = 0, idxMasterVolume = 0, idxBendRange = 0, idxVoices = 0;

    p5x::midi::MidiLearn midiLearn;
    p5x::midi::MidiHandler midiHandler;
    p5x::midi::MidiMonitorFifo monitorFifo;
    std::deque<p5x::midi::MonitorEvent> monitorHistory;
    uint64_t monitorEventCount = 0;

    // Audio thread state
    std::array<p5x::dsp::Voice, kMaxVoices> voices;
    p5x::dsp::VoiceAllocator<p5x::dsp::Voice, kMaxVoices> allocator { voices };
    p5x::dsp::LinearSmoother bendSmoother, tuneSmoother, volumeSmoother;
    std::vector<float> scratch, pitchOffset, gain;
    double currentSampleRate = 48000.0;
    float bendNormalised = 0.0f; // −1 … +1
    float modWheelValue = 0.0f, channelAftertouch = 0.0f;
    std::array<float, kMaxVoices> voicePressure {};
    double testTonePhase = 0.0;
    int testToneRemaining = 0;
    float cpuSmoothed = 0.0f;
    uint32_t xruns = 0;

    // Cross-thread
    juce::AbstractFifo injectFifo { 256 };
    std::array<std::array<uint8_t, 3>, 256> injectBuffer {};
    std::array<std::atomic<uint64_t>, 2> heldNotes {};
    std::atomic<uint32_t> midiActivity { 0 };
    std::atomic<bool> panicRequested { false }, testToneRequested { false };
    p5x::debug::TripleBuffer<p5x::debug::TelemetrySnapshot> telemetry;
    p5x::debug::TelemetrySnapshot lastTelemetry;

    // State-only settings
    std::atomic<int> midiChannel { 0 };
    std::atomic<bool> hqMode { false }, programChangeEnabled { false };
    std::atomic<float> windowScale { 1.0f };
    std::atomic<uint64_t> seed { 0 };
    juce::String presetName { "Init" };
    int presetIndex = 0;
    bool presetEdited = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (P5XAudioProcessor)
};
