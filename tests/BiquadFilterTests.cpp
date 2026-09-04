#include "dsp/BiquadFilter.hpp"

#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string_view>

namespace {
constexpr double pi = 3.141592653589793238462643383279502884;

void require(bool condition, std::string_view message) {
  if (!condition) {
    std::cerr << "FAILED: " << message << '\n';
    std::exit(EXIT_FAILURE);
  }
}

double magnitudeDb(const g3x::q10::BiquadCoefficients& coefficients,
    double frequency, double sampleRate) {
  return 20.0 * std::log10(std::abs(coefficients.response(frequency, sampleRate)));
}

void testZeroGainIsExactIdentity() {
  for (const auto type : {g3x::q10::FilterType::bell, g3x::q10::FilterType::adaptiveBell,
      g3x::q10::FilterType::lowShelf, g3x::q10::FilterType::highShelf}) {
    const auto coefficients = g3x::q10::makeBiquad({type, 1234.0, 0.0, 4.0, 48000.0});
    require(coefficients.b0 == 1.0 && coefficients.b1 == 0.0 && coefficients.b2 == 0.0
      && coefficients.a1 == 0.0 && coefficients.a2 == 0.0, "zero gain must be exact identity");
  }
}

void testBellGainAtCentre() {
  for (const auto gain : {-24.0, -9.0, 6.0, 24.0}) {
    const auto coefficients = g3x::q10::makeBiquad(
      {g3x::q10::FilterType::bell, 2500.0, gain, 3.2, 48000.0});
    require(std::abs(magnitudeDb(coefficients, 2500.0, 48000.0) - gain) < 1.0e-9,
      "bell centre gain must match the requested gain");
  }
}

void testCutFiltersAtCorner() {
  constexpr auto q = 0.7071067811865476;
  for (const auto type : {g3x::q10::FilterType::highPass, g3x::q10::FilterType::lowPass}) {
    const auto coefficients = g3x::q10::makeBiquad({type, 1000.0, 0.0, q, 48000.0});
    require(std::abs(magnitudeDb(coefficients, 1000.0, 48000.0) + 3.01029995664) < 1.0e-8,
      "cut filter corner must be minus 3.01 dB for Butterworth Q");
  }
}

void testShelvesReachRequestedGain() {
  const auto low = g3x::q10::makeBiquad(
    {g3x::q10::FilterType::lowShelf, 1000.0, 12.0, 0.707, 96000.0});
  const auto high = g3x::q10::makeBiquad(
    {g3x::q10::FilterType::highShelf, 3000.0, -9.0, 0.707, 96000.0});
  require(std::abs(magnitudeDb(low, 10.0, 96000.0) - 12.0) < 0.01,
    "low shelf must reach its low-frequency gain");
  require(std::abs(magnitudeDb(high, 30000.0, 96000.0) + 9.0) < 0.05,
    "high shelf must reach its high-frequency gain");
}

void testAdaptiveBellNarrowsWithGain() {
  const auto regular = g3x::q10::makeBiquad(
    {g3x::q10::FilterType::bell, 2000.0, 18.0, 1.0, 48000.0});
  const auto adaptive = g3x::q10::makeBiquad(
    {g3x::q10::FilterType::adaptiveBell, 2000.0, 18.0, 1.0, 48000.0});
  require(std::abs(magnitudeDb(adaptive, 2000.0, 48000.0) - 18.0) < 1.0e-9,
    "adaptive bell must preserve centre gain");
  require(magnitudeDb(adaptive, 900.0, 48000.0) < magnitudeDb(regular, 900.0, 48000.0),
    "adaptive bell must become narrower as absolute gain increases");
}

void testAnalyticalResponseMatchesProcessedSine() {
  constexpr double rate = 48000.0;
  constexpr double frequency = 997.0;
  g3x::q10::BiquadFilter filter;
  filter.configure({g3x::q10::FilterType::bell, 997.0, 9.0, 2.0, rate});
  double inputEnergy{};
  double outputEnergy{};
  constexpr std::size_t totalSamples = 48000;
  constexpr std::size_t warmup = 12000;
  for (std::size_t sample = 0; sample < totalSamples; ++sample) {
    const auto input = std::sin(2.0 * pi * frequency * static_cast<double>(sample) / rate);
    const auto output = filter.processSample(input);
    if (sample >= warmup) { inputEnergy += input * input; outputEnergy += output * output; }
  }
  const auto measured = std::sqrt(outputEnergy / inputEnergy);
  const auto analytical = std::abs(filter.coefficients().response(frequency, rate));
  require(std::abs(measured - analytical) < 1.0e-6,
    "processed sine gain must match analytical response");
}

void testPolesRemainStableAcrossExtremes() {
  constexpr std::array rates{44100.0, 48000.0, 96000.0, 192000.0};
  constexpr std::array types{g3x::q10::FilterType::bell, g3x::q10::FilterType::adaptiveBell,
    g3x::q10::FilterType::lowShelf, g3x::q10::FilterType::highShelf,
    g3x::q10::FilterType::highPass, g3x::q10::FilterType::lowPass};
  for (const auto rate : rates)
    for (const auto type : types)
      for (const auto frequency : {10.0, 1000.0, 30000.0})
        for (const auto gain : {-24.0, 24.0})
          for (const auto q : {0.1, 100.0})
            require(g3x::q10::makeBiquad({type, frequency, gain, q, rate}).isStable(),
              "all sanitized coefficient combinations must remain stable");
}

void testInvalidInputCannotPoisonState() {
  g3x::q10::BiquadFilter filter;
  filter.configure({g3x::q10::FilterType::highPass, std::numeric_limits<double>::infinity(),
    std::numeric_limits<double>::quiet_NaN(), -1.0, 0.0});
  require(filter.coefficients().isStable(), "invalid parameters must produce stable coefficients");
  require(std::isfinite(filter.processSample(std::numeric_limits<double>::quiet_NaN())),
    "NaN input must be sanitized");
  require(std::isfinite(filter.processSample(1.0)), "filter state must recover after invalid input");
  filter.process(nullptr, 100);
}
}

void runQ10ProcessorTests();

int main() {
  testZeroGainIsExactIdentity();
  testBellGainAtCentre();
  testCutFiltersAtCorner();
  testShelvesReachRequestedGain();
  testAdaptiveBellNarrowsWithGain();
  testAnalyticalResponseMatchesProcessedSine();
  testPolesRemainStableAcrossExtremes();
  testInvalidInputCannotPoisonState();
  runQ10ProcessorTests();
  std::cout << "All G3X Q10 DSP tests passed\n";
}
