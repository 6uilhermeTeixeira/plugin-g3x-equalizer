#include "dsp/Q10Processor.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace g3x::q10 {
namespace {
double finiteOr(double value, double fallback) noexcept {
  return std::isfinite(value) ? value : fallback;
}
}

void Q10Processor::prepare(double sampleRate, std::size_t channels, double rampSeconds) noexcept {
  sampleRate_ = std::clamp(finiteOr(sampleRate, 48000.0), 8000.0, 384000.0);
  channels_ = std::clamp<std::size_t>(channels, 1, maximumChannels);
  const auto ramp = std::clamp(finiteOr(rampSeconds, 0.02), 0.0, 1.0);
  rampSamples_ = std::max<std::size_t>(1, static_cast<std::size_t>(sampleRate_ * ramp));
  inputGainDb_.current = inputGainDb_.target;
  outputGainDb_.current = outputGainDb_.target;
  bypassMix_.current = bypassMix_.target;
  inputGainDb_.remaining = outputGainDb_.remaining = bypassMix_.remaining = 0;
  for (auto& channel : bands_)
    for (auto& runtime : channel) {
      runtime.target = sanitizeBand(runtime.target, sampleRate_);
      runtime.frequency = {runtime.target.frequencyHz, runtime.target.frequencyHz, 0.0, 0};
      runtime.gain = {runtime.target.gainDb, runtime.target.gainDb, 0.0, 0};
      runtime.q = {runtime.target.q, runtime.target.q, 0.0, 0};
      const auto enabled = runtime.target.enabled ? 1.0 : 0.0;
      runtime.enabled = {enabled, enabled, 0.0, 0};
      runtime.primaryType = runtime.target.type;
      runtime.secondaryType = runtime.target.type;
      runtime.typeBlend = {};
      runtime.typeTransition = false;
      runtime.primary.configure({runtime.primaryType, runtime.frequency.current,
        runtime.gain.current, runtime.q.current, sampleRate_});
      runtime.secondary.configure({runtime.secondaryType, runtime.frequency.current,
        runtime.gain.current, runtime.q.current, sampleRate_});
    }
  reset();
}

void Q10Processor::reset() noexcept {
  for (auto& channel : bands_)
    for (auto& runtime : channel) { runtime.primary.reset(); runtime.secondary.reset(); }
}

void Q10Processor::setStereoLinked(bool linked) noexcept {
  if (stereoLinked_ == linked) return;
  stereoLinked_ = linked;
  if (linked)
    for (std::size_t index = 0; index < bandCount; ++index)
      applyBand(bands_[1][index], bands_[0][index].target);
}

void Q10Processor::setBand(std::size_t index, BandSettings settings) noexcept {
  if (index >= bandCount) return;
  applyBand(bands_[0][index], settings);
  if (stereoLinked_) applyBand(bands_[1][index], settings);
}

void Q10Processor::setBand(std::size_t channel, std::size_t index,
    BandSettings settings) noexcept {
  if (channel >= maximumChannels || index >= bandCount) return;
  applyBand(bands_[channel][index], settings);
  if (stereoLinked_)
    applyBand(bands_[channel == 0 ? 1 : 0][index], settings);
}

BandSettings Q10Processor::band(std::size_t channel, std::size_t index) const noexcept {
  return channel < maximumChannels && index < bandCount ? bands_[channel][index].target
                                                        : BandSettings{};
}

void Q10Processor::setInputGainDb(double decibels) noexcept {
  setSmoothed(inputGainDb_, sanitizeGain(decibels));
}
void Q10Processor::setOutputGainDb(double decibels) noexcept {
  setSmoothed(outputGainDb_, sanitizeGain(decibels));
}
void Q10Processor::setBypassed(bool bypassed) noexcept {
  setSmoothed(bypassMix_, bypassed ? 1.0 : 0.0);
}

double Q10Processor::sanitizeGain(double value) noexcept {
  return std::clamp(finiteOr(value, 0.0), -24.0, 12.0);
}

BandSettings Q10Processor::sanitizeBand(BandSettings settings, double sampleRate) noexcept {
  const auto rate = std::clamp(finiteOr(sampleRate, 48000.0), 8000.0, 384000.0);
  settings.frequencyHz = std::clamp(finiteOr(settings.frequencyHz, 1000.0), 10.0,
    std::min(30000.0, 0.475 * rate));
  settings.gainDb = std::clamp(finiteOr(settings.gainDb, 0.0), -24.0, 24.0);
  settings.q = std::clamp(finiteOr(settings.q, 0.7071067811865476), 0.1, 100.0);
  return settings;
}

void Q10Processor::setSmoothed(SmoothedValue& value, double target) noexcept {
  if (target == value.target) return;
  value.target = target;
  value.remaining = rampSamples_;
  value.step = (value.target - value.current) / static_cast<double>(value.remaining);
}

double Q10Processor::advance(SmoothedValue& value) noexcept {
  if (value.remaining != 0) {
    value.current += value.step;
    if (--value.remaining == 0) value.current = value.target;
  }
  return value.current;
}

void Q10Processor::applyBand(BandRuntime& runtime, BandSettings settings) noexcept {
  settings = sanitizeBand(settings, sampleRate_);
  if (settings.type != runtime.target.type) {
    runtime.secondaryType = settings.type;
    runtime.secondary.reset();
    runtime.typeBlend = {0.0, 1.0, 1.0 / static_cast<double>(rampSamples_), rampSamples_};
    runtime.typeTransition = true;
  }
  runtime.target = settings;
  setSmoothed(runtime.frequency, settings.frequencyHz);
  setSmoothed(runtime.gain, settings.gainDb);
  setSmoothed(runtime.q, settings.q);
  setSmoothed(runtime.enabled, settings.enabled ? 1.0 : 0.0);
}

double Q10Processor::processBand(BandRuntime& runtime, double input) noexcept {
  const auto frequency = advance(runtime.frequency);
  const auto gain = advance(runtime.gain);
  const auto q = advance(runtime.q);
  runtime.primary.configure({runtime.primaryType, frequency, gain, q, sampleRate_});
  const auto primary = runtime.primary.processSample(input);
  auto filtered = primary;
  if (runtime.typeTransition) {
    runtime.secondary.configure({runtime.secondaryType, frequency, gain, q, sampleRate_});
    const auto secondary = runtime.secondary.processSample(input);
    const auto blend = advance(runtime.typeBlend);
    filtered = primary + blend * (secondary - primary);
    if (runtime.typeBlend.remaining == 0) {
      runtime.primary = runtime.secondary;
      runtime.primaryType = runtime.secondaryType;
      runtime.typeTransition = false;
    }
  }
  const auto enabled = advance(runtime.enabled);
  return input + enabled * (filtered - input);
}

template <typename Sample>
void Q10Processor::processTyped(Sample* const* channels, std::size_t channelCount,
    std::size_t sampleCount) noexcept {
  if (channels == nullptr) return;
  channelCount = std::min({channelCount, channels_, maximumChannels});
  for (std::size_t sample = 0; sample < sampleCount; ++sample) {
    const auto inputGain = std::pow(10.0, advance(inputGainDb_) / 20.0);
    const auto outputGain = std::pow(10.0, advance(outputGainDb_) / 20.0);
    const auto bypass = advance(bypassMix_);
    for (std::size_t channel = 0; channel < channelCount; ++channel) {
      if (channels[channel] == nullptr) continue;
      const auto dry = finiteOr(static_cast<double>(channels[channel][sample]), 0.0);
      auto wet = dry * inputGain;
      for (auto& runtime : bands_[channel]) wet = processBand(runtime, wet);
      wet *= outputGain;
      const auto output = wet + bypass * (dry - wet);
      const auto limit = static_cast<double>(std::numeric_limits<Sample>::max());
      channels[channel][sample] = std::isfinite(output) && std::abs(output) <= limit
        ? static_cast<Sample>(output) : static_cast<Sample>(0);
    }
  }
}

void Q10Processor::process(double* const* channels, std::size_t channelCount,
    std::size_t sampleCount) noexcept { processTyped(channels, channelCount, sampleCount); }

void Q10Processor::process(float* const* channels, std::size_t channelCount,
    std::size_t sampleCount) noexcept { processTyped(channels, channelCount, sampleCount); }

}
