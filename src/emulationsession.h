#pragma once

#include <QString>

#include <memory>
#include <vector>

enum class EmulatorShortcut;
class Emulator;
class OpenGLVideoWidget;
class QtRenderingDevice;
class QtKeyManager;
class SdlSoundManager;

struct GameSettings
{
    uint32_t dipSwitchCount = 0;
    uint32_t dipSwitches = 0;
    bool overrideOverscan = false;
    uint32_t overscanLeft = 0;
    uint32_t overscanRight = 0;
    uint32_t overscanTop = 0;
    uint32_t overscanBottom = 0;
};

struct FrontendSettings
{
    uint32_t emulationSpeed = 100;
    uint32_t turboSpeed = 300;
    int videoFilter = 0;
    int aspectRatio = 0;
    bool bilinearInterpolation = false;
    bool verticalSync = false;
    bool showFps = false;
};

struct FrontendCheat
{
    QString description;
    int type = 0;
    bool enabled = true;
    QString codes;
};

struct CheatTypeOption
{
    QString name;
    int type = 0;
};

class EmulationSession final
{
public:
    explicit EmulationSession(OpenGLVideoWidget &videoWidget);
    ~EmulationSession();

    EmulationSession(const EmulationSession &) = delete;
    EmulationSession &operator=(const EmulationSession &) = delete;

    bool loadRom(const QString &filePath, QString &errorMessage);
    bool isRunning() const;
    bool isPaused() const;
    bool isShortcutAllowed(EmulatorShortcut shortcut, uint32_t parameter = 0) const;

    void saveState(uint32_t slot);
    bool loadState(uint32_t slot);
    bool saveStateFile(const QString &filePath);
    bool loadStateFile(const QString &filePath);
    QString stateFilePath(uint32_t slot) const;
    QString lastSessionFilePath() const;
    void loadLastSession();
    bool loadRecentSession(const QString &filePath, QString &errorMessage);
    QString currentRomPath() const;

    void togglePause();
    void reset();
    void powerCycle();
    void reloadRom();
    void powerOff();
    void setInBackground(bool inBackground);
    void executeShortcut(EmulatorShortcut shortcut, uint32_t parameter = 0);
    void inputBarcode(uint64_t barcode, uint32_t digitCount);
    void playTape(const QString &filePath);
    void startTapeRecording(const QString &filePath);
    void stopTapeRecording();
    bool supportsGameSettings() const;
    GameSettings gameSettings() const;
    void applyGameSettings(const GameSettings &settings);
    FrontendSettings frontendSettings() const;
    void applyFrontendSettings(const FrontendSettings &settings);
    bool supportsCheats() const;
    QString cheatDatabaseName() const;
    QString cheatDatabaseHash() const;
    QString cheatFilePath() const;
    std::vector<CheatTypeOption> cheatTypeOptions() const;
    bool validateCheat(const FrontendCheat &cheat, QString &errorMessage) const;
    bool applyCheats(const std::vector<FrontendCheat> &cheats, bool disabled, QString &errorMessage);

private:
    void loadFrontendSettings();
    void loadGameSettings();
    void loadCheats();
    QString gameSettingsKey() const;
    QString sessionFilePath(const QString &romPath) const;

    std::unique_ptr<Emulator> _emulator;
    std::unique_ptr<QtKeyManager> _keyManager;
    std::unique_ptr<QtRenderingDevice> _renderingDevice;
    std::unique_ptr<SdlSoundManager> _soundManager;
};