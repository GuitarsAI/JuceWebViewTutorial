#include "MainComponent.h"

//==============================================================================
MainComponent::MainComponent() : webview(juce::WebBrowserComponent::Options{}.withBackend(
                                                                                 juce::WebBrowserComponent::Options::Backend::webview2)
                                             .withWinWebView2Options(
                                                 juce::WebBrowserComponent::Options::WinWebView2{}))
{
    setOpaque(true);
    addAndMakeVisible(liveAudioScroller);

    addAndMakeVisible(explanationLabel);
    explanationLabel.setFont(FontOptions(15.0f, Font::plain));
    explanationLabel.setJustificationType(Justification::topLeft);
    explanationLabel.setEditable(false, false, false);
    explanationLabel.setColour(TextEditor::textColourId, Colours::black);
    explanationLabel.setColour(TextEditor::backgroundColourId, Colour(0x00000000));

    addAndMakeVisible(recordButton);
    recordButton.setColour(TextButton::buttonColourId, Colour(0xffff5c5c));
    recordButton.setColour(TextButton::textColourOnId, Colours::black);

    recordButton.onClick = [this]
    {
        if (recorder.isRecording())
            stopRecording();
        else
            startRecording();
    };

    addAndMakeVisible(recordingThumbnail);
    addAndMakeVisible(webview);
    webview.goToURL("http://localhost:5000");

#ifndef JUCE_DEMO_RUNNER
    RuntimePermissions::request(RuntimePermissions::recordAudio,
                                [this](bool granted)
                                {
                                    int numInputChannels = granted ? 2 : 0;
                                    audioDeviceManager.initialise(numInputChannels, 2, nullptr, true, {}, nullptr);
                                });
#endif

    audioDeviceManager.addAudioCallback(&liveAudioScroller);
    audioDeviceManager.addAudioCallback(&recorder);

    setSize(500, 500);
}

//==============================================================================
void MainComponent::paint(juce::Graphics &g)
{
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    g.fillAll(getUIColourIfAvailable(LookAndFeel_V4::ColourScheme::UIColour::windowBackground));
}

void MainComponent::resized()
{
    // This is called when the MainComponent is resized.
    // If you add any child components, this is where you should
    // update their positions.
    auto area = getLocalBounds();

    liveAudioScroller.setBounds(area.removeFromTop(80).reduced(8));
    recordingThumbnail.setBounds(area.removeFromTop(80).reduced(8));
    recordButton.setBounds(area.removeFromTop(36).removeFromLeft(140).reduced(8));
    explanationLabel.setBounds(area.reduced(8));
    webview.setBounds(area.removeFromBottom(400));
}
