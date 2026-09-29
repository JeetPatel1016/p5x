// P5X — Copyright (c) 2026 Jeet Patel. Licensed under GPL-3.0-or-later.
#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include <array>
#include <atomic>
#include <map>
#include <memory>
#include <vector>

namespace p5x::midi
{
enum class ParamKind : uint8_t
{
    Float,
    Int,
    Bool,
    Choice
};

struct ParamDescriptor
{
    juce::String id;
    juce::String name;
    ParamKind kind = ParamKind::Float;
    int numSteps = 0; // Int: max − min + 1; Choice: number of options; otherwise unused
};

enum class Takeover
{
    Pickup,
    Jump
};

// Lets MIDI Learn read a parameter's current normalised value from the audio thread.
struct ParamValueSource
{
    virtual ~ParamValueSource() = default;
    virtual float getNormalised (int paramIndex) const noexcept = 0;
};

// Kontakt-style MIDI Learn (07-midi.md § MIDI Learn). Threading:
//  - audio thread: handleControlChange(), override slots
//  - message thread: learn start/cancel, commits, map edits, XML, host updates
//  - any thread: parameterChangedExternally(), armAllPickups(), takeover mode
class MidiLearn
{
public:
    static constexpr int kNumCcs = 128;
    static constexpr int kNotLearning = -1;

    explicit MidiLearn (std::vector<ParamDescriptor> parameters);

    static bool isReserved (int cc) noexcept;
    static const char* reservedName (int cc) noexcept; // e.g. "sustain" for CC 64

    const std::vector<ParamDescriptor>& getParams() const noexcept { return params; }
    int indexOfParam (const juce::String& paramId) const noexcept;

    //==============================================================================================
    // Audio thread
    enum class Outcome
    {
        Unmapped,
        LearnCaptured,
        ReservedWhileLearning,
        Applied,
        PickupWaiting,
        PickupCaught,
        NoChange
    };

    struct CcResult
    {
        Outcome outcome = Outcome::Unmapped;
        int paramIndex = -1;
        float value = 0.0f;
    };

    CcResult handleControlChange (int cc, int value, const ParamValueSource& source) noexcept;

    // Normalised value the DSP should use instead of the parameter, or NaN when none.
    float getOverride (int paramIndex) const noexcept;
    // Clears the override once the parameter has caught up with it (07-midi.md § Applying a learned CC).
    void clearOverrideIfMatched (int paramIndex, float parameterValue) noexcept;

    //==============================================================================================
    // Any thread
    void parameterChangedExternally (int paramIndex, float normalisedValue) noexcept;
    void armAllPickups() noexcept;
    void setTakeover (Takeover mode) noexcept { takeover.store ((int) mode, std::memory_order_relaxed); }
    Takeover getTakeover() const noexcept { return (Takeover) takeover.load (std::memory_order_relaxed); }

    //==============================================================================================
    // Message thread
    void startLearn (int paramIndex);
    void cancelLearn();
    int getWaitingParam() const noexcept { return waitingParam; } // −1 when no control is waiting

    struct LearnCommit
    {
        int cc = -1;
        int paramIndex = -1;
        int previousParamForCc = -1; // the CC moved away from this parameter
        int previousCcForParam = -1; // the parameter had this CC before
    };

    // Commits one captured learn event, if any. Call from a message-thread timer.
    bool popLearnCommit (LearnCommit& commit);

    struct HostUpdate
    {
        int paramIndex = -1;
        float value = 0.0f;
    };

    bool popHostUpdate (HostUpdate& update) noexcept;
    // Call after the host update has been applied with setValueNotifyingHost.
    void hostUpdateApplied (int paramIndex) noexcept;

    // Moves the CC if it was mapped elsewhere; replaces the parameter's previous CC.
    LearnCommit setMapping (int cc, int paramIndex);
    void removeMappingForParam (int paramIndex);
    void clearAll();

    int getCcForParam (int paramIndex) const;
    int getParamForCc (int cc) const noexcept;
    std::map<int, juce::String> getMap() const;
    int getNumMappings() const;

    // <MidiMap><Map cc="74" param="flt_cutoff"/></MidiMap>, by parameter ID (07-midi.md § Persistence).
    std::unique_ptr<juce::XmlElement> createXml() const;
    void loadXml (const juce::XmlElement& midiMap);

private:
    void rebuildTableLocked();
    float quantise (int paramIndex, float normalised) const noexcept;

    std::vector<ParamDescriptor> params;

    // Audio side: CC → parameter index, −1 = unmapped.
    std::array<std::atomic<int16_t>, kNumCcs> ccToParam;

    // ≥ 0: the parameter waiting for a CC; kCapturePending: a CC was captured, not yet committed.
    static constexpr int kCapturePending = -2;
    std::atomic<int> learningParam { kNotLearning };
    int waitingParam = kNotLearning;

    struct Captured
    {
        int cc = -1;
        int paramIndex = -1;
    };

    juce::AbstractFifo learnFifo { 16 };
    std::array<Captured, 16> learnBuffer {};

    juce::AbstractFifo hostFifo { 1024 };
    std::array<HostUpdate, 1024> hostBuffer {};

    std::unique_ptr<std::atomic<float>[]> overrides;
    std::unique_ptr<std::atomic<float>[]> lastApplied;
    std::unique_ptr<std::atomic<int>[]> pendingHostUpdates;
    std::unique_ptr<std::atomic<bool>[]> pickupArmed;
    std::array<int, kNumCcs> lastCcValue {}; // audio thread only; −1 = none yet

    std::atomic<int> takeover { (int) Takeover::Pickup };

    mutable juce::CriticalSection mapLock; // message side only, never taken on the audio thread
    std::map<int, juce::String> map;       // authoritative: CC → parameter ID
};
} // namespace p5x::midi
