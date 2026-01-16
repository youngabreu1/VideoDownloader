#pragma once
#include <JuceHeader.h>

class MainComponent : public juce::Component
{
public:
    MainComponent();
    ~MainComponent() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    juce::Label urlLabel;
    juce::TextEditor urlInput;

    juce::Label formatLabel;
    juce::ComboBox formatBox;

    juce::Label qualityLabel;
    juce::ComboBox qualityBox;

    juce::TextButton chooseDirBtn;
    juce::Label selectedDirLabel;

    juce::TextButton downloadBtn;
    juce::Label statusLabel;

    juce::File downloadFolder;
    std::unique_ptr<juce::FileChooser> fileChooser;

    void chooseFolder();
    void startDownload();
    void runYtDlpCommand(juce::String url, juce::String path, bool isAudioOnly, juce::String heightLimit);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};