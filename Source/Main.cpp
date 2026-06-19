#include <JuceHeader.h>
#include "AudioRecordingDemo.h"

/**
 * @class `MainCapteurWindow`
 * @brief Main application window for the Capteur app.
 *
 * The `MainCapteurWindow` is a top-level `juce::DocumentWindow` that hosts the
 * application's main content component. It performs platform-specific setup to
 * ensure correct presentation and behaviour:
 *
 * - On mobile platforms (`JUCE_ANDROID` or `JUCE_IOS`) the content is wrapped
 *   in a `SafeAreaComponent` to apply display safe-area insets (avoiding notches,
 *   rounded corners and system indicators) and the window is set to fullscreen.
 * - On desktop platforms the window is made resizable, given sensible resize
 *   limits, and centered on screen.
 *
 * Constructor parameters:
 * - `name` : The window title shown in the title bar.
 * - `content` : A `std::unique_ptr<juce::Component>` owning the main UI component.
 * - `app` : Reference to the hosting `JUCEApplication` used to signal application quit.
 */
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
