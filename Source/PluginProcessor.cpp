// P5X — Copyright (c) 2026 Jeet Patel. Licensed under GPL-3.0-or-later.
#include "PluginProcessor.h"
#include "AppPaths.h"
#include "PluginEditor.h"
#include "params/ParameterLayout.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <random>

using namespace p5x;

namespace
{
constexpr int kStateVersion = 1;
constexpr float kTestToneGain = 0.25118864f; // −12 dBFS (10-debug-and-harness.md § Settings dialog)

std::vector<midi::ParamDescriptor> makeDescriptors (juce::AudioProcessorValueTreeState& state)
{
    std::vector<midi::ParamDescriptor> out;

    for (const auto* id : params::kAllIds)
    {
        auto* p = state.getParameter (id);
        jassert (p != nullptr);

        midi::ParamDescriptor d;
        d.id = id;
        d.name = p->getName (64);

        if (dynamic_cast<juce::AudioParameterBool*> (p) != nullptr)
        {
            d.kind = midi::ParamKind::Bool;
        }
        else if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (p))
        {
            d.kind = midi::ParamKind::Choice;
            d.numSteps = choice->choices.size();
        }
        else if (auto* intParam = dynamic_cast<juce::AudioParameterInt*> (p))
        {
            d.kind = midi::ParamKind::Int;
            d.numSteps = intParam->getRange().getLength() + 1;
        }

        out.push_back (d);
    }

    return out;
}

uint64_t makeSeed()
{
    // Not DSP: the seed itself may come from the OS; everything after it is deterministic.
    std::random_device device;
    const uint64_t high = device(), low = device();
    return (high << 32) ^ low ^ (uint64_t) juce::Time::getHighResolutionTicks();
}

bool isNumber (const juce::String& text)
{
    const auto t = text.trim();
    return t.isNotEmpty() && t.containsOnly ("0123456789.-+eE") && std::isfinite (t.getDoubleValue());
}

int messageSize (uint8_t status) noexcept
{
    const int type = status & 0xF0;
    return (type == 0xC0 || type == 0xD0) ? 2 : 3;
}
} // namespace

//==================================================================================================
P5XAudioProcessor::P5XAudioProcessor()
    : P5XAudioProcessor (juce::JUCEApplicationBase::isStandaloneApp())
{
}

P5XAudioProcessor::P5XAudioProcessor (bool withInputBus)
    : AudioProcessor (withInputBus ? BusesProperties()
                                         .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                         .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                                   : BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      instanceId (debug::nextInstanceId()),
      inputBusEnabled (withInputBus),
      apvts (*this, nullptr, "PARAMETERS", params::createParameterLayout()),
      midiLearn (makeDescriptors (apvts))
{
    for (size_t i = 0; i < params::kAllIds.size(); ++i)
    {
        params[i] = apvts.getParameter (params::kAllIds[i]);
        jassert (params[i] != nullptr && params[i]->getParameterIndex() == (int) i);
        params[i]->addListener (this);
    }

    using namespace params::id;
    auto at = [this] (const char* id) { return getParameterIndex (id); };
    idx = { at (oscAFreq), at (oscASaw), at (oscAPulse), at (oscAPw),
            at (oscBFreq), at (oscBFine), at (oscBSaw), at (oscBTri), at (oscBPulse), at (oscBPw), at (oscBKbd),
            at (mixOscA), at (mixOscB), at (mixNoise),
            at (fltCutoff), at (fltRes), at (fltEnvAmt), at (fltKbd),
            at (fenvAttack), at (fenvDecay), at (fenvSustain), at (fenvRelease),
            at (aenvAttack), at (aenvDecay), at (aenvSustain), at (aenvRelease),
            at (masterTune), at (masterVolume), at (bendRange), at (perfVoices) };

    seed.store (makeSeed());
    loadGlobalDefaults();

    P5X_LOG (Info, State, instanceId, "P5X %s instance #%d created", JucePlugin_VersionString, (int) instanceId);
    startTimerHz (60);
}

P5XAudioProcessor::~P5XAudioProcessor()
{
    stopTimer();

    for (auto* p : params)
        p->removeListener (this);

    P5X_LOG (Info, State, instanceId, "Instance #%d destroyed", (int) instanceId);
}

juce::RangedAudioParameter* P5XAudioProcessor::getParameterByIndex (int index) const noexcept
{
    return (index >= 0 && index < params::kNumParameters) ? params[(size_t) index] : nullptr;
}

int P5XAudioProcessor::getParameterIndex (const juce::String& paramId) const noexcept
{
    for (size_t i = 0; i < params::kAllIds.size(); ++i)
        if (paramId == params::kAllIds[i])
            return (int) i;

    return -1;
}

//==================================================================================================
namespace
{
std::unique_ptr<juce::dsp::Oversampling<float>> makeOversampler (int maxBlock)
{
    // 00-architecture.md § Oversampling: polyphase IIR, max quality, one channel (mono internally).
    auto os = std::make_unique<juce::dsp::Oversampling<float>> (
        1, 1, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, false);
    os->initProcessing ((size_t) maxBlock);
    return os;
}
} // namespace

int P5XAudioProcessor::measureDownsamplerLatency (int maxBlock) const
{
    // Voices generate at the high rate directly, so only the downsampling half of the oversampler is
    // in the signal path: measure its impulse response peak rather than using the up+down figure.
    auto probe = makeOversampler (maxBlock);
    const int n = juce::jmin (maxBlock, 64);
    std::vector<float> in ((size_t) n, 0.0f), out ((size_t) n, 0.0f);

    const float* inPtr = in.data();
    auto upBlock = probe->processSamplesUp (juce::dsp::AudioBlock<const float> (&inPtr, 1, (size_t) n));
    upBlock.clear();
    upBlock.setSample (0, 0, 1.0f);

    float* outPtr = out.data();
    juce::dsp::AudioBlock<float> outBlock (&outPtr, 1, (size_t) n);
    probe->processSamplesDown (outBlock);

    int peak = 0;

    for (int i = 1; i < n; ++i)
        if (std::abs (out[(size_t) i]) > std::abs (out[(size_t) peak]))
            peak = i;

    return peak;
}

void P5XAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    hostRate = sampleRate > 0.0 ? sampleRate : 48000.0;
    internalRate = hostRate * kOversamplingFactor;

    // Rendering runs in chunks of this size, so any host block size works (00-architecture.md).
    chunkCapacity = juce::jmax (64, samplesPerBlock);
    hostScratch.assign ((size_t) chunkCapacity, 0.0f);
    zeros.assign ((size_t) chunkCapacity, 0.0f);

    for (auto& array : smoothed)
        array.assign ((size_t) (chunkCapacity * kOversamplingFactor), 0.0f);

    downsampler = makeOversampler (chunkCapacity);
    inputUpsampler = inputBusEnabled ? makeOversampler (chunkCapacity) : nullptr;
    setLatencySamples (measureDownsamplerLatency (chunkCapacity));

    const auto voiceSeed = seed.load();

    for (size_t i = 0; i < voices.size(); ++i)
        voices[i].prepare (internalRate, voiceSeed, (int) i);

    allocator.reset();
    heldNotes[0].store (0);
    heldNotes[1].store (0);

    // Smoothing times from 01-parameters.md; they run at the internal rate.
    for (auto* s : { &pwASmoother, &pwBSmoother, &fineBSmoother, &mixASmoother, &mixBSmoother, &mixNoiseSmoother,
                     &resSmoother, &envAmtSmoother, &fSustainSmoother, &aSustainSmoother, &tuneSmoother })
        s->prepare (internalRate, 20.0);

    bendSmoother.prepare (internalRate, 5.0); // 07-midi.md: pitch bend Lin 5 ms
    cutoffSmoother.prepare (internalRate, 20.0);
    volumeSmoother.prepare (hostRate, 20.0);

    readBlockParameters();
    pwASmoother.reset (effectiveValue (idx.oscAPw));
    pwBSmoother.reset (effectiveValue (idx.oscBPw));
    fineBSmoother.reset (effectiveValue (idx.oscBFine));
    mixASmoother.reset (effectiveValue (idx.mixOscA));
    mixBSmoother.reset (effectiveValue (idx.mixOscB));
    mixNoiseSmoother.reset (effectiveValue (idx.mixNoise));
    cutoffSmoother.reset (effectiveValue (idx.fltCutoff));
    resSmoother.reset (effectiveValue (idx.fltRes));
    envAmtSmoother.reset (effectiveValue (idx.fltEnvAmt));
    fSustainSmoother.reset (effectiveValue (idx.fenvSustain));
    aSustainSmoother.reset (effectiveValue (idx.aenvSustain));
    tuneSmoother.reset (effectiveValue (idx.masterTune));
    bendSmoother.reset (bendNormalised * effectiveValue (idx.bendRange));

    const float volumeDb = effectiveValue (idx.masterVolume);
    volumeSmoother.reset (volumeDb <= -60.0f ? 0.0f : juce::Decibels::decibelsToGain (volumeDb));

    const int voiceChoice = juce::jlimit (0, 2, (int) std::lround (effectiveValue (idx.perfVoices)));
    allocator.setActiveVoiceCount (params::kVoiceCounts[(size_t) voiceChoice]);

    testToneRemaining = 0;
    cpuSmoothed = 0.0f;
}

void P5XAudioProcessor::releaseResources()
{
}

bool P5XAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    // Mono internally, duplicated to stereo; stereo output only (00-architecture.md § Oversampling).
    if (layouts.outputBuses.size() != 1 || layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    if (layouts.inputBuses.isEmpty())
        return true;

    // Standalone input bus: mono, stereo or disabled.
    const auto in = layouts.getMainInputChannelSet();
    return inputBusEnabled && layouts.inputBuses.size() == 1
        && (in.isDisabled() || in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo());
}

void P5XAudioProcessor::setRouteInput (bool on)
{
    routeInput.store (on && inputBusEnabled);
    P5X_LOG (Info, Standalone, instanceId, "Route input into filter: %s", on ? "on" : "off");
}

//==================================================================================================
void P5XAudioProcessor::readBlockParameters() noexcept
{
    // 2. Read parameters once per block (with MIDI Learn overrides) and set smoother targets.
    for (int i = 0; i < params::kNumParameters; ++i)
        midiLearn.clearOverrideIfMatched (i, params[(size_t) i]->getValue());

    pwASmoother.setTarget (effectiveValue (idx.oscAPw));
    pwBSmoother.setTarget (effectiveValue (idx.oscBPw));
    fineBSmoother.setTarget (effectiveValue (idx.oscBFine));
    mixASmoother.setTarget (effectiveValue (idx.mixOscA));
    mixBSmoother.setTarget (effectiveValue (idx.mixOscB));
    mixNoiseSmoother.setTarget (effectiveValue (idx.mixNoise));
    cutoffSmoother.setTarget (effectiveValue (idx.fltCutoff));
    resSmoother.setTarget (effectiveValue (idx.fltRes));
    envAmtSmoother.setTarget (effectiveValue (idx.fltEnvAmt));
    fSustainSmoother.setTarget (effectiveValue (idx.fenvSustain));
    aSustainSmoother.setTarget (effectiveValue (idx.aenvSustain));
    tuneSmoother.setTarget (effectiveValue (idx.masterTune));
    bendSmoother.setTarget (bendNormalised * effectiveValue (idx.bendRange));

    const float volumeDb = effectiveValue (idx.masterVolume);
    volumeSmoother.setTarget (volumeDb <= -60.0f ? 0.0f : juce::Decibels::decibelsToGain (volumeDb));

    // Stepped, toggle and choice parameters and envelope times apply without smoothing.
    auto& in = voiceInputs;
    in.oscAFreq = (int) std::lround (effectiveValue (idx.oscAFreq));
    in.oscBFreq = (int) std::lround (effectiveValue (idx.oscBFreq));
    in.shapesA = { effectiveBool (idx.oscASaw), false, effectiveBool (idx.oscAPulse) };
    in.shapesB = { effectiveBool (idx.oscBSaw), effectiveBool (idx.oscBTri), effectiveBool (idx.oscBPulse) };
    in.oscBKeyboard = effectiveBool (idx.oscBKbd);
    in.keyboardTrack = 0.5f * (float) juce::jlimit (0, 2, (int) std::lround (effectiveValue (idx.fltKbd)));
    in.filterEnv = { effectiveValue (idx.fenvAttack), effectiveValue (idx.fenvDecay), 0.0f, effectiveValue (idx.fenvRelease) };
    in.ampEnv = { effectiveValue (idx.aenvAttack), effectiveValue (idx.aenvDecay), 0.0f, effectiveValue (idx.aenvRelease) };
}

void P5XAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    const auto startTicks = juce::Time::getHighResolutionTicks();
    const int numSamples = buffer.getNumSamples();

    // The input (Standalone only) arrives in the same buffer; render() reads it chunk by chunk
    // before writing the output over it.
    const bool routing = routeInput.load (std::memory_order_relaxed) && getTotalNumInputChannels() > 0
                      && inputUpsampler != nullptr;

    if (downsampler == nullptr)
    {
        buffer.clear();
        return;
    }

    midiHandler.setChannel (midiChannel.load (std::memory_order_relaxed));

    if (panicRequested.exchange (false))
    {
        // Panic = All Sound Off + clear sustain + clear note stack (07-midi.md).
        allSoundOff();
        allocator.clearSustainState();
        heldNotes[0].store (0);
        heldNotes[1].store (0);
        P5X_LOG (Info, Engine, instanceId, "Panic");
    }

    if (testToneRequested.exchange (false))
    {
        testToneRemaining = (int) (2.0 * hostRate);
        testTonePhase = 0.0;
    }

    readBlockParameters();
    applyVoiceCountIfIdle();

    // 3. Events: on-screen/computer keyboard first (at sample 0), then the host's, sample-accurately.
    {
        const auto scope = injectFifo.read (injectFifo.getNumReady());

        for (int i = 0; i < scope.blockSize1; ++i)
        {
            const auto& m = injectBuffer[(size_t) (scope.startIndex1 + i)];
            handleMidiEvent (m.data(), messageSize (m[0]));
        }

        for (int i = 0; i < scope.blockSize2; ++i)
        {
            const auto& m = injectBuffer[(size_t) (scope.startIndex2 + i)];
            handleMidiEvent (m.data(), messageSize (m[0]));
        }
    }

    int rendered = 0;

    for (const auto metadata : midiMessages)
    {
        const int position = juce::jlimit (0, numSamples, metadata.samplePosition);

        if (position > rendered)
        {
            render (buffer, rendered, position, routing);
            rendered = position;
        }

        handleMidiEvent (metadata.data, metadata.numBytes);
    }

    render (buffer, rendered, numSamples, routing);
    midiMessages.clear();

    pushTelemetry (numSamples, juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - startTicks));
}

void P5XAudioProcessor::render (juce::AudioBuffer<float>& buffer, int start, int end, bool routing)
{
    const int numChannels = buffer.getNumChannels();
    int position = start;

    while (position < end)
    {
        const int n = juce::jmin (end - position, chunkCapacity);
        const int internalN = n * kOversamplingFactor;

        // Standalone input: upsample the first input channel to the internal rate (read before
        // the output overwrites the buffer).
        voiceInputs.externalInput = nullptr;

        if (routing)
        {
            const float* inputChannel = buffer.getReadPointer (0, position);
            auto upsampled = inputUpsampler->processSamplesUp (juce::dsp::AudioBlock<const float> (&inputChannel, 1, (size_t) n));
            voiceInputs.externalInput = upsampled.getChannelPointer (0);
        }

        // Per-internal-sample parameter values.
        for (int i = 0; i < internalN; ++i)
        {
            const auto s = (size_t) i;
            smoothed[sPitchOffset][s] = bendSmoother.next() + tuneSmoother.next() * 0.01f;
            smoothed[sPwA][s] = pwASmoother.next();
            smoothed[sPwB][s] = pwBSmoother.next();
            smoothed[sFineB][s] = fineBSmoother.next();
            smoothed[sMixA][s] = mixASmoother.next();
            smoothed[sMixB][s] = mixBSmoother.next();
            smoothed[sMixNoise][s] = mixNoiseSmoother.next();
            smoothed[sCutoff][s] = cutoffSmoother.next();
            smoothed[sResonance][s] = resSmoother.next();
            smoothed[sEnvAmount][s] = envAmtSmoother.next();
            smoothed[sFilterSustain][s] = fSustainSmoother.next();
            smoothed[sAmpSustain][s] = aSustainSmoother.next();
        }

        auto& in = voiceInputs;
        in.pitchOffset = smoothed[sPitchOffset].data();
        in.pwA = smoothed[sPwA].data();
        in.pwB = smoothed[sPwB].data();
        in.fineB = smoothed[sFineB].data();
        in.mixA = smoothed[sMixA].data();
        in.mixB = smoothed[sMixB].data();
        in.mixNoise = smoothed[sMixNoise].data();
        in.cutoffHz = smoothed[sCutoff].data();
        in.resonance = smoothed[sResonance].data();
        in.envAmount = smoothed[sEnvAmount].data();
        in.filterSustain = smoothed[sFilterSustain].data();
        in.ampSustain = smoothed[sAmpSustain].data();

        // Voices render straight into the oversampler's high-rate buffer; one downsampler for all.
        const float* zeroPtr = zeros.data();
        auto highRate = downsampler->processSamplesUp (juce::dsp::AudioBlock<const float> (&zeroPtr, 1, (size_t) n));
        float* mixBus = highRate.getChannelPointer (0);
        std::fill (mixBus, mixBus + internalN, 0.0f);

        for (auto& v : voices)
            v.render (mixBus, in, internalN);

        float* hostPtr = hostScratch.data();
        juce::dsp::AudioBlock<float> hostBlock (&hostPtr, 1, (size_t) n);
        downsampler->processSamplesDown (hostBlock);

        // Output stage (06-voices.md): master volume, then the safety clipper.
        for (int i = 0; i < n; ++i)
            hostScratch[(size_t) i] *= volumeSmoother.next();

        if (dsp::SafetyClipper::processBlock (hostScratch.data(), n))
            P5X_LOG_RATE (Warn, 1, Engine, instanceId, "Safety clipper engaged (output above -1.9 dBFS)");

        if (testToneRemaining > 0)
        {
            const double increment = 440.0 / hostRate;
            const int toneSamples = juce::jmin (n, testToneRemaining);

            for (int i = 0; i < toneSamples; ++i)
            {
                hostScratch[(size_t) i] += kTestToneGain * (float) std::sin (juce::MathConstants<double>::twoPi * testTonePhase);
                testTonePhase += increment;
                testTonePhase -= std::floor (testTonePhase);
            }

            testToneRemaining -= toneSamples;
        }

        // Mono → every output channel.
        for (int ch = 0; ch < numChannels; ++ch)
            std::copy (hostScratch.begin(), hostScratch.begin() + n, buffer.getWritePointer (ch, position));

        position += n;
    }
}

void P5XAudioProcessor::pushTelemetry (int numSamples, double elapsed)
{
    // CPU: processBlock time / block duration, smoothed over 300 ms (10-debug-and-harness.md).
    if (numSamples > 0)
    {
        const double blockSeconds = numSamples / hostRate;
        const float load = (float) (100.0 * elapsed / blockSeconds);
        const float alpha = (float) (1.0 - std::exp (-blockSeconds / 0.3));
        cpuSmoothed += alpha * (load - cpuSmoothed);

        if (elapsed > blockSeconds)
            ++xruns;
    }

    debug::TelemetrySnapshot snapshot;
    snapshot.cpuPercent = cpuSmoothed;
    snapshot.sampleRate = hostRate;
    snapshot.blockSize = numSamples;
    snapshot.oversampling = kOversamplingFactor;
    snapshot.xruns = xruns;
    snapshot.voiceCount = allocator.getActiveVoiceCount();

    for (int i = 0; i < kMaxVoices; ++i)
    {
        const auto& v = voices[(size_t) i];
        auto& t = snapshot.voices[(size_t) i];

        if (v.isIdle())
        {
            t.state = debug::VoiceTelemetry::State::Idle;
            continue;
        }

        ++snapshot.activeVoices;
        t.state = allocator.isSustained (i) ? debug::VoiceTelemetry::State::Sustained
                : v.isReleasing()            ? debug::VoiceTelemetry::State::Release
                                             : debug::VoiceTelemetry::State::On;
        t.ampStage = (uint8_t) v.getAmpStage();
        t.note = (int8_t) v.getNote();
        t.velocity = v.getVelocity();
        t.oscAHz = v.getOscAHz();
        t.cutoffHz = v.getCutoffHz();
        t.ampLevel = v.getAmpLevel();
    }

    telemetry.write (snapshot);
}

void P5XAudioProcessor::handleMidiEvent (const uint8_t* data, int size)
{
    const auto result = midiHandler.handle (data, size, *this);

    midi::MonitorEvent event;
    event.timeMs = juce::Time::getMillisecondCounterHiRes();
    event.size = (uint8_t) juce::jlimit (0, 3, size);

    for (int i = 0; i < event.size; ++i)
        event.bytes[i] = data[i];

    event.dimmed = result == midi::MidiHandler::Result::WrongChannel || result == midi::MidiHandler::Result::OutOfRange;
    monitorFifo.push (event);

    if (result != midi::MidiHandler::Result::WrongChannel)
        midiActivity.fetch_add (1, std::memory_order_relaxed);

    if (result == midi::MidiHandler::Result::OutOfRange)
        P5X_LOG_RATE (Debug, 10, Midi, instanceId, "Note %d is outside 36-96, ignored", size > 1 ? (int) data[1] : -1);
}

float P5XAudioProcessor::getNormalised (int paramIndex) const noexcept
{
    const float override = midiLearn.getOverride (paramIndex);
    return std::isnan (override) ? params[(size_t) paramIndex]->getValue() : override;
}

float P5XAudioProcessor::effectiveValue (int paramIndex) noexcept
{
    return params[(size_t) paramIndex]->convertFrom0to1 (getNormalised (paramIndex));
}

void P5XAudioProcessor::applyVoiceCountIfIdle()
{
    const int choice = juce::jlimit (0, 2, (int) std::lround (effectiveValue (idx.perfVoices)));
    const int wanted = params::kVoiceCounts[(size_t) choice];

    // 06-voices.md § Voice count: a new count applies when all voices are idle.
    if (wanted != allocator.getActiveVoiceCount() && allocator.allVoicesIdle())
    {
        allocator.setActiveVoiceCount (wanted);
        P5X_LOG (Info, Engine, instanceId, "Voice count set to %d", wanted);
    }
}

void P5XAudioProcessor::setHeld (int note, bool held) noexcept
{
    if (note < 0 || note > 127)
        return;

    const uint64_t bit = 1ull << (note & 63);
    auto& word = heldNotes[(size_t) (note >> 6)];

    if (held)
        word.fetch_or (bit, std::memory_order_relaxed);
    else
        word.fetch_and (~bit, std::memory_order_relaxed);
}

bool P5XAudioProcessor::isNoteHeld (int note) const noexcept
{
    if (note < 0 || note > 127)
        return false;

    return (heldNotes[(size_t) (note >> 6)].load (std::memory_order_relaxed) >> (note & 63)) & 1u;
}

//==================================================================================================
// MidiSink (audio thread)
void P5XAudioProcessor::noteOn (int note, int velocity)
{
    const auto result = allocator.noteOn (note, (float) velocity / 127.0f);
    voicePressure[(size_t) result.voice] = 0.0f;
    setHeld (note, true);

    if (result.stolen)
        P5X_LOG_RATE (Warn, 10, Engine, instanceId, "Voice %d stolen: note %d -> %d", result.voice + 1,
                      result.stolenNote, note);
}

void P5XAudioProcessor::noteOff (int note)
{
    allocator.noteOff (note);
    setHeld (note, false);
}

void P5XAudioProcessor::sustainPedal (bool down)
{
    allocator.setSustainPedal (down);
}

void P5XAudioProcessor::allSoundOff()
{
    allocator.allSoundOff();

    // Hard cut (07-midi.md): also clear the downsampler so its filter tail doesn't ring on.
    if (downsampler != nullptr)
        downsampler->reset();
}

void P5XAudioProcessor::allNotesOff()
{
    allocator.allNotesOff();
    heldNotes[0].store (0);
    heldNotes[1].store (0);
}

void P5XAudioProcessor::resetAllControllers()
{
    modWheelValue = 0.0f;
    channelAftertouch = 0.0f;
    voicePressure.fill (0.0f);
    bendNormalised = 0.0f;
    bendSmoother.setTarget (0.0f);
    allocator.setSustainPedal (false);
}

void P5XAudioProcessor::pitchBend (int value14)
{
    // 14-bit, centred at 8192, scaled to ± bend_range semitones (07-midi.md).
    bendNormalised = value14 >= 8192 ? (float) (value14 - 8192) / 8191.0f : (float) (value14 - 8192) / 8192.0f;
    bendSmoother.setTarget (bendNormalised * effectiveValue (idx.bendRange));
}

void P5XAudioProcessor::modWheel (int value)
{
    modWheelValue = (float) value / 127.0f; // Wheel-Mod amount, used from milestone 3
}

void P5XAudioProcessor::channelPressure (int value)
{
    channelAftertouch = (float) value / 127.0f; // used from milestone 4
}

void P5XAudioProcessor::polyPressure (int note, int value)
{
    if (const int voice = allocator.findVoicePlayingNote (note); voice >= 0)
        voicePressure[(size_t) voice] = (float) value / 127.0f;
}

void P5XAudioProcessor::controlChange (int cc, int value)
{
    const auto result = midiLearn.handleControlChange (cc, value, *this);

    switch (result.outcome)
    {
        case midi::MidiLearn::Outcome::ReservedWhileLearning:
            P5X_LOG_RATE (Warn, 2, Learn, instanceId, "CC %d is reserved (%s)", cc, midi::MidiLearn::reservedName (cc));
            break;

        case midi::MidiLearn::Outcome::PickupCaught:
            P5X_LOG_RATE (Debug, 10, Learn, instanceId, "Pickup caught: CC %d", cc);
            break;

        default:
            break;
    }
}

void P5XAudioProcessor::programChange (int program)
{
    if (programChangeEnabled.load (std::memory_order_relaxed))
        P5X_LOG (Info, Midi, instanceId, "Program change %d received (presets arrive in milestone 6)", program);
}

//==================================================================================================
void P5XAudioProcessor::parameterValueChanged (int parameterIndex, float newValue)
{
    if (parameterIndex >= 0 && parameterIndex < params::kNumParameters)
        midiLearn.parameterChangedExternally (parameterIndex, newValue);
}

void P5XAudioProcessor::serviceMessageThread()
{
    midi::MidiLearn::LearnCommit commit;

    while (midiLearn.popLearnCommit (commit))
    {
        const auto name = params[(size_t) commit.paramIndex]->getName (64);

        if (commit.previousParamForCc >= 0)
            P5X_LOG (Warn, Learn, instanceId, "CC %d moved: %s -> %s", commit.cc,
                     params[(size_t) commit.previousParamForCc]->getName (64).toRawUTF8(), name.toRawUTF8());

        P5X_LOG (Info, Learn, instanceId, "%s <- CC %d", name.toRawUTF8(), commit.cc);

        if (onLearnCommitted)
            onLearnCommitted (commit);
    }

    // Host path of a learned CC (07-midi.md § Applying a learned CC): never from the audio thread.
    midi::MidiLearn::HostUpdate update;

    while (midiLearn.popHostUpdate (update))
    {
        auto* p = params[(size_t) update.paramIndex];
        p->beginChangeGesture();
        p->setValueNotifyingHost (update.value);
        p->endChangeGesture();
        midiLearn.hostUpdateApplied (update.paramIndex);
    }

    midi::MonitorEvent event;

    while (monitorFifo.pop (event))
    {
        monitorHistory.push_back (event);
        ++monitorEventCount;

        if ((int) monitorHistory.size() > kMonitorHistory)
            monitorHistory.pop_front();
    }
}

//==================================================================================================
debug::TelemetrySnapshot P5XAudioProcessor::getTelemetry()
{
    telemetry.read (lastTelemetry);
    return lastTelemetry;
}

void P5XAudioProcessor::injectMidi (const juce::MidiMessage& message)
{
    if (message.getRawDataSize() < 1 || message.getRawDataSize() > 3)
        return;

    const auto scope = injectFifo.write (1);

    if (scope.blockSize1 == 0)
        return;

    auto& slot = injectBuffer[(size_t) scope.startIndex1];
    slot = {};
    std::copy_n (message.getRawData(), message.getRawDataSize(), slot.begin());
}

void P5XAudioProcessor::injectNoteOn (int note, int velocity)
{
    injectMidi (juce::MidiMessage::noteOn (juce::jmax (1, midiChannel.load()), note, (juce::uint8) velocity));
}

void P5XAudioProcessor::injectNoteOff (int note)
{
    injectMidi (juce::MidiMessage::noteOff (juce::jmax (1, midiChannel.load()), note));
}

void P5XAudioProcessor::setMidiChannel (int channel)
{
    midiChannel.store (juce::jlimit (0, 16, channel));
    P5X_LOG (Info, Midi, instanceId, "MIDI channel: %s", channel == 0 ? "Omni" : juce::String (channel).toRawUTF8());
}

//==================================================================================================
// State (09-presets-and-state.md)
std::unique_ptr<juce::XmlElement> P5XAudioProcessor::createSettingsElement (bool includeScale) const
{
    auto settings = std::make_unique<juce::XmlElement> ("Settings");
    settings->setAttribute ("midiChannel", midiChannel.load());
    settings->setAttribute ("takeover", midiLearn.getTakeover() == midi::Takeover::Jump ? "jump" : "pickup");
    settings->setAttribute ("hq", hqMode.load() ? 1 : 0);
    settings->setAttribute ("programChange", programChangeEnabled.load() ? 1 : 0);

    if (includeScale)
        settings->setAttribute ("scale", (double) windowScale.load());

    return settings;
}

void P5XAudioProcessor::readSettingsElement (const juce::XmlElement& settings, bool includeScale)
{
    if (settings.hasAttribute ("midiChannel"))
        midiChannel.store (juce::jlimit (0, 16, settings.getIntAttribute ("midiChannel")));

    if (settings.hasAttribute ("takeover"))
        midiLearn.setTakeover (settings.getStringAttribute ("takeover").equalsIgnoreCase ("jump")
                                   ? midi::Takeover::Jump
                                   : midi::Takeover::Pickup);

    hqMode.store (settings.getBoolAttribute ("hq", hqMode.load()));
    programChangeEnabled.store (settings.getBoolAttribute ("programChange", programChangeEnabled.load()));

    if (includeScale && settings.hasAttribute ("scale"))
    {
        const auto scale = settings.getDoubleAttribute ("scale", 1.0);
        windowScale.store ((float) juce::jlimit (0.75, 1.5, std::isfinite (scale) ? scale : 1.0));
    }
}

void P5XAudioProcessor::loadGlobalDefaults()
{
    if (auto settings = juce::parseXML (paths::settingsFile()); settings != nullptr && settings->hasTagName ("Settings"))
        readSettingsElement (*settings, false);

    // The default map applies to new instances; a host state with <MidiMap> replaces it (07-midi.md).
    if (auto map = juce::parseXML (paths::midiMapFile()); map != nullptr && map->hasTagName ("MidiMap"))
        midiLearn.loadXml (*map);
}

void P5XAudioProcessor::saveSettingsAsDefault()
{
    paths::appDataDir().createDirectory();

    if (createSettingsElement (false)->writeTo (paths::settingsFile()))
        P5X_LOG (Debug, State, instanceId, "Settings saved as defaults");
    else
        P5X_LOG (Error, State, instanceId, "Couldn't write settings.xml");
}

bool P5XAudioProcessor::saveMidiMapAsDefault()
{
    paths::appDataDir().createDirectory();
    const bool ok = midiLearn.createXml()->writeTo (paths::midiMapFile());

    if (ok)
        P5X_LOG (Info, Learn, instanceId, "MIDI map saved as default (%d mappings)", midiLearn.getNumMappings());
    else
        P5X_LOG (Error, Learn, instanceId, "Couldn't write midi-map.xml");

    return ok;
}

void P5XAudioProcessor::clearMidiMap()
{
    midiLearn.clearAll();
    P5X_LOG (Info, Learn, instanceId, "All MIDI mappings cleared");
}

void P5XAudioProcessor::resetToDefaults()
{
    for (auto* p : params)
        p->setValueNotifyingHost (p->getDefaultValue());

    presetName = "Init";
    presetIndex = 0;
    presetEdited = false;
    midiLearn.armAllPickups();
}

void P5XAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::XmlElement root ("P5XState");
    root.setAttribute ("version", kStateVersion);

    if (auto parameters = apvts.copyState().createXml())
        root.addChildElement (parameters.release());

    auto* preset = root.createNewChildElement ("Preset");
    preset->setAttribute ("name", presetName);
    preset->setAttribute ("index", presetIndex);
    preset->setAttribute ("edited", presetEdited ? 1 : 0);

    root.addChildElement (midiLearn.createXml().release());
    root.addChildElement (createSettingsElement (true).release());
    root.createNewChildElement ("Seed")->setAttribute ("value", juce::String ((juce::uint64) seed.load()));

    copyXmlToBinary (root, destData);
}

void P5XAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // Tolerant: unknown content ignored, missing values take defaults, invalid values clamped;
    // never crash on bad data (09-presets-and-state.md § Plugin state format).
    const auto xml = (data != nullptr && sizeInBytes > 0) ? getXmlFromBinary (data, sizeInBytes) : nullptr;

    if (xml == nullptr || ! xml->hasTagName ("P5XState"))
    {
        P5X_LOG (Warn, State, instanceId, "State unreadable; loading defaults");
        resetToDefaults();
        return;
    }

    const auto versionText = xml->getStringAttribute ("version").trim();
    const int version = (versionText.isNotEmpty() && versionText.containsOnly ("0123456789")) ? versionText.getIntValue() : 0;

    if (version < 1)
    {
        P5X_LOG (Warn, State, instanceId, "State has a missing or invalid version; loading defaults");
        resetToDefaults();
        return;
    }

    if (version > kStateVersion)
        P5X_LOG (Warn, State, instanceId, "State version %d is newer than this build (%d); loading what it can",
                 version, kStateVersion);

    const auto* parameters = xml->getChildByName (apvts.state.getType());

    for (size_t i = 0; i < params.size(); ++i)
    {
        auto* p = params[i];
        float normalised = p->getDefaultValue();

        if (parameters != nullptr)
        {
            for (const auto* child : parameters->getChildIterator())
            {
                if (child->getStringAttribute ("id") != params::kAllIds[i])
                    continue;

                if (const auto text = child->getStringAttribute ("value"); isNumber (text))
                {
                    const auto& range = p->getNormalisableRange();
                    const float real = juce::jlimit (range.start, range.end, (float) text.getDoubleValue());
                    normalised = p->convertTo0to1 (real);
                }

                break;
            }
        }

        p->setValueNotifyingHost (normalised);
    }

    if (const auto* preset = xml->getChildByName ("Preset"))
    {
        presetName = preset->getStringAttribute ("name", "Init").substring (0, 64);
        presetIndex = juce::jmax (0, preset->getIntAttribute ("index", 0));
        presetEdited = preset->getBoolAttribute ("edited", false);
    }

    if (const auto* map = xml->getChildByName ("MidiMap"))
        midiLearn.loadXml (*map);

    if (const auto* settings = xml->getChildByName ("Settings"))
        readSettingsElement (*settings, true);

    if (const auto* seedElement = xml->getChildByName ("Seed"))
    {
        const auto text = seedElement->getStringAttribute ("value").trim();

        if (text.isNotEmpty() && text.length() <= 20 && text.containsOnly ("0123456789"))
            seed.store (std::strtoull (text.toRawUTF8(), nullptr, 10));
    }

    midiLearn.armAllPickups();
    P5X_LOG (Info, State, instanceId, "State loaded (%d MIDI mappings)", midiLearn.getNumMappings());
}

//==================================================================================================
juce::AudioProcessorEditor* P5XAudioProcessor::createEditor()
{
    return new P5XAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new P5XAudioProcessor();
}
