// P5X — Copyright (c) 2026 Jeet Patel. Licensed under GPL-3.0-or-later.
#include "midi/MidiLearn.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace p5x::midi
{
namespace
{
constexpr float kNoOverride = std::numeric_limits<float>::quiet_NaN();
constexpr float kPickupWindow = 1.0f / 127.0f + 1.0e-6f; // "lands within 1/127"
} // namespace

MidiLearn::MidiLearn (std::vector<ParamDescriptor> parameters)
    : params (std::move (parameters))
{
    const auto n = params.size();
    overrides = std::make_unique<std::atomic<float>[]> (n);
    lastApplied = std::make_unique<std::atomic<float>[]> (n);
    pendingHostUpdates = std::make_unique<std::atomic<int>[]> (n);
    pickupArmed = std::make_unique<std::atomic<bool>[]> (n);

    for (size_t i = 0; i < n; ++i)
    {
        overrides[i].store (kNoOverride);
        lastApplied[i].store (-1.0f);
        pendingHostUpdates[i].store (0);
        pickupArmed[i].store (true); // a fresh instance counts as "after a load"
    }

    for (auto& entry : ccToParam)
        entry.store (-1);

    lastCcValue.fill (-1);
}

bool MidiLearn::isReserved (int cc) noexcept
{
    return cc == 1 || cc == 64 || (cc >= 120 && cc <= 127);
}

const char* MidiLearn::reservedName (int cc) noexcept
{
    switch (cc)
    {
        case 1:   return "Wheel-Mod";
        case 64:  return "sustain";
        case 120: return "All Sound Off";
        case 121: return "Reset All Controllers";
        case 122: return "Local Control";
        case 123: return "All Notes Off";
        case 124:
        case 125: return "Omni mode";
        case 126:
        case 127: return "Mono/Poly mode";
        default:  return "not reserved";
    }
}

int MidiLearn::indexOfParam (const juce::String& paramId) const noexcept
{
    for (size_t i = 0; i < params.size(); ++i)
        if (params[i].id == paramId)
            return (int) i;

    return -1;
}

float MidiLearn::quantise (int paramIndex, float normalised) const noexcept
{
    const auto& p = params[(size_t) paramIndex];

    if (p.kind == ParamKind::Int && p.numSteps > 1)
    {
        const float steps = (float) (p.numSteps - 1);
        return std::round (normalised * steps) / steps;
    }

    return normalised;
}

//==================================================================================================
MidiLearn::CcResult MidiLearn::handleControlChange (int cc, int value, const ParamValueSource& source) noexcept
{
    CcResult result;

    if (cc < 0 || cc >= kNumCcs)
        return result;

    const int previousValue = lastCcValue[(size_t) cc];
    lastCcValue[(size_t) cc] = value;

    int learning = learningParam.load (std::memory_order_acquire);

    if (learning >= 0)
    {
        if (isReserved (cc))
        {
            result.outcome = Outcome::ReservedWhileLearning;
            result.paramIndex = learning;
            return result;
        }

        // Capture exactly once; the message thread commits and clears the pending state.
        if (learningParam.compare_exchange_strong (learning, kCapturePending, std::memory_order_acq_rel))
        {
            const auto scope = learnFifo.write (1);

            if (scope.blockSize1 > 0)
                learnBuffer[(size_t) scope.startIndex1] = { cc, learning };

            result.outcome = Outcome::LearnCaptured;
            result.paramIndex = learning;
            return result;
        }
    }

    if (learning == kCapturePending)
    {
        result.outcome = Outcome::NoChange;
        return result;
    }

    const int paramIndex = ccToParam[(size_t) cc].load (std::memory_order_relaxed);

    if (paramIndex < 0 || paramIndex >= (int) params.size())
        return result;

    result.paramIndex = paramIndex;
    const auto& param = params[(size_t) paramIndex];
    float normalised = 0.0f;

    switch (param.kind)
    {
        case ParamKind::Bool:
        {
            // Toggle on each rising crossing of 64; no pickup (DECISIONS.md, pickup is Float/Int only).
            const bool rising = value >= 64 && previousValue < 64;

            if (! rising)
            {
                result.outcome = Outcome::NoChange;
                return result;
            }

            normalised = source.getNormalised (paramIndex) >= 0.5f ? 0.0f : 1.0f;
            break;
        }

        case ParamKind::Choice:
        {
            // Equal zones, one per option; always jumps.
            const int options = std::max (1, param.numSteps);
            const int zone = std::min (options - 1, value * options / 128);
            normalised = options > 1 ? (float) zone / (float) (options - 1) : 0.0f;
            break;
        }

        case ParamKind::Float:
        case ParamKind::Int:
        default:
        {
            normalised = quantise (paramIndex, (float) value / 127.0f);

            const bool pickup = getTakeover() == Takeover::Pickup
                             && pickupArmed[(size_t) paramIndex].load (std::memory_order_relaxed);

            if (pickup)
            {
                const float current = source.getNormalised (paramIndex);
                const float raw = (float) value / 127.0f;
                bool caught = std::abs (raw - current) <= kPickupWindow;

                if (! caught && previousValue >= 0)
                {
                    const float previous = (float) previousValue / 127.0f;
                    caught = (previous - current) * (raw - current) <= 0.0f;
                }

                if (! caught)
                {
                    result.outcome = Outcome::PickupWaiting;
                    return result;
                }

                pickupArmed[(size_t) paramIndex].store (false, std::memory_order_relaxed);
                result.outcome = Outcome::PickupCaught;
            }

            break;
        }
    }

    // Two paths (07-midi.md § Applying a learned CC): the override slot for sound now, the FIFO for the host.
    overrides[(size_t) paramIndex].store (normalised, std::memory_order_relaxed);
    lastApplied[(size_t) paramIndex].store (normalised, std::memory_order_relaxed);

    const auto scope = hostFifo.write (1);

    if (scope.blockSize1 > 0)
    {
        pendingHostUpdates[(size_t) paramIndex].fetch_add (1, std::memory_order_relaxed);
        hostBuffer[(size_t) scope.startIndex1] = { paramIndex, normalised };
    }

    if (result.outcome != Outcome::PickupCaught)
        result.outcome = Outcome::Applied;

    result.value = normalised;
    return result;
}

float MidiLearn::getOverride (int paramIndex) const noexcept
{
    return overrides[(size_t) paramIndex].load (std::memory_order_relaxed);
}

void MidiLearn::clearOverrideIfMatched (int paramIndex, float parameterValue) noexcept
{
    auto& slot = overrides[(size_t) paramIndex];
    float current = slot.load (std::memory_order_relaxed);

    if (! std::isnan (current) && std::abs (current - parameterValue) < 1.0e-5f)
        slot.compare_exchange_strong (current, kNoOverride, std::memory_order_relaxed);
}

//==================================================================================================
void MidiLearn::parameterChangedExternally (int paramIndex, float normalisedValue) noexcept
{
    if (paramIndex < 0 || paramIndex >= (int) params.size())
        return;

    // Our own host updates echo back through the parameter listener; they aren't a UI/host change.
    if (pendingHostUpdates[(size_t) paramIndex].load (std::memory_order_relaxed) > 0)
        return;

    if (std::abs (normalisedValue - lastApplied[(size_t) paramIndex].load (std::memory_order_relaxed)) < 1.0e-4f)
        return;

    pickupArmed[(size_t) paramIndex].store (true, std::memory_order_relaxed);
    overrides[(size_t) paramIndex].store (kNoOverride, std::memory_order_relaxed);
}

void MidiLearn::armAllPickups() noexcept
{
    for (size_t i = 0; i < params.size(); ++i)
        pickupArmed[i].store (true, std::memory_order_relaxed);
}

//==================================================================================================
void MidiLearn::startLearn (int paramIndex)
{
    if (paramIndex < 0 || paramIndex >= (int) params.size())
        return;

    // Only one control waits at a time: starting another learn cancels the previous one.
    waitingParam = paramIndex;
    learningParam.store (paramIndex, std::memory_order_release);
}

void MidiLearn::cancelLearn()
{
    waitingParam = kNotLearning;
    learningParam.store (kNotLearning, std::memory_order_release);
    // Anything already captured is dropped by popLearnCommit(), because nothing is waiting any more.
}

bool MidiLearn::popLearnCommit (LearnCommit& commit)
{
    Captured captured;

    {
        const auto scope = learnFifo.read (1);

        if (scope.blockSize1 == 0)
            return false;

        captured = learnBuffer[(size_t) scope.startIndex1];
    }

    int pending = kCapturePending;
    learningParam.compare_exchange_strong (pending, kNotLearning, std::memory_order_acq_rel);

    if (waitingParam != captured.paramIndex)
        return false; // cancelled or superseded meanwhile

    waitingParam = kNotLearning;
    commit = setMapping (captured.cc, captured.paramIndex);
    return commit.cc >= 0;
}

bool MidiLearn::popHostUpdate (HostUpdate& update) noexcept
{
    const auto scope = hostFifo.read (1);

    if (scope.blockSize1 == 0)
        return false;

    update = hostBuffer[(size_t) scope.startIndex1];
    return true;
}

void MidiLearn::hostUpdateApplied (int paramIndex) noexcept
{
    if (paramIndex >= 0 && paramIndex < (int) params.size())
        pendingHostUpdates[(size_t) paramIndex].fetch_sub (1, std::memory_order_relaxed);
}

//==================================================================================================
MidiLearn::LearnCommit MidiLearn::setMapping (int cc, int paramIndex)
{
    LearnCommit commit;

    if (cc < 0 || cc >= kNumCcs || isReserved (cc) || paramIndex < 0 || paramIndex >= (int) params.size())
        return commit;

    const juce::ScopedLock sl (mapLock);
    const auto& paramId = params[(size_t) paramIndex].id;

    if (auto existing = map.find (cc); existing != map.end() && existing->second != paramId)
        commit.previousParamForCc = indexOfParam (existing->second);

    for (auto it = map.begin(); it != map.end(); ++it)
    {
        if (it->second == paramId && it->first != cc)
        {
            commit.previousCcForParam = it->first;
            map.erase (it);
            break;
        }
    }

    map[cc] = paramId;
    commit.cc = cc;
    commit.paramIndex = paramIndex;
    rebuildTableLocked();
    return commit;
}

void MidiLearn::removeMappingForParam (int paramIndex)
{
    if (paramIndex < 0 || paramIndex >= (int) params.size())
        return;

    const juce::ScopedLock sl (mapLock);
    const auto& paramId = params[(size_t) paramIndex].id;

    for (auto it = map.begin(); it != map.end();)
        it = (it->second == paramId) ? map.erase (it) : std::next (it);

    rebuildTableLocked();
}

void MidiLearn::clearAll()
{
    const juce::ScopedLock sl (mapLock);
    map.clear();
    rebuildTableLocked();
}

int MidiLearn::getCcForParam (int paramIndex) const
{
    if (paramIndex < 0 || paramIndex >= (int) params.size())
        return -1;

    const juce::ScopedLock sl (mapLock);

    for (const auto& [cc, id] : map)
        if (id == params[(size_t) paramIndex].id)
            return cc;

    return -1;
}

int MidiLearn::getParamForCc (int cc) const noexcept
{
    return (cc >= 0 && cc < kNumCcs) ? ccToParam[(size_t) cc].load (std::memory_order_relaxed) : -1;
}

std::map<int, juce::String> MidiLearn::getMap() const
{
    const juce::ScopedLock sl (mapLock);
    return map;
}

int MidiLearn::getNumMappings() const
{
    const juce::ScopedLock sl (mapLock);
    return (int) map.size();
}

std::unique_ptr<juce::XmlElement> MidiLearn::createXml() const
{
    auto xml = std::make_unique<juce::XmlElement> ("MidiMap");
    const juce::ScopedLock sl (mapLock);

    for (const auto& [cc, id] : map)
    {
        auto* entry = xml->createNewChildElement ("Map");
        entry->setAttribute ("cc", cc);
        entry->setAttribute ("param", id);
    }

    return xml;
}

void MidiLearn::loadXml (const juce::XmlElement& midiMap)
{
    const juce::ScopedLock sl (mapLock);
    map.clear();

    for (auto* entry : midiMap.getChildWithTagNameIterator ("Map"))
    {
        const auto ccText = entry->getStringAttribute ("cc").trim();
        const auto paramId = entry->getStringAttribute ("param");

        if (! ccText.containsOnly ("0123456789") || ccText.isEmpty())
            continue;

        const int cc = ccText.getIntValue();

        if (cc < 0 || cc >= kNumCcs || isReserved (cc) || indexOfParam (paramId) < 0)
            continue;

        // One parameter, one CC: a later duplicate replaces the earlier one.
        for (auto it = map.begin(); it != map.end(); ++it)
        {
            if (it->second == paramId)
            {
                map.erase (it);
                break;
            }
        }

        map[cc] = paramId;
    }

    rebuildTableLocked();
}

void MidiLearn::rebuildTableLocked()
{
    std::array<int16_t, kNumCcs> table;
    table.fill (-1);

    for (const auto& [cc, id] : map)
        table[(size_t) cc] = (int16_t) indexOfParam (id);

    for (size_t cc = 0; cc < table.size(); ++cc)
        ccToParam[cc].store (table[cc], std::memory_order_relaxed);
}
} // namespace p5x::midi
