#include "emulationsession.h"

#include "frontenddefaults.h"
#include "openglvideowidget.h"
#include "qtkeymanager.h"

#include "Core/Shared/CheatManager.h"
#include "Core/Shared/EmuSettings.h"
#include "Core/Shared/Emulator.h"
#include "Core/Shared/Interfaces/ITapeRecorder.h"
#include "Core/Shared/KeyManager.h"
#include "Core/Shared/NotificationManager.h"
#include "Core/Shared/RomInfo.h"
#include "Core/Shared/SaveStateManager.h"
#include "Core/Shared/SettingTypes.h"
#include "Core/Shared/ShortcutKeyHandler.h"
#include "Sdl/SdlSoundManager.h"
#include "Utilities/FolderUtilities.h"
#include "Utilities/VirtualFile.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>

#include <cstring>

EmulationSession::EmulationSession(OpenGLVideoWidget &videoWidget)
    : _emulator(std::make_unique<Emulator>())
{
    QString homeFolder = QStandardPaths::writableLocation(QStandardPaths::HomeLocation) + QDir::separator() + ".mesen";
    QDir().mkpath(homeFolder);
    FolderUtilities::SetHomeFolder(homeFolder.toStdString());

    _keyManager = std::make_unique<QtKeyManager>(videoWidget);
    KeyManager::SetSettings(_emulator->GetSettings());
    KeyManager::RegisterKeyManager(_keyManager.get());
    FrontendDefaults::apply(*_emulator->GetSettings(), *_keyManager);
    loadFrontendSettings();
    _emulator->Initialize(true);
    _renderingDevice = std::make_unique<QtRenderingDevice>(*_emulator, videoWidget);
    _soundManager = std::make_unique<SdlSoundManager>(_emulator.get());
}

EmulationSession::~EmulationSession()
{
    _emulator->Release();
    _soundManager.reset();
    _renderingDevice.reset();
    KeyManager::RegisterKeyManager(nullptr);
    _keyManager.reset();
}

bool EmulationSession::loadRom(const QString &filePath, QString &errorMessage)
{
    const QFileInfo fileInfo(filePath);
    if (!fileInfo.isFile()) {
        errorMessage = QObject::tr("文件不存在：%1").arg(QDir::toNativeSeparators(filePath));
        return false;
    }

    const QByteArray encodedPath = QFile::encodeName(fileInfo.absoluteFilePath());
    const QString recentSession = sessionFilePath(fileInfo.absoluteFilePath());
    bool loaded = false;
    if (QFileInfo::exists(recentSession)) {
        _emulator->GetSaveStateManager()->LoadRecentGame(QFile::encodeName(recentSession).constData(), false);
        const QString loadedPath = QFileInfo(QFile::decodeName(QByteArray::fromStdString(_emulator->GetRomInfo().RomFile.GetFilePath()))).absoluteFilePath();
        loaded = _emulator->IsRunning() && loadedPath == fileInfo.absoluteFilePath();
    }

    if (!loaded && !_emulator->LoadRom(VirtualFile(encodedPath.constData()), VirtualFile())) {
        errorMessage = QObject::tr("Mesen 无法识别或加载该游戏文件。文件可能损坏，或需要额外的固件。");
        return false;
    }

    loadGameSettings();
    loadCheats();
    return true;
}

bool EmulationSession::isRunning() const
{
    return _emulator->IsRunning();
}

bool EmulationSession::isPaused() const
{
    return _emulator->IsPaused();
}

bool EmulationSession::isShortcutAllowed(EmulatorShortcut shortcut, uint32_t parameter) const
{
    return _emulator->GetShortcutKeyHandler()->IsShortcutAllowed(shortcut, parameter);
}

void EmulationSession::saveState(uint32_t slot)
{
    _emulator->GetSaveStateManager()->SaveState(static_cast<int>(slot));
}

bool EmulationSession::loadState(uint32_t slot)
{
    return _emulator->GetSaveStateManager()->LoadState(static_cast<int>(slot));
}

bool EmulationSession::saveStateFile(const QString &filePath)
{
    return _emulator->GetSaveStateManager()->SaveState(QFile::encodeName(filePath).constData());
}

bool EmulationSession::loadStateFile(const QString &filePath)
{
    return _emulator->GetSaveStateManager()->LoadState(QFile::encodeName(filePath).constData());
}

QString EmulationSession::stateFilePath(uint32_t slot) const
{
    const std::string romName = _emulator->GetRomInfo().RomFile.GetFileName();
    const std::string baseName = FolderUtilities::GetFilename(romName, false);
    return QString::fromStdString(FolderUtilities::CombinePath(
        FolderUtilities::GetSaveStateFolder(), baseName + "_" + std::to_string(slot) + ".mss"));
}

QString EmulationSession::lastSessionFilePath() const
{
    return sessionFilePath(QFile::decodeName(QByteArray::fromStdString(_emulator->GetRomInfo().RomFile.GetFilePath())));
}

QString EmulationSession::sessionFilePath(const QString &romPath) const
{
    const std::string baseName = FolderUtilities::GetFilename(QFile::encodeName(romPath).constData(), false);
    return QString::fromStdString(FolderUtilities::CombinePath(FolderUtilities::GetRecentGamesFolder(), baseName + ".rgd"));
}

void EmulationSession::loadLastSession()
{
    _emulator->GetSaveStateManager()->LoadRecentGame(QFile::encodeName(lastSessionFilePath()).constData(), false);
}

bool EmulationSession::loadRecentSession(const QString &filePath, QString &errorMessage)
{
    _emulator->GetSaveStateManager()->LoadRecentGame(QFile::encodeName(filePath).constData(), false);
    if (!_emulator->IsRunning()) {
        errorMessage = QObject::tr("无法加载最近游戏进度。文件可能已损坏，或原游戏文件已被移动。");
        return false;
    }

    loadGameSettings();
    loadCheats();
    return true;
}

QString EmulationSession::currentRomPath() const
{
    return QFile::decodeName(QByteArray::fromStdString(_emulator->GetRomInfo().RomFile.GetFilePath()));
}

void EmulationSession::togglePause()
{
    if (_emulator->IsPaused()) {
        _emulator->Resume();
    } else {
        _emulator->Pause();
    }
}

void EmulationSession::reset()
{
    executeShortcut(EmulatorShortcut::ExecReset);
}

void EmulationSession::powerCycle()
{
    executeShortcut(EmulatorShortcut::ExecPowerCycle);
}

void EmulationSession::reloadRom()
{
    executeShortcut(EmulatorShortcut::ExecReloadRom);
}

void EmulationSession::powerOff()
{
    executeShortcut(EmulatorShortcut::ExecPowerOff);
}

void EmulationSession::setInBackground(bool inBackground)
{
    _emulator->GetSettings()->SetFlagState(EmulationFlags::InBackground, inBackground);
}

void EmulationSession::executeShortcut(EmulatorShortcut shortcut, uint32_t parameter)
{
    ExecuteShortcutParams params = {shortcut, parameter, nullptr};
    _emulator->GetNotificationManager()->SendNotification(ConsoleNotificationType::ExecuteShortcut, &params);
}

void EmulationSession::inputBarcode(uint64_t barcode, uint32_t digitCount)
{
    _emulator->InputBarcode(barcode, digitCount);
}

void EmulationSession::playTape(const QString &filePath)
{
    _emulator->ProcessTapeRecorderAction(TapeRecorderAction::Play, QFile::encodeName(filePath).constData());
}

void EmulationSession::startTapeRecording(const QString &filePath)
{
    _emulator->ProcessTapeRecorderAction(TapeRecorderAction::StartRecord, QFile::encodeName(filePath).constData());
}

void EmulationSession::stopTapeRecording()
{
    _emulator->ProcessTapeRecorderAction(TapeRecorderAction::StopRecord, {});
}

FrontendSettings EmulationSession::frontendSettings() const
{
    const EmulationConfig &emulation = _emulator->GetSettings()->GetEmulationConfig();
    const VideoConfig &video = _emulator->GetSettings()->GetVideoConfig();
    const PreferencesConfig &preferences = _emulator->GetSettings()->GetPreferences();
    return {
        emulation.EmulationSpeed,
        emulation.TurboSpeed,
        static_cast<int>(video.VideoFilter),
        static_cast<int>(video.AspectRatio),
        video.UseBilinearInterpolation,
        video.VerticalSync,
        preferences.ShowFps,
    };
}

void EmulationSession::applyFrontendSettings(const FrontendSettings &settings)
{
    EmulationConfig emulation = _emulator->GetSettings()->GetEmulationConfig();
    emulation.EmulationSpeed = settings.emulationSpeed;
    emulation.TurboSpeed = settings.turboSpeed;
    emulation.RunAheadFrames = 0;
    _emulator->GetSettings()->SetEmulationConfig(emulation);

    VideoConfig video = _emulator->GetSettings()->GetVideoConfig();
    video.VideoFilter = static_cast<VideoFilterType>(settings.videoFilter);
    video.AspectRatio = static_cast<VideoAspectRatio>(settings.aspectRatio);
    video.UseBilinearInterpolation = settings.bilinearInterpolation;
    video.VerticalSync = settings.verticalSync;
    _emulator->GetSettings()->SetVideoConfig(video);

    PreferencesConfig preferences = _emulator->GetSettings()->GetPreferences();
    preferences.ShowFps = settings.showFps;
    _emulator->GetSettings()->SetPreferences(preferences);

    QSettings persistedSettings;
    persistedSettings.beginGroup("settings");
    persistedSettings.setValue("emulationSpeed", settings.emulationSpeed);
    persistedSettings.setValue("turboSpeed", settings.turboSpeed);
    persistedSettings.setValue("videoFilter", settings.videoFilter);
    persistedSettings.setValue("aspectRatio", settings.aspectRatio);
    persistedSettings.setValue("bilinearInterpolation", settings.bilinearInterpolation);
    persistedSettings.setValue("verticalSync", settings.verticalSync);
    persistedSettings.setValue("showFps", settings.showFps);
    persistedSettings.endGroup();
}

void EmulationSession::loadFrontendSettings()
{
    QSettings persistedSettings;
    persistedSettings.beginGroup("settings");
    FrontendSettings settings = frontendSettings();
    settings.emulationSpeed = persistedSettings.value("emulationSpeed", settings.emulationSpeed).toUInt();
    settings.turboSpeed = persistedSettings.value("turboSpeed", settings.turboSpeed).toUInt();
    settings.videoFilter = persistedSettings.value("videoFilter", settings.videoFilter).toInt();
    settings.aspectRatio = persistedSettings.value("aspectRatio", settings.aspectRatio).toInt();
    settings.bilinearInterpolation = persistedSettings.value("bilinearInterpolation", settings.bilinearInterpolation).toBool();
    settings.verticalSync = persistedSettings.value("verticalSync", settings.verticalSync).toBool();
    settings.showFps = persistedSettings.value("showFps", settings.showFps).toBool();
    persistedSettings.endGroup();
    applyFrontendSettings(settings);
}

namespace
{
std::vector<QString> expandCheatCodes(const FrontendCheat &cheat, QString &errorMessage)
{
    std::vector<QString> result;
    const QStringList lines = cheat.codes.split(QRegularExpression("[;\\n\\r]+"), Qt::SkipEmptyParts);
    static const QRegularExpression nesRangePattern("^([0-9A-Fa-f]{4})-([0-9A-Fa-f]{2})-([0-9A-Fa-f]{2})$");
    for (QString line : lines) {
        line = line.trimmed();
        const QRegularExpressionMatch match = nesRangePattern.match(line);
        if (cheat.type == static_cast<int>(CheatType::NesCustom) && match.hasMatch()) {
            bool addressValid = false;
            bool countValid = false;
            const uint32_t address = match.captured(1).toUInt(&addressValid, 16);
            const uint32_t count = match.captured(2).toUInt(&countValid, 16);
            if (!addressValid || !countValid || count == 0 || address + count > 0x10000) {
                errorMessage = QObject::tr("无效的 NES 连续写入代码：%1").arg(line);
                return {};
            }
            for (uint32_t offset = 0; offset < count; ++offset) {
                result.push_back(QString("%1:%2")
                                     .arg(address + offset, 4, 16, QLatin1Char('0'))
                                     .arg(match.captured(3))
                                     .toUpper());
            }
        } else {
            result.push_back(line);
        }
    }
    if (result.empty()) {
        errorMessage = QObject::tr("至少需要一条金手指代码。");
    }
    return result;
}

CheatCode makeCoreCheat(int type, const QString &code)
{
    CheatCode result = {};
    result.Type = static_cast<CheatType>(type);
    const QByteArray encoded = code.toLatin1();
    std::strncpy(result.Code, encoded.constData(), sizeof(result.Code) - 1);
    return result;
}
}

bool EmulationSession::supportsCheats() const
{
    if (!_emulator->IsRunning()) {
        return false;
    }
    switch (_emulator->GetConsoleType()) {
    case ConsoleType::Nes:
    case ConsoleType::Snes:
    case ConsoleType::Gameboy:
    case ConsoleType::PcEngine:
    case ConsoleType::Sms:
        return true;
    default:
        return false;
    }
}

QString EmulationSession::cheatDatabaseName() const
{
    switch (_emulator->GetConsoleType()) {
    case ConsoleType::Nes:
        return "Nes";
    case ConsoleType::Snes:
        return "Snes";
    default:
        return {};
    }
}

QString EmulationSession::cheatDatabaseHash() const
{
    return QString::fromStdString(_emulator->GetHash(HashType::Sha1Cheat));
}

QString EmulationSession::cheatFilePath() const
{
    const std::string romName = FolderUtilities::GetFilename(_emulator->GetRomInfo().RomFile.GetFileName(), false);
    const QString folder = QString::fromStdString(FolderUtilities::CombinePath(FolderUtilities::GetHomeFolder(), "Cheats"));
    QDir().mkpath(folder);
    return QDir(folder).filePath(QString::fromStdString(romName) + ".json");
}

std::vector<CheatTypeOption> EmulationSession::cheatTypeOptions() const
{
    switch (_emulator->GetConsoleType()) {
    case ConsoleType::Nes:
        return {
            {QObject::tr("Game Genie"), static_cast<int>(CheatType::NesGameGenie)},
            {QObject::tr("Pro Action Rocky"), static_cast<int>(CheatType::NesProActionRocky)},
            {QObject::tr("自定义地址"), static_cast<int>(CheatType::NesCustom)},
        };
    case ConsoleType::Snes:
        return {
            {QObject::tr("Game Genie"), static_cast<int>(CheatType::SnesGameGenie)},
            {QObject::tr("Pro Action Replay"), static_cast<int>(CheatType::SnesProActionReplay)},
        };
    case ConsoleType::Gameboy:
        return {
            {QObject::tr("Game Genie"), static_cast<int>(CheatType::GbGameGenie)},
            {QObject::tr("GameShark"), static_cast<int>(CheatType::GbGameShark)},
        };
    case ConsoleType::PcEngine:
        return {
            {QObject::tr("原始代码"), static_cast<int>(CheatType::PceRaw)},
            {QObject::tr("地址代码"), static_cast<int>(CheatType::PceAddress)},
        };
    case ConsoleType::Sms:
        return {
            {QObject::tr("Pro Action Replay"), static_cast<int>(CheatType::SmsProActionReplay)},
            {QObject::tr("Game Genie"), static_cast<int>(CheatType::SmsGameGenie)},
        };
    default:
        return {};
    }
}

bool EmulationSession::validateCheat(const FrontendCheat &cheat, QString &errorMessage) const
{
    const std::vector<QString> codes = expandCheatCodes(cheat, errorMessage);
    if (codes.empty()) {
        return false;
    }
    for (const QString &code : codes) {
        if (code.toLatin1().size() >= static_cast<int>(sizeof(CheatCode::Code))) {
            errorMessage = QObject::tr("代码过长：%1").arg(code);
            return false;
        }
        InternalCheatCode converted = {};
        if (!_emulator->GetCheatManager()->GetConvertedCheat(makeCoreCheat(cheat.type, code), converted)) {
            errorMessage = QObject::tr("无法识别代码：%1").arg(code);
            return false;
        }
    }
    return true;
}

bool EmulationSession::applyCheats(const std::vector<FrontendCheat> &cheats, bool disabled, QString &errorMessage)
{
    std::vector<CheatCode> coreCheats;
    if (!disabled) {
        for (const FrontendCheat &cheat : cheats) {
            if (!cheat.enabled) {
                continue;
            }
            const std::vector<QString> codes = expandCheatCodes(cheat, errorMessage);
            if (codes.empty()) {
                return false;
            }
            for (const QString &code : codes) {
                InternalCheatCode converted = {};
                CheatCode coreCheat = makeCoreCheat(cheat.type, code);
                if (!_emulator->GetCheatManager()->GetConvertedCheat(coreCheat, converted)) {
                    errorMessage = QObject::tr("无法识别代码：%1").arg(code);
                    return false;
                }
                coreCheats.push_back(coreCheat);
            }
        }
    }
    _emulator->GetCheatManager()->SetCheats(coreCheats);
    return true;
}

void EmulationSession::loadCheats()
{
    QFile file(cheatFilePath());
    if (!file.open(QIODevice::ReadOnly)) {
        _emulator->GetCheatManager()->ClearCheats(false);
        return;
    }
    const QJsonArray values = QJsonDocument::fromJson(file.readAll()).object().value("cheats").toArray();
    std::vector<FrontendCheat> cheats;
    cheats.reserve(static_cast<size_t>(values.size()));
    for (const QJsonValue &value : values) {
        const QJsonObject object = value.toObject();
        cheats.push_back({
            object.value("description").toString(),
            object.value("type").toInt(),
            object.value("enabled").toBool(true),
            object.value("codes").toString(),
        });
    }
    QString errorMessage;
    applyCheats(cheats, QSettings().value("cheats/disableAll", false).toBool(), errorMessage);
}

bool EmulationSession::supportsGameSettings() const
{
    if (!_emulator->IsRunning()) {
        return false;
    }
    const RomFormat format = _emulator->GetRomInfo().Format;
    return _emulator->GetConsoleType() != ConsoleType::Gameboy && _emulator->GetConsoleType() != ConsoleType::Gba && format != RomFormat::GameGear;
}

GameSettings EmulationSession::gameSettings() const
{
    const GameConfig &config = _emulator->GetSettings()->GetGameConfig();
    const DipSwitchInfo dipInfo = _emulator->GetRomInfo().DipSwitches;
    return {
        dipInfo.DipSwitchCount,
        config.DipSwitches,
        config.OverrideOverscan,
        config.Overscan.Left,
        config.Overscan.Right,
        config.Overscan.Top,
        config.Overscan.Bottom,
    };
}

void EmulationSession::applyGameSettings(const GameSettings &settings)
{
    GameConfig config = _emulator->GetSettings()->GetGameConfig();
    config.DipSwitches = settings.dipSwitches;
    config.OverrideOverscan = settings.overrideOverscan;
    config.Overscan = {settings.overscanLeft, settings.overscanRight, settings.overscanTop, settings.overscanBottom};
    _emulator->GetSettings()->SetGameConfig(config);

    QSettings persistedSettings;
    persistedSettings.beginGroup(gameSettingsKey());
    persistedSettings.setValue("dipSwitches", settings.dipSwitches);
    persistedSettings.setValue("overrideOverscan", settings.overrideOverscan);
    persistedSettings.setValue("overscanLeft", settings.overscanLeft);
    persistedSettings.setValue("overscanRight", settings.overscanRight);
    persistedSettings.setValue("overscanTop", settings.overscanTop);
    persistedSettings.setValue("overscanBottom", settings.overscanBottom);
    persistedSettings.endGroup();
}

void EmulationSession::loadGameSettings()
{
    QSettings persistedSettings;
    persistedSettings.beginGroup(gameSettingsKey());
    GameSettings settings = gameSettings();
    settings.dipSwitches = persistedSettings.value("dipSwitches", settings.dipSwitches).toUInt();
    settings.overrideOverscan = persistedSettings.value("overrideOverscan", settings.overrideOverscan).toBool();
    settings.overscanLeft = persistedSettings.value("overscanLeft", settings.overscanLeft).toUInt();
    settings.overscanRight = persistedSettings.value("overscanRight", settings.overscanRight).toUInt();
    settings.overscanTop = persistedSettings.value("overscanTop", settings.overscanTop).toUInt();
    settings.overscanBottom = persistedSettings.value("overscanBottom", settings.overscanBottom).toUInt();
    persistedSettings.endGroup();
    applyGameSettings(settings);
}

QString EmulationSession::gameSettingsKey() const
{
    return QString("gameSettings/%1").arg(_emulator->GetCrc32(), 8, 16, QLatin1Char('0'));
}