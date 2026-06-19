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

#include "Capteur.h"

void Capteur::startRecording (const File& file)
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

void Capteur::stop()
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

void Capteur::audioDeviceIOCallbackWithContext (const float* const*                 inputChannelData,
                                                int                                 numInputChannels,
                                                float* const*                       outputChannelData,
                                                int                                 numOutputChannels,
                                                int                                 numSamples,
                                                const AudioIODeviceCallbackContext& context)
{
    ignoreUnused (context);

    const ScopedLock sl (writerLock);

    if (auto* writer = activeWriter.load(); writer != nullptr && numInputChannels >= thumbnail.getNumChannels())
    {
        const auto numChannelsToWrite = thumbnail.getNumChannels();

        if (recordingBuffer.getNumChannels() != numChannelsToWrite || recordingBuffer.getNumSamples() < numSamples)
            recordingBuffer.setSize (numChannelsToWrite, numSamples, false, false, true);

        for (int channel = 0; channel < numChannelsToWrite; ++channel)
        {
            if (const auto* inputChannel = inputChannelData[channel])
                recordingBuffer.copyFrom (channel, 0, inputChannel, numSamples);
            else
                recordingBuffer.clear (channel, 0, numSamples);
        }

        recordingBuffer.applyGain (inputGain.load());
        writer->write (recordingBuffer.getArrayOfReadPointers(), numSamples);
        thumbnail.addBlock (nextSampleNum, recordingBuffer, 0, numSamples);
        nextSampleNum += numSamples;
    }

    // We need to clear the output buffers, in case they're full of junk..
    for (int i = 0; i < numOutputChannels; ++i)
        if (outputChannelData[i] != nullptr)
            FloatVectorOperations::clear (outputChannelData[i], numSamples);
}
