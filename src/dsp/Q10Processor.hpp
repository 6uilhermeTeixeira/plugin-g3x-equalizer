#pragma once

#include <array>
#include <cstddef>

#include "dsp/BiquadFilter.hpp"

namespace g3x::q10 {

constexpr std::size_t bandCount = 10;
constexpr std::size_t maximumChannels = 2;

struct BandSettings {
  bool enabled{};
  FilterType type{FilterType::bell};
  double frequencyHz{1000.0};
  double gainDb{};
  double q{0.7071067811865476};
};

class Q10Processor {
public:
  void prepare(double sampleRate, std::size_t channels, double rampSeconds = 0.02) noexcept;
  void reset() noexcept;
  void setStereoLinked(bool linked) noexcept;
  [[nodiscard]] bool stereoLinked() const noexcept { return stereoLinked_; }
  void setBand(std::size_t band, BandSettings settings) noexcept;
  void setBand(std::size_t channel, std::size_t band, BandSettings settings) noexcept;
  [[nodiscard]] BandSettings band(std::size_t channel, std::size_t band) const noexcept;
  void setInputGainDb(double decibels) noexcept;
  void setOutputGainDb(double decibels) noexcept;
  void setBypassed(bool bypassed) noexcept;
  [[nodiscard]] std::size_t latencySamples() const noexcept { return 0; }
  void process(double* const* channels, std::size_t channelCount,
    std::size_t sampleCount) noexcept;
  void process(float* const* channels, std::size_t channelCount,
    std::size_t sampleCount) noexcept;

private:
  struct SmoothedValue {
    double current{};
    double target{};
    double step{};
    std::size_t remaining{};
  };
  struct BandRuntime {
    BandSettings target;
    SmoothedValue frequency{1000.0, 1000.0, 0.0, 0};
    SmoothedValue gain;
    SmoothedValue q{0.7071067811865476, 0.7071067811865476, 0.0, 0};
    SmoothedValue enabled;
    BiquadFilter primary;
    BiquadFilter secondary;
    FilterType primaryType{FilterType::bell};
    FilterType secondaryType{FilterType::bell};
    SmoothedValue typeBlend;
    bool typeTransition{};
  };

  static double sanitizeGain(double value) noexcept;
  static BandSettings sanitizeBand(BandSettings settings, double sampleRate) noexcept;
  void setSmoothed(SmoothedValue& value, double target) noexcept;
  static double advance(SmoothedValue& value) noexcept;
  void applyBand(BandRuntime& runtime, BandSettings settings) noexcept;
  double processBand(BandRuntime& runtime, double input) noexcept;
  template <typename Sample>
  void processTyped(Sample* const* channels, std::size_t channelCount,
    std::size_t sampleCount) noexcept;

  double sampleRate_{48000.0};
  std::size_t channels_{2};
  std::size_t rampSamples_{960};
  bool stereoLinked_{true};
  SmoothedValue inputGainDb_;
  SmoothedValue outputGainDb_;
  SmoothedValue bypassMix_;
  std::array<std::array<BandRuntime, bandCount>, maximumChannels> bands_;
};

}
