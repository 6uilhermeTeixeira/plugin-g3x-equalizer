#pragma once

#include <array>
#include <cstddef>
#include <complex>

namespace g3x::q10 {

enum class FilterType {
  bell,
  adaptiveBell,
  lowShelf,
  highShelf,
  highPass,
  lowPass
};

struct FilterParameters {
  FilterType type{FilterType::bell};
  double frequencyHz{1000.0};
  double gainDb{};
  double q{0.7071067811865476};
  double sampleRate{48000.0};
};

struct BiquadCoefficients {
  double b0{1.0};
  double b1{};
  double b2{};
  double a1{};
  double a2{};

  [[nodiscard]] std::complex<double> response(double frequencyHz,
    double sampleRate) const noexcept;
  [[nodiscard]] std::array<std::complex<double>, 2> poles() const noexcept;
  [[nodiscard]] bool isStable(double tolerance = 1.0e-12) const noexcept;
};

[[nodiscard]] BiquadCoefficients makeBiquad(FilterParameters parameters) noexcept;

class BiquadFilter {
public:
  void setCoefficients(BiquadCoefficients coefficients) noexcept;
  void configure(FilterParameters parameters) noexcept;
  void reset() noexcept;
  [[nodiscard]] double processSample(double input) noexcept;
  void process(double* samples, std::size_t sampleCount) noexcept;
  [[nodiscard]] const BiquadCoefficients& coefficients() const noexcept { return coefficients_; }

private:
  BiquadCoefficients coefficients_;
  double state1_{};
  double state2_{};
};

}
