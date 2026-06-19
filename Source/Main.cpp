#include <JuceHeader.h>
#include "AudioRecordingDemo.h"

class MainCapteurWindow : public juce::DocumentWindow
{
public:
    MainCapteurWindow (const juce::String& name, std::unique_ptr<juce::Component> content, JUCEApplication& app)
        : DocumentWindow (
            name,
            juce::Desktop::getInstance ().getDefaultLookAndFeel ().findColour (ResizableWindow::backgroundColourId),
            juce::DocumentWindow::allButtons),
        capteurApp (app)
    {
        setUsingNativeTitleBar (true);

#if JUCE_ANDROID || JUCE_IOS
        setContentOwned (new SafeAreaComponent { std::move (c) }, true);
        setFullScreen (true);
#else
        setContentOwned (content.release (), true);
        setResizable (true, false);
        setResizeLimits (300, 250, 10000, 10000);
        centreWithSize (getWidth (), getHeight ());
#endif

        setVisible (true);
    }

    void closeButtonPressed () override { capteurApp.systemRequestedQuit (); }

#if JUCE_ANDROID || JUCE_IOS
    class SafeAreaComponent : public juce::Component
    {
    public:
        explicit SafeAreaComponent (std::unique_ptr<Component> c) : content (std::move (c))
        {
            addAndMakeVisible (*content);
        }

        void resized () override
        {
            if (const auto* d = Desktop::getInstance ().getDisplays ().getDisplayForRect (getLocalBounds ()))
                content->setBounds (d->safeAreaInsets.subtractedFrom (getLocalBounds ()));
        }

    private:
        std::unique_ptr<Component> content;
    };

    void parentSizeChanged () override
    {
        if (auto* c = getContentComponent ())
            c->resized ();
    }
#endif

private:
    JUCEApplication& capteurApp;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainCapteurWindow)
};

//==============================================================================

class Application : public juce::JUCEApplication
{
public:
    Application() = default;

    const juce::String getApplicationName() override { return "Capteur"; }
    const juce::String getApplicationVersion() override { return "1.0.0"; }

    void initialise (const juce::String&) override
    {
        mainWindow.reset (new MainCapteurWindow ("Capteur", std::make_unique<AudioRecordingDemo>(), *this));
    }

    void shutdown() override { mainWindow = nullptr; }

private:
    std::unique_ptr<MainCapteurWindow> mainWindow;
};

//==============================================================================
START_JUCE_APPLICATION (Application)
