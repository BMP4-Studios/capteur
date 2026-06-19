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
#include "Capteur.h"
#include "AudioLiveScrollingDisplay.h"
#include "RecordingThumbnail.h"

inline Colour getUIColourIfAvailable (LookAndFeel_V4::ColourScheme::UIColour uiColour,
                                      Colour                                 fallback = Colour (0xff4d4d4d)) noexcept
{
    if (auto* v4 = dynamic_cast<LookAndFeel_V4*> (&LookAndFeel::getDefaultLookAndFeel ()))
        return v4->getCurrentColourScheme ().getUIColour (uiColour);

    return fallback;
}

class CapteurComponent final : public Component, private ChangeListener
{
public:
    CapteurComponent ();

    ~CapteurComponent () override;

    void paint (Graphics& g) override
    {
        g.fillAll (getUIColourIfAvailable (LookAndFeel_V4::ColourScheme::UIColour::windowBackground));
    }

    void resized () override;

private:

    AudioDeviceManager audioDeviceManager;
    LiveScrollingAudioDisplay liveAudioScroller;
    RecordingThumbnail        recordingThumbnail;
    Capteur             recorder { recordingThumbnail.getAudioThumbnail () };

    Label       explanationLabel { {},
                                   "This page demonstrates how to record a wave file from the live audio input.\n\n"
                                   "After you are done with your recording you can choose where to save it." };
    Label       gainLabel { {}, "Input Trim" };
    Slider      gainSlider { Slider::LinearHorizontal, Slider::TextBoxRight };
    TextButton  recordButton { "Record" };
    TextButton  settingsButton { "Settings" };
    File        lastRecording;
    FileChooser chooser { "Output file...",
                          File::getCurrentWorkingDirectory ().getChildFile ("recording.wav"),
                          "*.wav" };

    void startRecording ();

    void stopRecording ();

    void showAudioSettings ();

    void disableInputPreprocessingIfAvailable ();

    void changeListenerCallback (ChangeBroadcaster* source) override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CapteurComponent)
};
