#include "plugin/PluginProcessor.hpp"

#include <array>
#include <string>

namespace {
std::string bandId(std::size_t band, const char* suffix, bool right = false) {
  return "band" + std::to_string(band + 1) + (right ? ".right." : ".") + suffix;
}

}

G3XEqualizerAudioProcessor::G3XEqualizerAudioProcessor()
  : AudioProcessor(BusesProperties()
      .withInput("Input", juce::AudioChannelSet::stereo(), true)
      .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
    state(*this, nullptr, "state", createParameterLayout()) {
  for (std::size_t channel = 0; channel < bandParameters_.size(); ++channel)
    for (std::size_t band = 0; band < g3x::q10::bandCount; ++band) {
      const auto right = channel == 1;
      auto& refs = bandParameters_[channel][band];
      refs.enabled = state.getRawParameterValue(bandId(band, "enabled", right));
      refs.type = state.getRawParameterValue(bandId(band, "type", right));
      refs.frequency = state.getRawParameterValue(bandId(band, "frequencyHz", right));
      refs.gain = state.getRawParameterValue(bandId(band, "gainDb", right));
      refs.q = state.getRawParameterValue(bandId(band, "q", right));
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout G3XEqualizerAudioProcessor::createParameterLayout() {
  juce::AudioProcessorValueTreeState::ParameterLayout layout;
  layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"inputGainDb", 1},
    "Input gain", juce::NormalisableRange<float>{-24.0F, 12.0F, 0.1F}, 0.0F, "dB"));
  layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"outputGainDb", 1},
    "Output gain", juce::NormalisableRange<float>{-24.0F, 12.0F, 0.1F}, 0.0F, "dB"));
  layout.add(std::make_unique<juce::AudioParameterBool>(
    juce::ParameterID{"bypass", 1}, "Bypass", false));
  layout.add(std::make_unique<juce::AudioParameterBool>(
    juce::ParameterID{"stereoLink", 1}, "Stereo link", true));
  const juce::StringArray types{"Bell", "Adaptive Bell", "Low Shelf", "High Shelf",
    "High Pass", "Low Pass"};
  for (std::size_t band = 0; band < g3x::q10::bandCount; ++band) {
    for (const auto right : {false, true}) {
      const auto side = right ? " right" : "";
      const auto name = "Band " + juce::String(static_cast<int>(band + 1)) + side;
      auto frequencyRange = juce::NormalisableRange<float>{10.0F, 30000.0F, 0.01F};
      frequencyRange.setSkewForCentre(1000.0F);
      layout.add(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{bandId(band, "enabled", right), 1}, name + " enabled", false));
      layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{bandId(band, "type", right), 1}, name + " type", types, 0));
      layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{bandId(band, "frequencyHz", right), 1}, name + " frequency",
        frequencyRange, 1000.0F, "Hz"));
      layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{bandId(band, "gainDb", right), 1}, name + " gain",
        juce::NormalisableRange<float>{-24.0F, 24.0F, 0.1F}, 0.0F, "dB"));
      auto qRange = juce::NormalisableRange<float>{0.1F, 100.0F, 0.001F};
      qRange.setSkewForCentre(1.0F);
      layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{bandId(band, "q", right), 1}, name + " Q", qRange, 0.707F));
    }
  }
  return layout;
}

void G3XEqualizerAudioProcessor::prepareToPlay(double sampleRate, int maximumBlockSize) {
  juce::ignoreUnused(maximumBlockSize);
  dsp.prepare(sampleRate, static_cast<std::size_t>(getTotalNumOutputChannels()));
  updateParameters();
  setLatencySamples(static_cast<int>(dsp.latencySamples()));
}
void G3XEqualizerAudioProcessor::releaseResources() { dsp.reset(); }
bool G3XEqualizerAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
  const auto input = layouts.getMainInputChannelSet();
  return input == layouts.getMainOutputChannelSet()
    && (input == juce::AudioChannelSet::mono() || input == juce::AudioChannelSet::stereo());
}
template <typename Sample>
void G3XEqualizerAudioProcessor::process(juce::AudioBuffer<Sample>& buffer) {
  updateParameters();
  std::array<Sample*, 2> channels{buffer.getWritePointer(0),
    buffer.getNumChannels() > 1 ? buffer.getWritePointer(1) : nullptr};
  dsp.process(channels.data(), static_cast<std::size_t>(buffer.getNumChannels()),
    static_cast<std::size_t>(buffer.getNumSamples()));
}
void G3XEqualizerAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) {
  juce::ScopedNoDenormals guard;
  process(buffer);
}
void G3XEqualizerAudioProcessor::processBlock(juce::AudioBuffer<double>& buffer, juce::MidiBuffer&) {
  juce::ScopedNoDenormals guard;
  process(buffer);
}
juce::AudioProcessorEditor* G3XEqualizerAudioProcessor::createEditor() {
  return new juce::GenericAudioProcessorEditor(*this);
}
void G3XEqualizerAudioProcessor::getStateInformation(juce::MemoryBlock& destination) {
  auto snapshot = state.copyState();
  snapshot.setProperty("stateVersion", 1, nullptr);
  if (auto xml = snapshot.createXml()) copyXmlToBinary(*xml, destination);
}
void G3XEqualizerAudioProcessor::setStateInformation(const void* data, int size) {
  if (auto xml = getXmlFromBinary(data, size)) state.replaceState(juce::ValueTree::fromXml(*xml));
}
g3x::q10::BandSettings G3XEqualizerAudioProcessor::readBand(std::size_t band,
    bool right) const noexcept {
  const auto& refs = bandParameters_[right ? 1U : 0U][band];
  return {refs.enabled->load() >= 0.5F,
    static_cast<g3x::q10::FilterType>(static_cast<int>(refs.type->load())),
    refs.frequency->load(), refs.gain->load(), refs.q->load()};
}
void G3XEqualizerAudioProcessor::updateParameters() noexcept {
  const auto linked = *state.getRawParameterValue("stereoLink") >= 0.5F;
  dsp.setStereoLinked(linked);
  dsp.setInputGainDb(*state.getRawParameterValue("inputGainDb"));
  dsp.setOutputGainDb(*state.getRawParameterValue("outputGainDb"));
  dsp.setBypassed(*state.getRawParameterValue("bypass") >= 0.5F);
  for (std::size_t band = 0; band < g3x::q10::bandCount; ++band) {
    if (linked) dsp.setBand(band, readBand(band, false));
    else {
      dsp.setBand(0, band, readBand(band, false));
      dsp.setBand(1, band, readBand(band, true));
    }
  }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new G3XEqualizerAudioProcessor(); }
