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

/** A simple class that acts as an AudioIODeviceCallback and writes the
    incoming audio data to a WAV file.
*/
class Capteur final : public AudioIODeviceCallback
{
public:
    Capteur (AudioThumbnail& thumbnailToUpdate) : thumbnail (thumbnailToUpdate) { backgroundThread.startThread(); }

    ~Capteur() override { stop(); }

    void startRecording (const File& file);

    void stop();

    bool isRecording() const { return activeWriter.load() != nullptr; }

    void setInputGain (float newGain) noexcept { inputGain.store (jmax (0.0f, newGain)); }

    void audioDeviceAboutToStart (AudioIODevice* device) override
    {
        sampleRate = device->getCurrentSampleRate();
        recordingBuffer.setSize (1, jmax (1, device->getCurrentBufferSizeSamples()), false, false, true);
    }

    void audioDeviceStopped() override { sampleRate = 0; }

    void audioDeviceIOCallbackWithContext (const float* const*                 inputChannelData,
                                           int                                 numInputChannels,
                                           float* const*                       outputChannelData,
                                           int                                 numOutputChannels,
                                           int                                 numSamples,
                                           const AudioIODeviceCallbackContext& context) override;

private:
    AudioThumbnail& thumbnail;
    TimeSliceThread backgroundThread { "Audio Recorder Thread" }; // the thread that will write our audio data to disk
    std::unique_ptr<AudioFormatWriter::ThreadedWriter> threadedWriter; // the FIFO used to buffer the incoming data
    double                                             sampleRate    = 0.0;
    int64                                              nextSampleNum = 0;
    AudioBuffer<float>                                 recordingBuffer;
    std::atomic<float>                                 inputGain { 1.0f };

    CriticalSection                                 writerLock;
    std::atomic<AudioFormatWriter::ThreadedWriter*> activeWriter { nullptr };
};

//==============================================================================
