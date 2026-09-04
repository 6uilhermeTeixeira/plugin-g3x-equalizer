#include "dsp/BiquadFilter.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace g3x::q10 {
namespace {
constexpr double pi = 3.141592653589793238462643383279502884;

double finiteOr(double value, double fallback) noexcept {
  return std::isfinite(value) ? value : fallback;
}

BiquadCoefficients normalized(double b0, double b1, double b2,
    double a0, double a1, double a2) noexcept {
  if (!std::isfinite(a0) || std::abs(a0) < std::numeric_limits<double>::epsilon()) return {};
  const auto inverse = 1.0 / a0;
  const BiquadCoefficients result{
    b0 * inverse, b1 * inverse, b2 * inverse, a1 * inverse, a2 * inverse};
  return std::isfinite(result.b0) && std::isfinite(result.b1) && std::isfinite(result.b2)
      && std::isfinite(result.a1) && std::isfinite(result.a2) ? result : BiquadCoefficients{};
}
}

std::complex<double> BiquadCoefficients::response(double frequencyHz,
    double sampleRate) const noexcept {
  const auto rate = std::clamp(finiteOr(sampleRate, 48000.0), 8000.0, 384000.0);
  const auto frequency = std::clamp(finiteOr(frequencyHz, 0.0), 0.0, rate * 0.5);
  const auto z1 = std::polar(1.0, -2.0 * pi * frequency / rate);
  const auto z2 = z1 * z1;
  const auto denominator = 1.0 + a1 * z1 + a2 * z2;
  if (std::abs(denominator) < std::numeric_limits<double>::epsilon())
    return {std::numeric_limits<double>::infinity(), 0.0};
  return (b0 + b1 * z1 + b2 * z2) / denominator;
}

std::array<std::complex<double>, 2> BiquadCoefficients::poles() const noexcept {
  const auto discriminant = std::sqrt(std::complex<double>{a1 * a1 - 4.0 * a2, 0.0});
  return {(-a1 + discriminant) * 0.5, (-a1 - discriminant) * 0.5};
}

bool BiquadCoefficients::isStable(double tolerance) const noexcept {
  const auto margin = std::clamp(finiteOr(tolerance, 1.0e-12), 0.0, 0.1);
  for (const auto pole : poles())
    if (!std::isfinite(pole.real()) || !std::isfinite(pole.imag())
        || std::abs(pole) >= 1.0 - margin) return false;
  return true;
}

BiquadCoefficients makeBiquad(FilterParameters parameters) noexcept {
  const auto rate = std::clamp(finiteOr(parameters.sampleRate, 48000.0), 8000.0, 384000.0);
  const auto maximumFrequency = std::min(30000.0, 0.475 * rate);
  const auto frequency = std::clamp(finiteOr(parameters.frequencyHz, 1000.0), 10.0,
    maximumFrequency);
  const auto gain = std::clamp(finiteOr(parameters.gainDb, 0.0), -24.0, 24.0);
  auto q = std::clamp(finiteOr(parameters.q, 0.7071067811865476), 0.1, 100.0);
  if (parameters.type == FilterType::adaptiveBell)
    q = std::min(100.0, q * (1.0 + std::abs(gain) / 12.0));
  if ((parameters.type == FilterType::bell || parameters.type == FilterType::adaptiveBell
      || parameters.type == FilterType::lowShelf || parameters.type == FilterType::highShelf)
      && gain == 0.0) return {};

  const auto omega = 2.0 * pi * frequency / rate;
  const auto cosine = std::cos(omega);
  const auto sine = std::sin(omega);
  const auto alpha = sine / (2.0 * q);
  const auto amplitude = std::pow(10.0, gain / 40.0);

  switch (parameters.type) {
    case FilterType::bell:
    case FilterType::adaptiveBell:
      return normalized(1.0 + alpha * amplitude, -2.0 * cosine,
        1.0 - alpha * amplitude, 1.0 + alpha / amplitude, -2.0 * cosine,
        1.0 - alpha / amplitude);
    case FilterType::lowShelf: {
      const auto rootTerm = 2.0 * std::sqrt(amplitude) * alpha;
      return normalized(amplitude * ((amplitude + 1.0) - (amplitude - 1.0) * cosine + rootTerm),
        2.0 * amplitude * ((amplitude - 1.0) - (amplitude + 1.0) * cosine),
        amplitude * ((amplitude + 1.0) - (amplitude - 1.0) * cosine - rootTerm),
        (amplitude + 1.0) + (amplitude - 1.0) * cosine + rootTerm,
        -2.0 * ((amplitude - 1.0) + (amplitude + 1.0) * cosine),
        (amplitude + 1.0) + (amplitude - 1.0) * cosine - rootTerm);
    }
    case FilterType::highShelf: {
      const auto rootTerm = 2.0 * std::sqrt(amplitude) * alpha;
      return normalized(amplitude * ((amplitude + 1.0) + (amplitude - 1.0) * cosine + rootTerm),
        -2.0 * amplitude * ((amplitude - 1.0) + (amplitude + 1.0) * cosine),
        amplitude * ((amplitude + 1.0) + (amplitude - 1.0) * cosine - rootTerm),
        (amplitude + 1.0) - (amplitude - 1.0) * cosine + rootTerm,
        2.0 * ((amplitude - 1.0) - (amplitude + 1.0) * cosine),
        (amplitude + 1.0) - (amplitude - 1.0) * cosine - rootTerm);
    }
    case FilterType::highPass:
      return normalized((1.0 + cosine) * 0.5, -(1.0 + cosine),
        (1.0 + cosine) * 0.5, 1.0 + alpha, -2.0 * cosine, 1.0 - alpha);
    case FilterType::lowPass:
      return normalized((1.0 - cosine) * 0.5, 1.0 - cosine,
        (1.0 - cosine) * 0.5, 1.0 + alpha, -2.0 * cosine, 1.0 - alpha);
  }
  return {};
}

void BiquadFilter::setCoefficients(BiquadCoefficients coefficients) noexcept {
  coefficients_ = coefficients.isStable() ? coefficients : BiquadCoefficients{};
}
void BiquadFilter::configure(FilterParameters parameters) noexcept {
  setCoefficients(makeBiquad(parameters));
}
void BiquadFilter::reset() noexcept { state1_ = 0.0; state2_ = 0.0; }
double BiquadFilter::processSample(double input) noexcept {
  if (!std::isfinite(input)) input = 0.0;
  const auto output = coefficients_.b0 * input + state1_;
  state1_ = coefficients_.b1 * input - coefficients_.a1 * output + state2_;
  state2_ = coefficients_.b2 * input - coefficients_.a2 * output;
  if (!std::isfinite(output) || !std::isfinite(state1_) || !std::isfinite(state2_)) {
    reset();
    return 0.0;
  }
  return output;
}
void BiquadFilter::process(double* samples, std::size_t sampleCount) noexcept {
  if (samples == nullptr) return;
  for (std::size_t sample = 0; sample < sampleCount; ++sample)
    samples[sample] = processSample(samples[sample]);
}

}
