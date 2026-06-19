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


class CapteurComponent final : public Component
{
public:
    CapteurComponent ()
    {
        setOpaque (true);
        addAndMakeVisible (liveAudioScroller);

        addAndMakeVisible (explanationLabel);
        explanationLabel.setFont (FontOptions (15.0f, Font::plain));
        explanationLabel.setJustificationType (Justification::topLeft);
        explanationLabel.setEditable (false, false, false);
        explanationLabel.setColour (TextEditor::textColourId, Colours::black);
        explanationLabel.setColour (TextEditor::backgroundColourId, Colour (0x00000000));

        addAndMakeVisible (recordButton);
        recordButton.setColour (TextButton::buttonColourId, Colour (0xffff5c5c));
        recordButton.setColour (TextButton::textColourOnId, Colours::black);

        recordButton.onClick = [this]
            {
                if (recorder.isRecording ())
                    stopRecording ();
                else
                    startRecording ();
            };

        addAndMakeVisible (settingsButton);
        settingsButton.onClick = [this] { showAudioSettings (); };

        addAndMakeVisible (recordingThumbnail);

#ifndef JUCE_DEMO_RUNNER
        RuntimePermissions::request (RuntimePermissions::recordAudio,
                                     [this](bool granted)
                                     {
                                         int numInputChannels = granted ? 2 : 0;
                                         audioDeviceManager.initialise (
                                             numInputChannels, 2, nullptr, true, {}, nullptr);
                                     });
#endif

        audioDeviceManager.addAudioCallback (&liveAudioScroller);
        audioDeviceManager.addAudioCallback (&recorder);

        setSize (500, 500);
    }

    ~CapteurComponent () override
    {
        audioDeviceManager.removeAudioCallback (&recorder);
        audioDeviceManager.removeAudioCallback (&liveAudioScroller);
    }

    void paint (Graphics& g) override
    {
        g.fillAll (getUIColourIfAvailable (LookAndFeel_V4::ColourScheme::UIColour::windowBackground));
    }

    void resized () override
    {
        auto area = getLocalBounds ();

        liveAudioScroller.setBounds (area.removeFromTop (80).reduced (8));
        recordingThumbnail.setBounds (area.removeFromTop (80).reduced (8));

        auto buttonRow = area.removeFromTop (36);
        recordButton.setBounds (buttonRow.removeFromLeft (140).reduced (8));
        settingsButton.setBounds (buttonRow.removeFromLeft (140).reduced (8));

        explanationLabel.setBounds (area.reduced (8));
    }

private:
    // if this PIP is running inside the demo runner, we'll use the shared device manager instead
#ifndef JUCE_DEMO_RUNNER
    AudioDeviceManager audioDeviceManager;
#else
    AudioDeviceManager& audioDeviceManager { getSharedAudioDeviceManager (1, 0) };
#endif

    LiveScrollingAudioDisplay liveAudioScroller;
    RecordingThumbnail        recordingThumbnail;
    Capteur             recorder { recordingThumbnail.getAudioThumbnail () };

    Label       explanationLabel { {},
                                   "This page demonstrates how to record a wave file from the live audio input.\n\n"
                                   "After you are done with your recording you can choose where to save it." };
    TextButton  recordButton { "Record" };
    TextButton  settingsButton { "Settings" };
    File        lastRecording;
    FileChooser chooser { "Output file...",
                          File::getCurrentWorkingDirectory ().getChildFile ("recording.wav"),
                          "*.wav" };

    void startRecording ()
    {
#if ! JUCE_ANDROID
        if (! RuntimePermissions::isGranted (RuntimePermissions::writeExternalStorage))
        {
            SafePointer<CapteurComponent> safeThis (this);

            RuntimePermissions::request (RuntimePermissions::writeExternalStorage,
                                         [safeThis](bool granted) mutable
                                         {
                                             if (granted)
                                                 safeThis->startRecording ();
                                         });
            return;
        }
#endif

#if (JUCE_ANDROID || JUCE_IOS)
        auto parentDir = File::getSpecialLocation (File::tempDirectory);
#else
        auto parentDir = File::getSpecialLocation (File::userDocumentsDirectory);
#endif

        lastRecording = parentDir.getNonexistentChildFile ("JUCE Demo Audio Recording", ".wav");

        recorder.startRecording (lastRecording);

        recordButton.setButtonText ("Stop");
        recordingThumbnail.setDisplayFullThumbnail (false);
    }

    void stopRecording ()
    {
        recorder.stop ();

        chooser.launchAsync (FileBrowserComponent::saveMode | FileBrowserComponent::canSelectFiles
                             | FileBrowserComponent::warnAboutOverwriting,
                             [this](const FileChooser& c)
                             {
                                 if (FileInputStream inputStream (lastRecording); inputStream.openedOk ())
                                     if (const auto outputStream = makeOutputStream (c.getURLResult ()))
                                         outputStream->writeFromInputStream (inputStream, -1);

                                 recordButton.setButtonText ("Record");
                                 recordingThumbnail.setDisplayFullThumbnail (true);
                             });
    }

    void showAudioSettings ()
    {
        auto selector = std::make_unique<AudioDeviceSelectorComponent> (
            audioDeviceManager, 0, 2, 0, 2, false, false, true, false);

        // Adjust size based on the current window size
        auto width = jmin (500, (int) (getWidth () * 0.95f));
        auto height = jmin (600, (int) (getHeight () * 0.95f));
        selector->setSize (width, height);

        DialogWindow::LaunchOptions options;
        options.content.setOwned (selector.release ());
        options.dialogTitle = "Audio Settings";
        options.dialogBackgroundColour
            = getUIColourIfAvailable (LookAndFeel_V4::ColourScheme::UIColour::windowBackground);
        options.escapeKeyTriggersCloseButton = true;
        options.useNativeTitleBar = false; // Use JUCE title bar to ensure a close button is visible
        options.resizable = false;
        options.componentToCentreAround = this;

        options.launchAsync ();
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CapteurComponent)
};
