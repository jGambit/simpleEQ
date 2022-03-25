/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

enum Slope {
    Slope_12,
    Slope_24,
    Slope_36,
    Slope_48
};

struct ChainSetting {
    float peakFreq{ 0 }, peakGainInDecibels{ 0 }, peakQ{ 1.0f };
    float lowCutFreq{ 0 }, highCutFreq{0};
    Slope lowCutSlope{ Slope::Slope_12 }, highCutSlope{ Slope::Slope_12 };
};

ChainSetting getChainSettings(juce::AudioProcessorValueTreeState& audioState);

//==============================================================================
/**
*/
class SimpleEQAudioProcessor  : public juce::AudioProcessor
{
public:
    //==============================================================================
    SimpleEQAudioProcessor();
    ~SimpleEQAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    juce::AudioProcessorValueTreeState audioState{
        *this, nullptr, "Parameters", createParameterLayout()
    };

private:
    using Filter = juce::dsp::IIR::Filter<float>;
    using CutFilter = juce::dsp::ProcessorChain<Filter, Filter, Filter, Filter>; // 12 db/Octave * 4 = 48 db/Octave
    using MonoCahin = juce::dsp::ProcessorChain<CutFilter, Filter, CutFilter>;  // all Filters together (LowCut, Peak, HighCut)
    MonoCahin leftChannel, rightChannel;

    enum ChainPositions {
        LowCut,
        Peak,
        HighCut
    };

    void updatePeakFilter(const ChainSetting& settings);
    void updateLowCutFilter(const ChainSetting& settings);
    void updateHighCutFilter(const ChainSetting& settings);
    using Coefficients = Filter::CoefficientsPtr;
    static void updateCoefficients(Coefficients& old, const Coefficients& replacement);

    template<int Index, typename ChainType, typename CoefficientType>
    void update(ChainType& chain, const CoefficientType& coefficients) {
        updateCoefficients(chain.get<Index>().coefficients, coefficients[Index]);
        chain.setBypassed<Index>(false);
    }

    template<typename ChainType, typename CoefficientType>

    void updateCutFilter(ChainType& leftLowCut,
        const CoefficientType& cutCoefficients,
        const Slope& lowCutSlope) {
        // bypass links in the chain
        leftLowCut.setBypassed<0>(true);
        leftLowCut.setBypassed<1>(true);
        leftLowCut.setBypassed<2>(true);
        leftLowCut.setBypassed<3>(true);

        switch (lowCutSlope) {
        case Slope_48:
            update<3>(leftLowCut, cutCoefficients);
        case Slope_36:
            update<2>(leftLowCut, cutCoefficients);
        case Slope_24:
            update<1>(leftLowCut, cutCoefficients);
        case Slope_12:
            update<0>(leftLowCut, cutCoefficients);
        }
    }

    void updateFilters();

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SimpleEQAudioProcessor)
};
