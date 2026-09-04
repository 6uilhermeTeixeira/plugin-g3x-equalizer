#pragma once

#include <array>
#include <atomic>
#include <juce_audio_utils/juce_audio_utils.h>
#include "dsp/Q10Processor.hpp"

class G3XEqualizerAudioProcessor final : public juce::AudioProcessor {
public:
  G3XEqualizerAudioProcessor();
  static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
  void prepareToPlay(double, int) override;
  void releaseResources() override;
  bool isBusesLayoutSupported(const BusesLayout&) const override;
  void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
  void processBlock(juce::AudioBuffer<double>&, juce::MidiBuffer&) override;
  bool supportsDoublePrecisionProcessing() const override { return true; }
  juce::AudioProcessorEditor* createEditor() override;
  bool hasEditor() const override { return true; }
  const juce::String getName() const override { return "G3X Equalizer"; }
  double getTailLengthSeconds() const override { return 0.0; }
  bool acceptsMidi() const override { return false; }
  bool producesMidi() const override { return false; }
  int getNumPrograms() override { return 1; }
  int getCurrentProgram() override { return 0; }
  void setCurrentProgram(int) override {}
  const juce::String getProgramName(int) override { return {}; }
  void changeProgramName(int, const juce::String&) override {}
  void getStateInformation(juce::MemoryBlock&) override;
  void setStateInformation(const void*, int) override;

private:
  struct BandParameterRefs {
    std::atomic<float>* enabled{};
    std::atomic<float>* type{};
    std::atomic<float>* frequency{};
    std::atomic<float>* gain{};
    std::atomic<float>* q{};
  };
  template <typename Sample>
  void process(juce::AudioBuffer<Sample>&);
  [[nodiscard]] g3x::q10::BandSettings readBand(std::size_t, bool) const noexcept;
  void updateParameters() noexcept;
  g3x::q10::Q10Processor dsp;
  juce::AudioProcessorValueTreeState state;
  std::array<std::array<BandParameterRefs, g3x::q10::bandCount>, 2> bandParameters_;
};
