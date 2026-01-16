#include "MainComponent.h"

MainComponent::MainComponent()
{
    // 1. Configurar Input de URL
    addAndMakeVisible(urlLabel);
    urlLabel.setText("URL do YouTube:", juce::dontSendNotification);
    addAndMakeVisible(urlInput);
    urlInput.setText("Cole o link aqui...");

    // 2. Configurar Formato (MP4 vs MP3)
    addAndMakeVisible(formatLabel);
    formatLabel.setText("Formato:", juce::dontSendNotification);
    addAndMakeVisible(formatBox);
    formatBox.addItem("Video (MP4)", 1);
    formatBox.addItem("Audio (MP3)", 2);
    formatBox.setSelectedId(1); // Padrão MP4

    // 3. Configurar Qualidade (Resolução máxima permitida)
    addAndMakeVisible(qualityLabel);
    qualityLabel.setText("Resolucao Max:", juce::dontSendNotification);
    addAndMakeVisible(qualityBox);
    qualityBox.addItem("Melhor Disponivel (4K+)", 1);
    qualityBox.addItem("1080p", 2);
    qualityBox.addItem("720p", 3);
    qualityBox.setSelectedId(1);

    // 4. Botões e Status
    addAndMakeVisible(chooseDirBtn);
    chooseDirBtn.setButtonText("Salvar em...");
    chooseDirBtn.onClick = [this] { chooseFolder(); };

    addAndMakeVisible(selectedDirLabel);
    selectedDirLabel.setText("Nenhuma pasta selecionada", juce::dontSendNotification);

    addAndMakeVisible(downloadBtn);
    downloadBtn.setButtonText("BAIXAR");
    downloadBtn.onClick = [this] { startDownload(); };

    addAndMakeVisible(statusLabel);
    statusLabel.setText("Pronto.", juce::dontSendNotification);

    setSize(600, 400);
}

MainComponent::~MainComponent() {}

void MainComponent::paint(juce::Graphics& g)
{
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
}

void MainComponent::resized()
{
    // Layout simples usando setBounds (x, y, largura, altura)
    int margin = 20;

    urlLabel.setBounds(margin, margin, 100, 24);
    urlInput.setBounds(margin, 50, getWidth() - (margin * 2), 30);

    formatLabel.setBounds(margin, 100, 80, 24);
    formatBox.setBounds(margin, 125, 150, 30);

    qualityLabel.setBounds(200, 100, 100, 24);
    qualityBox.setBounds(200, 125, 150, 30);

    chooseDirBtn.setBounds(margin, 180, 150, 30);
    selectedDirLabel.setBounds(180, 180, 300, 30);

    downloadBtn.setBounds(margin, 250, getWidth() - (margin * 2), 50);
    statusLabel.setBounds(margin, 310, getWidth() - (margin * 2), 30);
}

void MainComponent::chooseFolder()
{
    fileChooser = std::make_unique<juce::FileChooser>("Escolha onde salvar", juce::File::getSpecialLocation(juce::File::userHomeDirectory));

    auto folderFlags = juce::FileBrowserComponent::canSelectDirectories |
        juce::FileBrowserComponent::openMode;

    fileChooser->launchAsync(folderFlags, [this](const juce::FileChooser& fc)
        {
            auto file = fc.getResult();
            if (file.exists())
            {
                downloadFolder = file;
                selectedDirLabel.setText(file.getFullPathName(), juce::dontSendNotification);
            }
        });
}

void MainComponent::startDownload()
{
    juce::String url = urlInput.getText();

    if (url.isEmpty() || !downloadFolder.exists()) {
        statusLabel.setText("Erro: Verifique a URL e a Pasta.", juce::dontSendNotification);
        return;
    }

    bool isAudio = (formatBox.getSelectedId() == 2);

    // Define limite de altura (resolução)
    juce::String heightLimit;
    switch (qualityBox.getSelectedId()) {
    case 2: heightLimit = "[height<=1080]"; break;
    case 3: heightLimit = "[height<=720]"; break;
    default: heightLimit = ""; break; // Sem limite (4K/8K)
    }

    runYtDlpCommand(url, downloadFolder.getFullPathName(), isAudio, heightLimit);
}

void MainComponent::runYtDlpCommand(juce::String url, juce::String path, bool isAudioOnly, juce::String heightLimit)
{
    // 1. Localizar os executáveis na pasta atual do App
    juce::File currentApp = juce::File::getSpecialLocation(juce::File::currentExecutableFile);
    juce::File appDir = currentApp.getParentDirectory();

    juce::File ytDlpExe = appDir.getChildFile("yt-dlp.exe");
    juce::File ffmpegExe = appDir.getChildFile("ffmpeg.exe");

    // 2. Verificação de Segurança (CRUCIAL)
    if (!ytDlpExe.exists())
    {
        statusLabel.setText("ERRO: yt-dlp.exe sumiu!", juce::dontSendNotification);
        return;
    }

    // Se for baixar MP3, o FFmpeg é OBRIGATÓRIO
    if (isAudioOnly && !ffmpegExe.exists())
    {
        statusLabel.setText("ERRO: ffmpeg.exe faltando. Nao consigo converter para MP3.", juce::dontSendNotification);
        return;
    }

    statusLabel.setText("Baixando e Convertendo...", juce::dontSendNotification);

    // 3. Montagem do Comando (Com aspas para evitar erro de espaço)
    juce::String ytDlpPath = "\"" + ytDlpExe.getFullPathName() + "\"";
    juce::String ffmpegPath = "\"" + ffmpegExe.getFullPathName() + "\"";
    juce::String outputTemplate = path + "/%(title)s.%(ext)s";

    juce::String args;

    if (isAudioOnly)
    {
        // --ffmpeg-location é o segredo aqui. 
        // --audio-format mp3 força a conversão final.
        args = " --ffmpeg-location " + ffmpegPath + " -x --audio-format mp3 --audio-quality 0 -o \"" + outputTemplate + "\" " + url;
    }
    else
    {
        // Alteração para compatibilidade com Windows Media Player:
        // Pedimos explicitamente video com extensão mp4 (geralmente H.264) 
        // e audio com extensão m4a (geralmente AAC).

        juce::String formatSelector;

        if (heightLimit.isEmpty()) {
            // Se for 4K (sem limite), aceitamos qualquer coisa, mas preferimos mp4
            formatSelector = "bestvideo[ext=mp4]+bestaudio[ext=m4a]/best[ext=mp4]/best";
        }
        else {
            // Se for 1080p ou 720p, forçamos o padrão compatível
            formatSelector = "bestvideo[ext=mp4]" + heightLimit + "+bestaudio[ext=m4a]/best[ext=mp4]/best";
        }

        // O comando --merge-output-format mp4 garante que, no final, tudo fique num container mp4
        args = " --ffmpeg-location " + ffmpegPath + " -f \"" + formatSelector + "\" --merge-output-format mp4 -o \"" + outputTemplate + "\" " + url;
    }

    // 4. Executar
    juce::String finalCommand = ytDlpPath + args;
    DBG("Executando: " + finalCommand); // Veja no Output do Visual Studio

    juce::ChildProcess process;
    if (process.start(finalCommand))
    {
        juce::String output = process.readAllProcessOutput();
        int exitCode = process.getExitCode();

        if (exitCode == 0)
            statusLabel.setText("Sucesso! Arquivo salvo.", juce::dontSendNotification);
        else
            // Mostra o erro real se falhar
            statusLabel.setText("Falha na conversao (Veja Logs)", juce::dontSendNotification);

        // Dica: Imprima o output no console para ver o erro do FFmpeg se houver
        DBG(output);
    }
    else
    {
        statusLabel.setText("Nao foi possivel iniciar o processo.", juce::dontSendNotification);
    }
}