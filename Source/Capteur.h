/*
  ==============================================================================

   This file is part of the JUCE framework examples.
   Copyright (c) Raw Material Software Limited

   The code included in this file is provided under the terms of the ISC license
   http://www.isc.org/downloads/software-support-policy/isc-license. Permission
   to use, copy, modify, and/or distribute this software for any purpose with or
   without fee is hereby granted provided that the above copyright notice and
   this permission notice appear in all copies.

   THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH
   REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
   AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT,
   INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
   LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
   OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
   PERFORMANCE OF THIS SOFTWARE.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

inline std::unique_ptr<InputSource> makeInputSource (const URL& url)
{
    if (const auto doc = AndroidDocument::fromDocument (url))
        return std::make_unique<AndroidDocumentInputSource> (doc);

#if ! JUCE_IOS
    if (url.isLocalFile())
        return std::make_unique<FileInputSource> (url.getLocalFile());
#endif

    return std::make_unique<URLInputSource> (url);
}

inline std::unique_ptr<OutputStream> makeOutputStream (const URL& url)
{
    if (const auto doc = AndroidDocument::fromDocument (url))
        return doc.createOutputStream();

#if ! JUCE_IOS
    if (url.isLocalFile())
        return url.getLocalFile().createOutputStream();
#endif

    return url.createOutputStream();
}

//==============================================================================
/** A simple class that acts as an AudioIODeviceCallback and writes the
    incoming audio data to a WAV file.
*/
class Capteur final : public AudioIODeviceCallback
{
public:
    Capteur (AudioThumbnail& thumbnailToUpdate) : thumbnail (thumbnailToUpdate)
    {
        backgroundThread.startThread();
    }

    ~Capteur() override { stop(); }

    //==============================================================================
    void startRecording (const File& file)
    {
        stop();

        if (sampleRate > 0)
        {
            // Create an OutputStream to write to our destination file...
            file.deleteFile();

            if (std::unique_ptr<OutputStream> fileStream { file.createOutputStream() })
            {
                // Now create a WAV writer object that writes to our output stream...
                WavAudioFormat wavFormat;

                using Opts = AudioFormatWriterOptions;

                if (auto writer = wavFormat.createWriterFor (
                        fileStream, Opts {}.withSampleRate (sampleRate).withNumChannels (1).withBitsPerSample (16)))
                {
                    auto* writerPtr = writer.get();

                    // Now we'll create one of these helper objects which will act as a FIFO buffer, and will
                    // write the data to disk on our background thread.
                    threadedWriter.reset (
                        new AudioFormatWriter::ThreadedWriter (writer.release(), backgroundThread, 32768));

                    // Reset our recording thumbnail
                    thumbnail.reset (writerPtr->getNumChannels(), writerPtr->getSampleRate());
                    nextSampleNum = 0;

                    // And now, swap over our active writer pointer so that the audio callback will start using it..
                    const ScopedLock sl (writerLock);
                    activeWriter = threadedWriter.get();
                }
            }
        }
    }

    void stop()
    {
        // First, clear this pointer to stop the audio callback from using our writer object..
        {
            const ScopedLock sl (writerLock);
            activeWriter = nullptr;
        }

        // Now we can delete the writer object. It's done in this order because the deletion could
        // take a little time while remaining data gets flushed to disk, so it's best to avoid blocking
        // the audio callback while this happens.
        threadedWriter.reset();
    }

    bool isRecording() const { return activeWriter.load() != nullptr; }

    //==============================================================================
    void audioDeviceAboutToStart (AudioIODevice* device) override { sampleRate = device->getCurrentSampleRate(); }

    void audioDeviceStopped() override { sampleRate = 0; }

    void audioDeviceIOCallbackWithContext (const float* const*                 inputChannelData,
                                           int                                 numInputChannels,
                                           float* const*                       outputChannelData,
                                           int                                 numOutputChannels,
                                           int                                 numSamples,
                                           const AudioIODeviceCallbackContext& context) override
    {
        ignoreUnused (context);

        const ScopedLock sl (writerLock);

        if (activeWriter.load() != nullptr && numInputChannels >= thumbnail.getNumChannels())
        {
            activeWriter.load()->write (inputChannelData, numSamples);

            // Create an AudioBuffer to wrap our incoming data, note that this does no allocations or copies, it simply references our input data
            AudioBuffer<float> buffer (const_cast<float**> (inputChannelData), thumbnail.getNumChannels(), numSamples);
            thumbnail.addBlock (nextSampleNum, buffer, 0, numSamples);
            nextSampleNum += numSamples;
        }

        // We need to clear the output buffers, in case they're full of junk..
        for (int i = 0; i < numOutputChannels; ++i)
            if (outputChannelData[i] != nullptr)
                FloatVectorOperations::clear (outputChannelData[i], numSamples);
    }

private:
    AudioThumbnail& thumbnail;
    TimeSliceThread backgroundThread { "Audio Recorder Thread" }; // the thread that will write our audio data to disk
    std::unique_ptr<AudioFormatWriter::ThreadedWriter> threadedWriter; // the FIFO used to buffer the incoming data
    double                                             sampleRate    = 0.0;
    int64                                              nextSampleNum = 0;

    CriticalSection                                 writerLock;
    std::atomic<AudioFormatWriter::ThreadedWriter*> activeWriter { nullptr };
};



//==============================================================================
