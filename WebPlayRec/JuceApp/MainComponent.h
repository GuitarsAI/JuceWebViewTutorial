#pragma once

// CMake builds don't use an AppConfig.h, so it's safe to include juce module headers
// directly. If you need to remain compatible with Projucer-generated builds, and
// have called `juce_generate_juce_header(<thisTarget>)` in your CMakeLists.txt,
// you could `#include <JuceHeader.h>` here instead, to make all your module headers visible.
#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_audio_utils/juce_audio_utils.h>

#include "../JUCE/examples/Assets/DemoUtilities.h"
#include "../JUCE/examples/Assets/AudioLiveScrollingDisplay.h"

//==============================================================================
/** A simple class that acts as an AudioIODeviceCallback and writes the
    incoming audio data to a WAV file.
*/
class AudioRecorder final : public AudioIODeviceCallback
{
public:
    AudioRecorder(AudioThumbnail &thumbnailToUpdate)
        : thumbnail(thumbnailToUpdate)
    {
        backgroundThread.startThread();
    }

    ~AudioRecorder() override
    {
        stop();
    }

    //==============================================================================
    void startRecording(const File &file)
    {
        stop();

        if (sampleRate > 0)
        {
            // Create an OutputStream to write to our destination file...
            file.deleteFile();

            if (auto fileStream = std::unique_ptr<FileOutputStream>(file.createOutputStream()))
            {
                // Now create a WAV writer object that writes to our output stream...
                WavAudioFormat wavFormat;

                if (auto writer = wavFormat.createWriterFor(fileStream.get(), sampleRate, 1, 16, {}, 0))
                {
                    fileStream.release(); // (passes responsibility for deleting the stream to the writer object that is now using it)

                    // Now we'll create one of these helper objects which will act as a FIFO buffer, and will
                    // write the data to disk on our background thread.
                    threadedWriter.reset(new AudioFormatWriter::ThreadedWriter(writer, backgroundThread, 32768));

                    // Reset our recording thumbnail
                    thumbnail.reset(writer->getNumChannels(), writer->getSampleRate());
                    nextSampleNum = 0;

                    // And now, swap over our active writer pointer so that the audio callback will start using it..
                    const ScopedLock sl(writerLock);
                    activeWriter = threadedWriter.get();
                }
            }
        }
    }

    void stop()
    {
        // First, clear this pointer to stop the audio callback from using our writer object..
        {
            const ScopedLock sl(writerLock);
            activeWriter = nullptr;
        }

        // Now we can delete the writer object. It's done in this order because the deletion could
        // take a little time while remaining data gets flushed to disk, so it's best to avoid blocking
        // the audio callback while this happens.
        threadedWriter.reset();
    }

    bool isRecording() const
    {
        return activeWriter.load() != nullptr;
    }

    //==============================================================================
    void audioDeviceAboutToStart(AudioIODevice *device) override
    {
        sampleRate = device->getCurrentSampleRate();
    }

    void audioDeviceStopped() override
    {
        sampleRate = 0;
    }

    void audioDeviceIOCallbackWithContext(const float *const *inputChannelData, int numInputChannels,
                                          float *const *outputChannelData, int numOutputChannels,
                                          int numSamples, const AudioIODeviceCallbackContext &context) override
    {
        ignoreUnused(context);

        const ScopedLock sl(writerLock);

        if (activeWriter.load() != nullptr && numInputChannels >= thumbnail.getNumChannels())
        {
            activeWriter.load()->write(inputChannelData, numSamples);

            // Create an AudioBuffer to wrap our incoming data, note that this does no allocations or copies, it simply references our input data
            AudioBuffer<float> buffer(const_cast<float **>(inputChannelData), thumbnail.getNumChannels(), numSamples);
            thumbnail.addBlock(nextSampleNum, buffer, 0, numSamples);
            nextSampleNum += numSamples;
        }

        // We need to clear the output buffers, in case they're full of junk..
        for (int i = 0; i < numOutputChannels; ++i)
            if (outputChannelData[i] != nullptr)
                FloatVectorOperations::clear(outputChannelData[i], numSamples);
    }

private:
    AudioThumbnail &thumbnail;
    TimeSliceThread backgroundThread{"Audio Recorder Thread"};         // the thread that will write our audio data to disk
    std::unique_ptr<AudioFormatWriter::ThreadedWriter> threadedWriter; // the FIFO used to buffer the incoming data
    double sampleRate = 0.0;
    int64 nextSampleNum = 0;

    CriticalSection writerLock;
    std::atomic<AudioFormatWriter::ThreadedWriter *> activeWriter{nullptr};
};

//==============================================================================
class RecordingThumbnail final : public Component,
                                 private ChangeListener
{
public:
    RecordingThumbnail()
    {
        formatManager.registerBasicFormats();
        thumbnail.addChangeListener(this);
    }

    ~RecordingThumbnail() override
    {
        thumbnail.removeChangeListener(this);
    }

    AudioThumbnail &getAudioThumbnail() { return thumbnail; }

    void setDisplayFullThumbnail(bool displayFull)
    {
        displayFullThumb = displayFull;
        repaint();
    }

    void paint(Graphics &g) override
    {
        g.fillAll(Colours::darkgrey);
        g.setColour(Colours::lightgrey);

        if (thumbnail.getTotalLength() > 0.0)
        {
            auto endTime = displayFullThumb ? thumbnail.getTotalLength()
                                            : jmax(30.0, thumbnail.getTotalLength());

            auto thumbArea = getLocalBounds();
            thumbnail.drawChannels(g, thumbArea.reduced(2), 0.0, endTime, 1.0f);
        }
        else
        {
            g.setFont(14.0f);
            g.drawFittedText("(No file recorded)", getLocalBounds(), Justification::centred, 2);
        }
    }

private:
    AudioFormatManager formatManager;
    AudioThumbnailCache thumbnailCache{10};
    AudioThumbnail thumbnail{512, formatManager, thumbnailCache};

    bool displayFullThumb = false;

    void changeListenerCallback(ChangeBroadcaster *source) override
    {
        if (source == &thumbnail)
            repaint();
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RecordingThumbnail)
};

//==============================================================================
/*
    This component lives inside our window, and this is where you should put all
    your controls and content.
*/
class MainComponent final : public juce::Component
{
public:
    //==============================================================================
    MainComponent();
    ~MainComponent() override
    {
        audioDeviceManager.removeAudioCallback(&recorder);
        audioDeviceManager.removeAudioCallback(&liveAudioScroller);
    }

    //==============================================================================
    void paint(juce::Graphics &) override;
    void resized() override;

private:
    //==============================================================================
    // Your private member variables go here...

    // if this PIP is running inside the demo runner, we'll use the shared device manager instead
#ifndef JUCE_DEMO_RUNNER
    AudioDeviceManager audioDeviceManager;
#else
    AudioDeviceManager &audioDeviceManager{getSharedAudioDeviceManager(1, 0)};
#endif

    WebBrowserComponent webview;

    LiveScrollingAudioDisplay liveAudioScroller;
    RecordingThumbnail recordingThumbnail;
    AudioRecorder recorder{recordingThumbnail.getAudioThumbnail()};

    Label explanationLabel{{},
                           "This page demonstrates how to record a wave file from the live audio input.\n\n"
                           "After you are done with your recording you can choose where to save it."};
    TextButton recordButton{"Record"};
    File lastRecording;
    FileChooser chooser{"Output file...", File::getCurrentWorkingDirectory().getChildFile("recording.wav"), "*.wav"};

    void startRecording()
    {
        if (!RuntimePermissions::isGranted(RuntimePermissions::writeExternalStorage))
        {
            SafePointer<MainComponent> safeThis(this);

            RuntimePermissions::request(RuntimePermissions::writeExternalStorage,
                                        [safeThis](bool granted) mutable
                                        {
                                            if (granted)
                                                safeThis->startRecording();
                                        });
            return;
        }

#if (JUCE_ANDROID || JUCE_IOS)
        auto parentDir = File::getSpecialLocation(File::tempDirectory);
#else
        auto parentDir = File::getSpecialLocation(File::userDocumentsDirectory);
#endif

        lastRecording = parentDir.getNonexistentChildFile("JUCE Demo Audio Recording", ".wav");

        recorder.startRecording(lastRecording);

        recordButton.setButtonText("Stop");
        recordingThumbnail.setDisplayFullThumbnail(false);
    }

    void stopRecording()
    {
        recorder.stop();

        chooser.launchAsync(FileBrowserComponent::saveMode | FileBrowserComponent::canSelectFiles | FileBrowserComponent::warnAboutOverwriting,
                            [this](const FileChooser &c)
                            {
                                if (FileInputStream inputStream(lastRecording); inputStream.openedOk())
                                    if (const auto outputStream = makeOutputStream(c.getURLResult()))
                                        outputStream->writeFromInputStream(inputStream, -1);

                                recordButton.setButtonText("Record");
                                recordingThumbnail.setDisplayFullThumbnail(true);
                            });
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
