// P5X — Copyright (c) 2026 Jeet Patel. Licensed under GPL-3.0-or-later.
#include "midi/MidiLearn.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>

using namespace p5x::midi;
using Catch::Approx;
using Outcome = MidiLearn::Outcome;

namespace
{
enum Index
{
    cutoff,
    resonance,
    unison,
    lfoShape,
    oscAFreq
};

MidiLearn makeLearn()
{
    return MidiLearn ({ { "flt_cutoff", "Filter Cutoff", ParamKind::Float, 0 },
                        { "flt_res", "Filter Resonance", ParamKind::Float, 0 },
                        { "perf_unison", "Unison", ParamKind::Bool, 0 },
                        { "lfo_shape", "LFO Shape", ParamKind::Choice, 3 },
                        { "osc_a_freq", "Osc A Frequency", ParamKind::Int, 49 } });
}

// Plays the role of the parameters: applying a CC writes the value back like the host update would.
struct FakeParams : ParamValueSource
{
    std::array<float, 5> values { 0.5f, 0.0f, 0.0f, 0.5f, 0.5f };

    float getNormalised (int index) const noexcept override { return values[(size_t) index]; }
};

MidiLearn::CcResult sendCc (MidiLearn& learn, FakeParams& params, int cc, int value)
{
    const auto result = learn.handleControlChange (cc, value, params);

    if (result.outcome == Outcome::Applied || result.outcome == Outcome::PickupCaught)
        params.values[(size_t) result.paramIndex] = result.value;

    return result;
}

void learnCc (MidiLearn& learn, FakeParams& params, int paramIndex, int cc)
{
    learn.startLearn (paramIndex);
    REQUIRE (sendCc (learn, params, cc, 0).outcome == Outcome::LearnCaptured);
    MidiLearn::LearnCommit commit;
    REQUIRE (learn.popLearnCommit (commit));
}
} // namespace

TEST_CASE ("MidiLearn: reserved CCs are ignored while learning; CC 74 binds", "[midi][learn]")
{
    auto learn = makeLearn();
    FakeParams params;

    learn.startLearn (cutoff);
    REQUIRE (learn.getWaitingParam() == cutoff);

    REQUIRE (sendCc (learn, params, 64, 127).outcome == Outcome::ReservedWhileLearning);
    REQUIRE (sendCc (learn, params, 1, 10).outcome == Outcome::ReservedWhileLearning);
    REQUIRE (sendCc (learn, params, 120, 0).outcome == Outcome::ReservedWhileLearning);
    REQUIRE (learn.getNumMappings() == 0);

    const auto captured = sendCc (learn, params, 74, 50);
    REQUIRE (captured.outcome == Outcome::LearnCaptured);
    REQUIRE (params.values[cutoff] == 0.5f); // the capturing CC isn't applied

    MidiLearn::LearnCommit commit;
    REQUIRE (learn.popLearnCommit (commit));
    REQUIRE (commit.cc == 74);
    REQUIRE (commit.paramIndex == cutoff);
    REQUIRE (learn.getWaitingParam() == MidiLearn::kNotLearning);
    REQUIRE (learn.getMap() == std::map<int, juce::String> { { 74, "flt_cutoff" } });
    REQUIRE (learn.getParamForCc (74) == cutoff);
}

TEST_CASE ("MidiLearn: re-learning CC 74 on resonance moves it", "[midi][learn]")
{
    auto learn = makeLearn();
    FakeParams params;
    learnCc (learn, params, cutoff, 74);

    learn.startLearn (resonance);
    sendCc (learn, params, 74, 0);
    MidiLearn::LearnCommit commit;
    REQUIRE (learn.popLearnCommit (commit));

    REQUIRE (commit.previousParamForCc == cutoff);
    REQUIRE (learn.getCcForParam (cutoff) == -1);
    REQUIRE (learn.getCcForParam (resonance) == 74);
    REQUIRE (learn.getNumMappings() == 1);
}

TEST_CASE ("MidiLearn: a new CC for a mapped parameter replaces the old one", "[midi][learn]")
{
    auto learn = makeLearn();
    FakeParams params;
    learnCc (learn, params, cutoff, 74);

    learn.startLearn (cutoff);
    sendCc (learn, params, 20, 0);
    MidiLearn::LearnCommit commit;
    REQUIRE (learn.popLearnCommit (commit));

    REQUIRE (commit.previousCcForParam == 74);
    REQUIRE (learn.getParamForCc (74) == -1);
    REQUIRE (learn.getCcForParam (cutoff) == 20);
}

TEST_CASE ("MidiLearn: only one control waits; the last learn wins", "[midi][learn]")
{
    auto learn = makeLearn();
    FakeParams params;

    learn.startLearn (cutoff);
    learn.startLearn (resonance);
    sendCc (learn, params, 30, 0);
    MidiLearn::LearnCommit commit;
    REQUIRE (learn.popLearnCommit (commit));
    REQUIRE (commit.paramIndex == resonance);
}

TEST_CASE ("MidiLearn: cancel leaves the map unchanged", "[midi][learn]")
{
    auto learn = makeLearn();
    FakeParams params;

    learn.startLearn (cutoff);
    learn.cancelLearn();
    REQUIRE (learn.getWaitingParam() == MidiLearn::kNotLearning);
    REQUIRE (sendCc (learn, params, 74, 10).outcome == Outcome::Unmapped);

    // Captured, then cancelled before the commit: dropped.
    learn.startLearn (cutoff);
    sendCc (learn, params, 75, 0);
    learn.cancelLearn();
    MidiLearn::LearnCommit commit;
    REQUIRE_FALSE (learn.popLearnCommit (commit));
    REQUIRE (learn.getNumMappings() == 0);
}

TEST_CASE ("MidiLearn: Bool toggles on each rising crossing of 64", "[midi][learn]")
{
    auto learn = makeLearn();
    FakeParams params;
    learnCc (learn, params, unison, 20);

    int toggles = 0;

    for (int value : { 0, 127, 127, 0, 127 })
    {
        const float before = params.values[unison];
        sendCc (learn, params, 20, value);
        toggles += params.values[unison] != before ? 1 : 0;
    }

    REQUIRE (toggles == 2);
    REQUIRE (params.values[unison] == 0.0f); // off → on → off
}

TEST_CASE ("MidiLearn: pickup waits until the CC reaches the parameter", "[midi][learn]")
{
    auto learn = makeLearn();
    FakeParams params;
    learnCc (learn, params, cutoff, 74);
    learn.parameterChangedExternally (cutoff, 0.5f); // a UI/host change arms pickup
    params.values[cutoff] = 0.5f;

    for (int value : { 10, 20, 30 })
    {
        REQUIRE (sendCc (learn, params, 74, value).outcome == Outcome::PickupWaiting);
        REQUIRE (params.values[cutoff] == 0.5f);
    }

    REQUIRE (sendCc (learn, params, 74, 64).outcome == Outcome::PickupCaught);
    REQUIRE (params.values[cutoff] == Approx (64.0f / 127.0f));

    REQUIRE (sendCc (learn, params, 74, 10).outcome == Outcome::Applied); // now tracks
    REQUIRE (params.values[cutoff] == Approx (10.0f / 127.0f));
}

TEST_CASE ("MidiLearn: pickup catches a CC that jumps across the value", "[midi][learn]")
{
    auto learn = makeLearn();
    FakeParams params;
    learnCc (learn, params, cutoff, 74);
    learn.armAllPickups();

    REQUIRE (sendCc (learn, params, 74, 40).outcome == Outcome::PickupWaiting);
    REQUIRE (sendCc (learn, params, 74, 90).outcome == Outcome::PickupCaught);
}

TEST_CASE ("MidiLearn: Jump applies immediately", "[midi][learn]")
{
    auto learn = makeLearn();
    FakeParams params;
    learnCc (learn, params, cutoff, 74);
    learn.armAllPickups();
    learn.setTakeover (Takeover::Jump);

    REQUIRE (sendCc (learn, params, 74, 10).outcome == Outcome::Applied);
    REQUIRE (params.values[cutoff] == Approx (10.0f / 127.0f));
}

TEST_CASE ("MidiLearn: pickup never blocks Bool or Choice parameters", "[midi][learn]")
{
    auto learn = makeLearn();
    FakeParams params;
    learnCc (learn, params, unison, 20);
    learnCc (learn, params, lfoShape, 21);
    learn.armAllPickups();

    REQUIRE (sendCc (learn, params, 20, 127).outcome == Outcome::Applied);
    REQUIRE (params.values[unison] == 1.0f);

    REQUIRE (sendCc (learn, params, 21, 0).outcome == Outcome::Applied);
    REQUIRE (params.values[lfoShape] == 0.0f);
}

TEST_CASE ("MidiLearn: Choice CCs split into equal zones", "[midi][learn]")
{
    auto learn = makeLearn();
    FakeParams params;
    learnCc (learn, params, lfoShape, 21);

    const std::vector<std::pair<int, float>> expected { { 0, 0.0f }, { 42, 0.0f }, { 43, 0.5f }, { 85, 0.5f },
                                                        { 86, 1.0f }, { 127, 1.0f } };

    for (const auto& [value, normalised] : expected)
    {
        sendCc (learn, params, 21, value);
        REQUIRE (params.values[lfoShape] == normalised);
    }
}

TEST_CASE ("MidiLearn: Int CCs round to the nearest step", "[midi][learn]")
{
    auto learn = makeLearn();
    FakeParams params;
    learnCc (learn, params, oscAFreq, 22);
    learn.setTakeover (Takeover::Jump);

    sendCc (learn, params, 22, 100);
    const float steps = params.values[oscAFreq] * 48.0f;
    REQUIRE (steps == Approx (std::round (steps)).margin (1.0e-4));
    REQUIRE (steps == Approx (std::round (100.0f / 127.0f * 48.0f)));
}

TEST_CASE ("MidiLearn: override slot and host update path", "[midi][learn]")
{
    auto learn = makeLearn();
    FakeParams params;
    learnCc (learn, params, cutoff, 74);
    learn.setTakeover (Takeover::Jump);

    REQUIRE (std::isnan (learn.getOverride (cutoff)));
    learn.handleControlChange (74, 100, params); // don't write back: the host hasn't caught up yet

    const float expected = 100.0f / 127.0f;
    REQUIRE (learn.getOverride (cutoff) == Approx (expected));

    MidiLearn::HostUpdate update;
    REQUIRE (learn.popHostUpdate (update));
    REQUIRE (update.paramIndex == cutoff);
    REQUIRE (update.value == Approx (expected));
    REQUIRE_FALSE (learn.popHostUpdate (update));

    // The host echo is not a UI/host change: no pickup armed.
    learn.parameterChangedExternally (cutoff, update.value);
    learn.hostUpdateApplied (cutoff);

    learn.clearOverrideIfMatched (cutoff, 0.1f);
    REQUIRE_FALSE (std::isnan (learn.getOverride (cutoff)));
    learn.clearOverrideIfMatched (cutoff, update.value);
    REQUIRE (std::isnan (learn.getOverride (cutoff)));
}

TEST_CASE ("MidiLearn: a UI change arms pickup, the CC's own echo does not", "[midi][learn]")
{
    auto learn = makeLearn();
    FakeParams params;
    learnCc (learn, params, cutoff, 74);
    learn.setTakeover (Takeover::Pickup);

    // Catch it first.
    sendCc (learn, params, 74, 64);
    REQUIRE (sendCc (learn, params, 74, 70).outcome == Outcome::Applied);

    MidiLearn::HostUpdate update;

    while (learn.popHostUpdate (update))
    {
        learn.parameterChangedExternally (update.paramIndex, update.value);
        learn.hostUpdateApplied (update.paramIndex);
    }

    REQUIRE (sendCc (learn, params, 74, 72).outcome == Outcome::Applied);

    // The user moves the on-screen control far away.
    learn.parameterChangedExternally (cutoff, 0.1f);
    params.values[cutoff] = 0.1f;
    REQUIRE (sendCc (learn, params, 74, 73).outcome == Outcome::PickupWaiting);
}

TEST_CASE ("MidiLearn: unmapped CCs do nothing", "[midi][learn]")
{
    auto learn = makeLearn();
    FakeParams params;
    REQUIRE (sendCc (learn, params, 30, 64).outcome == Outcome::Unmapped);
}

TEST_CASE ("MidiLearn: reserved CCs can't be mapped", "[midi][learn]")
{
    auto learn = makeLearn();

    for (int cc : { 1, 64, 120, 121, 122, 123, 124, 125, 126, 127 })
    {
        REQUIRE (MidiLearn::isReserved (cc));
        REQUIRE (learn.setMapping (cc, cutoff).cc == -1);
    }

    REQUIRE_FALSE (MidiLearn::isReserved (0));
    REQUIRE_FALSE (MidiLearn::isReserved (74));
    REQUIRE (learn.getNumMappings() == 0);
    REQUIRE (juce::String (MidiLearn::reservedName (64)) == "sustain");
}

TEST_CASE ("MidiLearn: XML round trip by parameter ID", "[midi][learn]")
{
    auto learn = makeLearn();
    learn.setMapping (74, cutoff);
    learn.setMapping (71, resonance);
    learn.setMapping (20, unison);

    const auto xml = learn.createXml();
    REQUIRE (xml->hasTagName ("MidiMap"));

    auto other = makeLearn();
    other.loadXml (*xml);
    REQUIRE (other.getMap() == learn.getMap());
    REQUIRE (other.getParamForCc (71) == resonance);
}

TEST_CASE ("MidiLearn: loading bad map XML skips bad entries", "[midi][learn]")
{
    auto learn = makeLearn();
    const auto xml = juce::parseXML (R"(<MidiMap>
        <Map cc="74" param="flt_cutoff"/>
        <Map cc="64" param="flt_res"/>
        <Map cc="300" param="flt_res"/>
        <Map cc="abc" param="flt_res"/>
        <Map cc="20" param="no_such_param"/>
        <Map cc="21" param="flt_cutoff"/>
        <Unknown/>
    </MidiMap>)");

    REQUIRE (xml != nullptr);
    learn.loadXml (*xml);

    // One parameter, one CC: the later cutoff mapping replaces the earlier one.
    REQUIRE (learn.getMap() == std::map<int, juce::String> { { 21, "flt_cutoff" } });
}

TEST_CASE ("MidiLearn: clearAll empties the audio table", "[midi][learn]")
{
    auto learn = makeLearn();
    learn.setMapping (74, cutoff);
    learn.clearAll();

    REQUIRE (learn.getNumMappings() == 0);
    REQUIRE (learn.getParamForCc (74) == -1);
}
