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

#include "CapteurComponent.h"

CapteurComponent::CapteurComponent ()
{
    setOpaque (true);
    addAndMakeVisible (liveAudioScroller);

    addAndMakeVisible (explanationLabel);
    explanationLabel.setFont (FontOptions (15.0f, Font::plain));
    explanationLabel.setJustificationType (Justification::topLeft);
    explanationLabel.setEditable (false, false, false);
    explanationLabel.setColour (TextEditor::textColourId, Colours::black);
    explanationLabel.setColour (TextEditor::backgroundColourId, Colour (0x00000000));

    addAndMakeVisible (gainLabel);
    gainLabel.setFont (FontOptions (15.0f, Font::plain));
    gainLabel.setJustificationType (Justification::centredRight);

    addAndMakeVisible (gainSlider);
    gainSlider.setRange (-48.0, 0.0, 0.5);
    gainSlider.setValue (0.0, dontSendNotification);
    gainSlider.setDoubleClickReturnValue (true, 0.0);
    gainSlider.setNumDecimalPlacesToDisplay (1);
    gainSlider.setTextBoxStyle (Slider::TextBoxRight, false, 72, 24);
    gainSlider.setTextValueSuffix (" dB");
    gainSlider.onValueChange = [this]
        {
            const auto gain = Decibels::decibelsToGain (static_cast<float> (gainSlider.getValue()));
            liveAudioScroller.setInputGain (gain);
            recorder.setInputGain (gain);
        };

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

    audioDeviceManager.addChangeListener (this);

    addAndMakeVisible (settingsButton);
    settingsButton.onClick = [this] { showAudioSettings (); };

    addAndMakeVisible (recordingThumbnail);

    RuntimePermissions::request (RuntimePermissions::recordAudio,
                                 [this](bool granted)
                                 {
                                     int numInputChannels = granted ? 2 : 0;
                                     audioDeviceManager.initialise (
                                         numInputChannels, 2, nullptr, true, {}, nullptr);
                                     disableInputPreprocessingIfAvailable ();
                                 });

    audioDeviceManager.addAudioCallback (&liveAudioScroller);
    audioDeviceManager.addAudioCallback (&recorder);

    setSize (500, 500);
}

CapteurComponent::~CapteurComponent ()
{
    audioDeviceManager.removeChangeListener (this);
    audioDeviceManager.removeAudioCallback (&recorder);
    audioDeviceManager.removeAudioCallback (&liveAudioScroller);
}

void CapteurComponent::resized ()
{
    auto area = getLocalBounds ();

    liveAudioScroller.setBounds (area.removeFromTop (80).reduced (8));
    recordingThumbnail.setBounds (area.removeFromTop (80).reduced (8));

    auto gainRow = area.removeFromTop (44);
    gainLabel.setBounds (gainRow.removeFromLeft (100).reduced (8));
    gainSlider.setBounds (gainRow.reduced (8));

    auto buttonRow = area.removeFromTop (36);
    recordButton.setBounds (buttonRow.removeFromLeft (140).reduced (8));
    settingsButton.setBounds (buttonRow.removeFromLeft (140).reduced (8));

    explanationLabel.setBounds (area.reduced (8));
}

void CapteurComponent::startRecording ()
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

    disableInputPreprocessingIfAvailable ();
    recorder.startRecording (lastRecording);

    recordButton.setButtonText ("Stop");
    recordingThumbnail.setDisplayFullThumbnail (false);
}

std::unique_ptr<OutputStream> makeOutputStream (const URL& url)
{
    if (const auto doc = AndroidDocument::fromDocument (url))
        return doc.createOutputStream ();

#if ! JUCE_IOS
    if (url.isLocalFile ())
        return url.getLocalFile ().createOutputStream ();
#endif

    return url.createOutputStream ();
}

void CapteurComponent::stopRecording ()
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

void CapteurComponent::showAudioSettings ()
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

void CapteurComponent::disableInputPreprocessingIfAvailable ()
{
    if (auto* device = audioDeviceManager.getCurrentAudioDevice ())
    {
        const auto accepted = device->setAudioPreprocessingEnabled (false);
        DBG ("Audio input preprocessing disable request on " << device->getTypeName ()
                                                             << (accepted ? " accepted" : " rejected"));
    }
}

void CapteurComponent::changeListenerCallback (ChangeBroadcaster* source)
{
    if (source == &audioDeviceManager)
        disableInputPreprocessingIfAvailable ();
}
