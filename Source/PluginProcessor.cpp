/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
SimpleEQAudioProcessor::SimpleEQAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       )
#endif
{
}

SimpleEQAudioProcessor::~SimpleEQAudioProcessor()
{
}

//==============================================================================
const juce::String SimpleEQAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool SimpleEQAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool SimpleEQAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool SimpleEQAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double SimpleEQAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int SimpleEQAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int SimpleEQAudioProcessor::getCurrentProgram()
{
    return 0;
}

void SimpleEQAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String SimpleEQAudioProcessor::getProgramName (int index)
{
    return {};
}

void SimpleEQAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void SimpleEQAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Use this method as the place to do any pre-playback
    // initialisation that you need..
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = samplesPerBlock;
    spec.numChannels = 1;   // The filters work on a mono Channel

    leftChannel.prepare(spec);
    rightChannel.prepare(spec);

    updateFilters();
}

void SimpleEQAudioProcessor::updateLowCutFilter(const ChainSetting& settings)
{
    auto cutCoefficients = juce::dsp::FilterDesign<float>::designIIRHighpassHighOrderButterworthMethod(settings.lowCutFreq,
        getSampleRate(),
        2 * (settings.lowCutSlope + 1));

    auto& leftLowCut = leftChannel.get<ChainPositions::LowCut>();
    updateCutFilter(leftLowCut, cutCoefficients, settings.lowCutSlope);
    
    auto& rightLowCut = rightChannel.get<ChainPositions::LowCut>();
    updateCutFilter(rightLowCut, cutCoefficients, settings.lowCutSlope);
}

void SimpleEQAudioProcessor::updateHighCutFilter(const ChainSetting& settings)
{
    auto cutCoefficients = juce::dsp::FilterDesign<float>::designIIRLowpassHighOrderButterworthMethod(settings.highCutFreq,
        getSampleRate(),
        2 * (settings.highCutSlope + 1));

    auto& leftHighCut = leftChannel.get<ChainPositions::HighCut>();
    updateCutFilter(leftHighCut, cutCoefficients, settings.highCutSlope);

    auto& rightHighCut = rightChannel.get<ChainPositions::HighCut>();
    updateCutFilter(rightHighCut, cutCoefficients, settings.highCutSlope);
}

void SimpleEQAudioProcessor::updateCoefficients(Coefficients& old, const Coefficients& replacement)
{
    *old = *replacement;
}

void SimpleEQAudioProcessor::updateFilters()
{
    // always update settings before running audio through
    auto settings = getChainSettings(audioState);
    updateLowCutFilter(settings);
    updatePeakFilter(settings);
    updateHighCutFilter(settings);
}


void SimpleEQAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool SimpleEQAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

void SimpleEQAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // In case we have more outputs than inputs, this code clears any output
    // channels that didn't contain input data, (because these aren't
    // guaranteed to be empty - they may contain garbage).
    // This is here to avoid people getting screaming feedback
    // when they first compile a plugin, but obviously you don't need to keep
    // this code if your algorithm always overwrites all the output channels.
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    updateFilters();

    juce::dsp::AudioBlock<float> block(buffer);

    auto leftBlock = block.getSingleChannelBlock(0);
    auto rightBlock = block.getSingleChannelBlock(1);

    juce::dsp::ProcessContextReplacing<float> leftContext(leftBlock);
    juce::dsp::ProcessContextReplacing<float> rightContext(rightBlock);

    leftChannel.process(leftContext);
    rightChannel.process(rightContext);
}

//==============================================================================
bool SimpleEQAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* SimpleEQAudioProcessor::createEditor()
{
    // return new SimpleEQAudioProcessorEditor (*this);
    return new juce::GenericAudioProcessorEditor(*this);
}

//==============================================================================
void SimpleEQAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
}

void SimpleEQAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
}

juce::AudioProcessorValueTreeState::ParameterLayout SimpleEQAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout result = juce::AudioProcessorValueTreeState::ParameterLayout();
    result.add(std::make_unique<juce::AudioParameterFloat>("LowCut Freq", "LowCut Freq", juce::NormalisableRange<float>(20.f, 20000.f, 1.f, 0.25f), 20.f));
    result.add(std::make_unique<juce::AudioParameterFloat>("HighCut Freq", "HighCut Freq", juce::NormalisableRange<float>(20.f, 20000.f, 1.f, 0.25f), 20000.f));
    result.add(std::make_unique<juce::AudioParameterFloat>("Peak Freq", "Peak Freq", juce::NormalisableRange<float>(20.f, 20000.f, 1.f, 0.25f), 750.f));
    result.add(std::make_unique<juce::AudioParameterFloat>("Peak Gain", "Peak Gain", juce::NormalisableRange<float>(-24.f, 24.f, 0.5f), 0.0f));
    result.add(std::make_unique<juce::AudioParameterFloat>("Peak Q", "Peak Q", juce::NormalisableRange<float>(0.1, 10.f, 0.05f), 1.f));

    juce::StringArray slopes;
    slopes.add("12");
    slopes.add("24");
    slopes.add("36");
    slopes.add("48");

    result.add(std::make_unique<juce::AudioParameterChoice>("LowCut Slope", "Low Cut Slope", slopes, 0));
    result.add(std::make_unique<juce::AudioParameterChoice>("HighCut Slope", "High Cut Slope", slopes, 0));

    return result;
}

void SimpleEQAudioProcessor::updatePeakFilter(const ChainSetting& settings)
{
    auto peakCoefficients = juce::dsp::IIR::Coefficients<float>::makePeakFilter(getSampleRate(),
        settings.peakFreq,
        settings.peakQ,
        juce::Decibels::decibelsToGain(settings.peakGainInDecibels));
    *leftChannel.get<ChainPositions::Peak>().coefficients = *peakCoefficients;
    *rightChannel.get<ChainPositions::Peak>().coefficients = *peakCoefficients;
}

ChainSetting getChainSettings(juce::AudioProcessorValueTreeState& audioState)
{
    ChainSetting result;
    result.highCutFreq = audioState.getRawParameterValue("HighCut Freq")->load();
    result.highCutSlope = static_cast<Slope>(audioState.getRawParameterValue("HighCut Slope")->load());
    result.lowCutFreq = audioState.getRawParameterValue("LowCut Freq")->load();
    result.lowCutSlope = static_cast<Slope>(audioState.getRawParameterValue("LowCut Slope")->load());
    result.peakFreq = audioState.getRawParameterValue("Peak Freq")->load();
    result.peakGainInDecibels = audioState.getRawParameterValue("Peak Gain")->load();
    result.peakQ = audioState.getRawParameterValue("Peak Q")->load();

    return result;
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SimpleEQAudioProcessor();
}
