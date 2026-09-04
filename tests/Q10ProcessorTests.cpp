#include "dsp/Q10Processor.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string_view>
#include <vector>

namespace {
void require(bool condition, std::string_view message) {
  if (!condition) {
    std::cerr << "FAILED: " << message << '\n';
    std::exit(EXIT_FAILURE);
  }
}

void testFlatChainIsExactIdentity() {
  g3x::q10::Q10Processor processor;
  processor.prepare(48000.0, 2);
  std::array<double, 128> left;
  std::array<double, 128> right;
  for (std::size_t i = 0; i < left.size(); ++i) {
    left[i] = std::sin(static_cast<double>(i) * 0.17);
    right[i] = std::cos(static_cast<double>(i) * 0.11);
  }
  const auto expectedLeft = left;
  const auto expectedRight = right;
  std::array<double*, 2> channels{left.data(), right.data()};
  processor.process(channels.data(), channels.size(), left.size());
  require(left == expectedLeft && right == expectedRight,
    "disabled ten-band chain must be bit-exact identity");
  require(processor.latencySamples() == 0, "processor must report zero latency");
}

void testTenBandsRemainFinite() {
  g3x::q10::Q10Processor processor;
  processor.prepare(192000.0, 2, 0.0);
  constexpr std::array types{g3x::q10::FilterType::bell,
    g3x::q10::FilterType::adaptiveBell, g3x::q10::FilterType::lowShelf,
    g3x::q10::FilterType::highShelf, g3x::q10::FilterType::highPass,
    g3x::q10::FilterType::lowPass};
  for (std::size_t band = 0; band < g3x::q10::bandCount; ++band)
    processor.setBand(band, {true, types[band % types.size()],
      20.0 * std::pow(2.0, static_cast<double>(band)), band % 2 == 0 ? 24.0 : -24.0,
      band % 2 == 0 ? 0.1 : 100.0});
  std::array<double, 4096> left{};
  std::array<double, 4096> right{};
  left[0] = 1.0; right[0] = -1.0;
  std::array<double*, 2> channels{left.data(), right.data()};
  processor.process(channels.data(), channels.size(), left.size());
  require(std::all_of(left.begin(), left.end(), [](double value) { return std::isfinite(value); }),
    "ten extreme bands must keep left output finite");
  require(std::all_of(right.begin(), right.end(), [](double value) { return std::isfinite(value); }),
    "ten extreme bands must keep right output finite");
}

void testLinkedStereoHasParity() {
  g3x::q10::Q10Processor processor;
  processor.prepare(48000.0, 2, 0.0);
  processor.setBand(3, {true, g3x::q10::FilterType::bell, 1800.0, 12.0, 3.0});
  std::array<double, 512> left{};
  std::array<double, 512> right{};
  for (std::size_t i = 0; i < left.size(); ++i) left[i] = right[i] = i == 0 ? 1.0 : 0.0;
  std::array<double*, 2> channels{left.data(), right.data()};
  processor.process(channels.data(), channels.size(), left.size());
  require(left == right, "linked stereo channels must have identical output and state");
}

void testDualMonoIsIndependent() {
  g3x::q10::Q10Processor processor;
  processor.prepare(48000.0, 2, 0.0);
  processor.setStereoLinked(false);
  processor.setBand(0, 0, {true, g3x::q10::FilterType::lowPass, 400.0, 0.0, 0.707});
  std::array<double, 1024> left{};
  std::array<double, 1024> right{};
  left[0] = right[0] = 1.0;
  std::array<double*, 2> channels{left.data(), right.data()};
  processor.process(channels.data(), channels.size(), left.size());
  require(left != right, "unlinked channel filter must not alter the other channel");
  require(right[0] == 1.0 && std::all_of(right.begin() + 1, right.end(),
    [](double value) { return value == 0.0; }), "unlinked right channel must remain neutral");
  require(processor.band(0, 0).enabled && !processor.band(1, 0).enabled,
    "dual-mono settings must be stored independently");
}

void testGainAndBypassAreSmoothed() {
  g3x::q10::Q10Processor processor;
  processor.prepare(8000.0, 1, 0.00125);
  processor.setInputGainDb(12.0);
  std::array<double, 20> samples;
  samples.fill(0.25);
  std::array<double*, 1> channels{samples.data()};
  processor.process(channels.data(), channels.size(), samples.size());
  require(samples.front() > 0.25 && samples.front() < samples.back(),
    "input gain must ramp rather than jump");
  require(std::abs(samples.back() - 0.25 * std::pow(10.0, 12.0 / 20.0)) < 1.0e-12,
    "input gain must reach its target");
  processor.setBypassed(true);
  samples.fill(0.25);
  processor.process(channels.data(), channels.size(), samples.size());
  require(samples.front() > samples.back() && samples.back() == 0.25,
    "bypass must crossfade toward the dry signal");
  require(std::abs(samples.back() - 0.25) < 1.0e-12, "bypass must finish at dry unity");
}

void testEnableAndTypeChangesStayContinuous() {
  g3x::q10::Q10Processor processor;
  processor.prepare(48000.0, 1, 0.02);
  std::vector<double> samples(4096, 0.2);
  std::array<double*, 1> channels{samples.data()};
  processor.process(channels.data(), 1, 1024);
  processor.setBand(0, {true, g3x::q10::FilterType::bell, 1000.0, 18.0, 2.0});
  processor.process(channels.data(), 1, 1024);
  processor.setBand(0, {true, g3x::q10::FilterType::highPass, 500.0, 0.0, 0.707});
  processor.process(channels.data(), 1, samples.size());
  double maximumStep{};
  for (std::size_t i = 1; i < samples.size(); ++i)
    maximumStep = std::max(maximumStep, std::abs(samples[i] - samples[i - 1]));
  require(std::isfinite(maximumStep) && maximumStep < 1.0,
    "enable and type changes must not create a discontinuity spike");
}

void testInvalidValuesAreContained() {
  g3x::q10::Q10Processor processor;
  processor.prepare(std::numeric_limits<double>::quiet_NaN(), 8);
  processor.setInputGainDb(std::numeric_limits<double>::infinity());
  processor.setOutputGainDb(std::numeric_limits<double>::quiet_NaN());
  processor.setBand(99, {});
  processor.setBand(8, 99, {});
  processor.setBand(0, {true, g3x::q10::FilterType::adaptiveBell,
    std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN(), -10.0});
  std::array<double, 64> samples{};
  samples[0] = std::numeric_limits<double>::quiet_NaN();
  std::array<double*, 1> channels{samples.data()};
  processor.process(channels.data(), 1, samples.size());
  require(std::all_of(samples.begin(), samples.end(), [](double value) { return std::isfinite(value); }),
    "invalid parameters and audio must not poison processor output");
  processor.process(static_cast<double* const*>(nullptr), 2, 64);
}

void testFloatProcessingPath() {
  g3x::q10::Q10Processor processor;
  processor.prepare(44100.0, 2, 0.0);
  processor.setBand(2, {true, g3x::q10::FilterType::highShelf, 5000.0, 6.0, 0.8});
  std::array<float, 256> left{};
  std::array<float, 256> right{};
  left[0] = right[0] = 1.0F;
  std::array<float*, 2> channels{left.data(), right.data()};
  processor.process(channels.data(), channels.size(), left.size());
  require(left == right && std::all_of(left.begin(), left.end(),
    [](float value) { return std::isfinite(value); }),
    "float host path must remain finite and stereo-linked");
}
}

void runQ10ProcessorTests() {
  testFlatChainIsExactIdentity();
  testTenBandsRemainFinite();
  testLinkedStereoHasParity();
  testDualMonoIsIndependent();
  testGainAndBypassAreSmoothed();
  testEnableAndTypeChangesStayContinuous();
  testInvalidValuesAreContained();
  testFloatProcessingPath();
}
