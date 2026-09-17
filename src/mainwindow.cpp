#include "mainwindow.h"

#include "cheatdialog.h"
#include "emulationsession.h"
#include "gamesettingsdialog.h"
#include "openglvideowidget.h"
#include "recentfiles.h"
#include "recentgameswidget.h"

#include "Core/Shared/SettingTypes.h"

#include <QAction>
#include <QApplication>
#include <QDateTime>
#include <QFileDialog>
#include <QFileInfo>
#include <QInputDialog>
#include <QKeySequence>
#include <QLocale>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QMouseEvent>
#include <QSettings>
#include <QStackedWidget>
#include <QStatusBar>
#include <QTimer>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , _videoWidget(new OpenGLVideoWidget(this))
    , _session(std::make_unique<EmulationSession>(*_videoWidget))
    , _recentFiles(std::make_unique<RecentFiles>())
{
    _recentGamesWidget = new RecentGamesWidget(
        [this](const QString &filePath) {
            loadRecentSession(filePath);
        },
        [this] {
            chooseGame();
        },
        this);
    _contentStack = new QStackedWidget(this);
    _contentStack->addWidget(_recentGamesWidget);
    _contentStack->addWidget(_videoWidget);
    _contentStack->setCurrentWidget(_recentGamesWidget);
    setCentralWidget(_contentStack);
    _videoWidget->setMouseTracking(true);
    _videoWidget->installEventFilter(this);
    resize(768, 720);
    setMinimumSize(320, 280);

    createFileMenu();
    createGameMenu();
    createSettingsMenu();
    connect(qApp, &QGuiApplication::applicationStateChanged, this, [this](Qt::ApplicationState state) {
        _session->setInBackground(state != Qt::ApplicationActive);
    });
    _session->setInBackground(qApp->applicationState() != Qt::ApplicationActive);

    statusBar()->showMessage(tr("请选择一个游戏文件"));
    updateWindowTitle();
}

void MainWindow::createSettingsMenu()
{
    QMenu *settingsMenu = menuBar()->addMenu(tr("设置(&S)"));
    _speedMenu = settingsMenu->addMenu(tr("速度"));
    for (const auto &speed : {std::pair<const char *, int>("正常 (100%)", 100), {"最大速度", 0}, {"三倍速 (300%)", 300}, {"双倍速 (200%)", 200}, {"半速 (50%)", 50}, {"四分之一速 (25%)", 25}}) {
        QAction *action = _speedMenu->addAction(tr(speed.first));
        action->setCheckable(true);
        action->setData(speed.second);
        connect(action, &QAction::triggered, this, [this, speed] {
            FrontendSettings settings = _session->frontendSettings();
            settings.emulationSpeed = static_cast<uint32_t>(speed.second);
            _session->applyFrontendSettings(settings);
        });
        _speedActions.append(action);
    }
    _speedMenu->addSeparator();
    QAction *increaseSpeedAction = _speedMenu->addAction(tr("提高速度"));
    QAction *decreaseSpeedAction = _speedMenu->addAction(tr("降低速度"));
    connect(increaseSpeedAction, &QAction::triggered, this, [this] {
        static constexpr std::array<uint32_t, 20> SpeedValues = {1, 3, 6, 12, 25, 50, 75, 100, 150, 200, 250, 300, 350, 400, 450, 500, 750, 1000, 2000, 4000};
        FrontendSettings settings = _session->frontendSettings();
        if (settings.emulationSpeed == SpeedValues.back()) {
            settings.emulationSpeed = 0;
        } else if (settings.emulationSpeed != 0) {
            auto next = std::find_if(SpeedValues.begin(), SpeedValues.end(), [speed = settings.emulationSpeed](uint32_t value) {
                return value > speed;
            });
            if (next != SpeedValues.end()) {
                settings.emulationSpeed = *next;
            }
        }
        _session->applyFrontendSettings(settings);
    });
    connect(decreaseSpeedAction, &QAction::triggered, this, [this] {
        static constexpr std::array<uint32_t, 20> SpeedValues = {1, 3, 6, 12, 25, 50, 75, 100, 150, 200, 250, 300, 350, 400, 450, 500, 750, 1000, 2000, 4000};
        FrontendSettings settings = _session->frontendSettings();
        if (settings.emulationSpeed == 0) {
            settings.emulationSpeed = SpeedValues.back();
        } else {
            auto previous = std::find_if(SpeedValues.rbegin(), SpeedValues.rend(), [speed = settings.emulationSpeed](uint32_t value) {
                return value < speed;
            });
            if (previous != SpeedValues.rend()) {
                settings.emulationSpeed = *previous;
            }
        }
        _session->applyFrontendSettings(settings);
    });
    _speedMenu->addSeparator();
    _showFpsAction = _speedMenu->addAction(tr("显示 FPS"));
    _showFpsAction->setCheckable(true);
    connect(_showFpsAction, &QAction::triggered, this, [this](bool checked) {
        FrontendSettings settings = _session->frontendSettings();
        settings.showFps = checked;
        _session->applyFrontendSettings(settings);
    });
    _turboSpeedMenu = settingsMenu->addMenu(tr("快进速度"));
    for (int speed : {200, 300, 400, 600, 1000}) {
        QAction *action = _turboSpeedMenu->addAction(tr("%1%").arg(speed));
        action->setCheckable(true);
        action->setData(speed);
        connect(action, &QAction::triggered, this, [this, speed] {
            FrontendSettings settings = _session->frontendSettings();
            settings.turboSpeed = static_cast<uint32_t>(speed);
            _session->applyFrontendSettings(settings);
        });
        _turboSpeedActions.append(action);
    }

    settingsMenu->addSeparator();
    QMenu *scaleMenu = settingsMenu->addMenu(tr("窗口比例"));
    for (int scale = 1; scale <= 10; ++scale) {
        QAction *action = scaleMenu->addAction(tr("%1x").arg(scale));
        connect(action, &QAction::triggered, this, [this, scale] {
            if (isFullScreen()) {
                toggleFullscreen();
            }
            QSize frameSize = _videoWidget->frameSize();
            if (frameSize.isEmpty()) {
                frameSize = QSize(256, 240);
            }
            resize(frameSize.width() * scale, frameSize.height() * scale + menuBar()->height() + statusBar()->height());
        });
    }
    scaleMenu->addSeparator();
    QAction *fullscreenAction = scaleMenu->addAction(tr("全屏"));
    fullscreenAction->setShortcut(Qt::Key_F11);
    connect(fullscreenAction, &QAction::triggered, this, &MainWindow::toggleFullscreen);

    _filterMenu = settingsMenu->addMenu(tr("视频滤镜"));
    const std::initializer_list<std::pair<const char *, VideoFilterType>> filters = {
        {"无", VideoFilterType::None},
        {"NTSC (Blargg)", VideoFilterType::NtscBlargg},
        {"NTSC (Bisqwit)", VideoFilterType::NtscBisqwit},
        {"LCD 网格", VideoFilterType::LcdGrid},
        {"xBRZ 2x", VideoFilterType::xBRZ2x},
        {"xBRZ 3x", VideoFilterType::xBRZ3x},
        {"xBRZ 4x", VideoFilterType::xBRZ4x},
        {"xBRZ 5x", VideoFilterType::xBRZ5x},
        {"xBRZ 6x", VideoFilterType::xBRZ6x},
        {"HQ 2x", VideoFilterType::HQ2x},
        {"HQ 3x", VideoFilterType::HQ3x},
        {"HQ 4x", VideoFilterType::HQ4x},
        {"Scale 2x", VideoFilterType::Scale2x},
        {"Scale 3x", VideoFilterType::Scale3x},
        {"Scale 4x", VideoFilterType::Scale4x},
        {"2xSaI", VideoFilterType::_2xSai},
        {"Super 2xSaI", VideoFilterType::Super2xSai},
        {"Super Eagle", VideoFilterType::SuperEagle},
        {"Prescale 2x", VideoFilterType::Prescale2x},
        {"Prescale 3x", VideoFilterType::Prescale3x},
        {"Prescale 4x", VideoFilterType::Prescale4x},
        {"Prescale 6x", VideoFilterType::Prescale6x},
        {"Prescale 8x", VideoFilterType::Prescale8x},
        {"Prescale 10x", VideoFilterType::Prescale10x},
    };
    for (const auto &filter : filters) {
        QAction *action = _filterMenu->addAction(tr(filter.first));
        action->setCheckable(true);
        action->setData(static_cast<int>(filter.second));
        connect(action, &QAction::triggered, this, [this, filter] {
            FrontendSettings settings = _session->frontendSettings();
            settings.videoFilter = static_cast<int>(filter.second);
            _session->applyFrontendSettings(settings);
        });
        _filterActions.append(action);
    }
    _filterMenu->addSeparator();
    _bilinearAction = _filterMenu->addAction(tr("双线性插值"));
    _bilinearAction->setCheckable(true);
    connect(_bilinearAction, &QAction::triggered, this, [this](bool checked) {
        FrontendSettings settings = _session->frontendSettings();
        settings.bilinearInterpolation = checked;
        _session->applyFrontendSettings(settings);
    });
    _verticalSyncAction = _filterMenu->addAction(tr("垂直同步（重启后生效）"));
    _verticalSyncAction->setCheckable(true);
    connect(_verticalSyncAction, &QAction::triggered, this, [this](bool checked) {
        FrontendSettings settings = _session->frontendSettings();
        settings.verticalSync = checked;
        _session->applyFrontendSettings(settings);
    });

    _aspectRatioMenu = settingsMenu->addMenu(tr("宽高比"));
    const std::initializer_list<std::pair<const char *, VideoAspectRatio>> ratios = {
        {"不拉伸", VideoAspectRatio::NoStretching},
        {"自动", VideoAspectRatio::Auto},
        {"NTSC", VideoAspectRatio::NTSC},
        {"PAL", VideoAspectRatio::PAL},
        {"标准 4:3", VideoAspectRatio::Standard},
        {"宽屏 16:9", VideoAspectRatio::Widescreen},
    };
    for (const auto &ratio : ratios) {
        QAction *action = _aspectRatioMenu->addAction(tr(ratio.first));
        action->setCheckable(true);
        action->setData(static_cast<int>(ratio.second));
        connect(action, &QAction::triggered, this, [this, ratio] {
            FrontendSettings settings = _session->frontendSettings();
            settings.aspectRatio = static_cast<int>(ratio.second);
            _session->applyFrontendSettings(settings);
        });
        _aspectRatioActions.append(action);
    }

    connect(settingsMenu, &QMenu::aboutToShow, this, &MainWindow::updateSettingsMenu);
}

void MainWindow::updateSettingsMenu()
{
    const FrontendSettings settings = _session->frontendSettings();
    for (QAction *action : _speedActions) {
        action->setChecked(action->data().toUInt() == settings.emulationSpeed);
    }
    for (QAction *action : _turboSpeedActions) {
        action->setChecked(action->data().toUInt() == settings.turboSpeed);
    }
    for (QAction *action : _filterActions) {
        action->setChecked(action->data().toInt() == settings.videoFilter);
    }
    for (QAction *action : _aspectRatioActions) {
        action->setChecked(action->data().toInt() == settings.aspectRatio);
    }
    _showFpsAction->setChecked(settings.showFps);
    _bilinearAction->setChecked(settings.bilinearInterpolation);
    _verticalSyncAction->setChecked(settings.verticalSync);
}

MainWindow::~MainWindow() = default;

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == _videoWidget && event->type() == QEvent::MouseMove && isFullScreen()) {
        const auto *mouseEvent = static_cast<QMouseEvent *>(event);
        updateFullscreenChrome(mapFromGlobal(mouseEvent->globalPos()));
    }
    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::toggleFullscreen()
{
    if (isFullScreen()) {
        menuBar()->show();
        statusBar()->show();
        showNormal();
    } else {
        menuBar()->hide();
        statusBar()->hide();
        showFullScreen();
    }
}

void MainWindow::updateFullscreenChrome(const QPoint &position)
{
    constexpr int RevealMargin = 8;
    constexpr int HideMargin = 16;

    const bool showMenu = position.y() <= (menuBar()->isVisible() ? menuBar()->height() + HideMargin : RevealMargin);
    const bool showStatus = position.y() >= height() - (statusBar()->isVisible() ? statusBar()->height() + HideMargin : RevealMargin);
    menuBar()->setVisible(showMenu);
    statusBar()->setVisible(showStatus);
}

void MainWindow::createFileMenu()
{
    QMenu *fileMenu = menuBar()->addMenu(tr("文件(&F)"));
    QAction *openAction = fileMenu->addAction(tr("打开游戏(&O)..."));
    openAction->setShortcut(QKeySequence::Open);
    connect(openAction, &QAction::triggered, this, [this] {
        chooseGame();
    });

    fileMenu->addSeparator();
    _saveStateMenu = fileMenu->addMenu(tr("保存状态"));
    _loadStateMenu = fileMenu->addMenu(tr("加载状态"));
    connect(_saveStateMenu, &QMenu::aboutToShow, this, &MainWindow::rebuildStateMenus);
    connect(_loadStateMenu, &QMenu::aboutToShow, this, &MainWindow::rebuildStateMenus);

    _loadLastSessionAction = fileMenu->addAction(tr("加载上次会话"));
    connect(_loadLastSessionAction, &QAction::triggered, this, [this] {
        _session->loadLastSession();
    });

    fileMenu->addSeparator();
    _recentMenu = fileMenu->addMenu(tr("最近打开的文件"));
    connect(_recentMenu, &QMenu::aboutToShow, this, &MainWindow::rebuildRecentMenu);

    fileMenu->addSeparator();
    QAction *exitAction = fileMenu->addAction(tr("退出(&X)"));
    exitAction->setShortcut(QKeySequence::Quit);
    connect(exitAction, &QAction::triggered, this, &QWidget::close);
    connect(fileMenu, &QMenu::aboutToShow, this, [this] {
        const bool running = _session->isRunning();
        _saveStateMenu->setEnabled(running);
        _loadStateMenu->setEnabled(running);
        _loadLastSessionAction->setEnabled(running && QFileInfo::exists(_session->lastSessionFilePath()));
        _recentMenu->setEnabled(!_recentFiles->items().isEmpty());
    });
}

void MainWindow::createGameMenu()
{
    _gameMenu = menuBar()->addMenu(tr("游戏(&G)"));
    _cheatsAction = _gameMenu->addAction(tr("金手指..."));
    _gameMenu->addSeparator();
    connect(_cheatsAction, &QAction::triggered, this, [this] {
        CheatDialog dialog(*_session, this);
        dialog.exec();
    });
    _pauseAction = _gameMenu->addAction(tr("暂停"));
    _pauseAction->setShortcut(Qt::Key_Pause);
    connect(_pauseAction, &QAction::triggered, this, [this] {
        _session->togglePause();
    });

    _gameMenu->addSeparator();
    auto addGameAction = [this](const QString &text, auto callback) {
        QAction *action = _gameMenu->addAction(text);
        connect(action, &QAction::triggered, this, callback);
        _requiresGameActions.append(action);
        return action;
    };
    addGameAction(tr("重置"), [this] {
        _session->reset();
    });
    addGameAction(tr("重新上电"), [this] {
        _session->powerCycle();
    });
    addGameAction(tr("重新加载游戏"), [this] {
        _session->reloadRom();
    });
    _gameMenu->addSeparator();
    addGameAction(tr("关闭游戏"), [this] {
        _session->powerOff();
        updateWindowTitle();
        statusBar()->showMessage(tr("游戏已关闭"));
        showRecentGames();
    });

    _gameMenu->addSeparator();
    _gameSettingsAction = addGameAction(tr("游戏设置..."), [this] {
        GameSettingsDialog dialog(_session->gameSettings(), this);
        if (dialog.exec() == QDialog::Accepted) {
            _session->applyGameSettings(dialog.settings());
        }
    });

    _gameMenu->addSeparator();
    _diskMenu = _gameMenu->addMenu(tr("选择磁盘"));
    for (uint32_t side = 0; side < 8; ++side) {
        QAction *action = _diskMenu->addAction(tr("磁盘 %1 - %2 面").arg(side / 2 + 1).arg(side % 2 == 0 ? "A" : "B"));
        connect(action, &QAction::triggered, this, [this, side] {
            _session->executeShortcut(EmulatorShortcut::FdsInsertDiskNumber, side);
        });
        _diskActions.append(action);
    }
    _ejectDiskAction = _gameMenu->addAction(tr("弹出磁盘"));
    connect(_ejectDiskAction, &QAction::triggered, this, [this] {
        _session->executeShortcut(EmulatorShortcut::FdsEjectDisk);
    });

    _gameMenu->addSeparator();
    for (int player = 1; player <= 4; ++player) {
        QAction *action = _gameMenu->addAction(tr("投入硬币 %1").arg(player));
        const EmulatorShortcut shortcut = static_cast<EmulatorShortcut>(static_cast<int>(EmulatorShortcut::VsInsertCoin1) + player - 1);
        connect(action, &QAction::triggered, this, [this, shortcut] {
            _session->executeShortcut(shortcut);
        });
        _vsActions.append(action);
    }

    _gameMenu->addSeparator();
    _barcodeAction = _gameMenu->addAction(tr("输入条码..."));
    connect(_barcodeAction, &QAction::triggered, this, &MainWindow::inputBarcode);
    _tapeMenu = _gameMenu->addMenu(tr("磁带录音机"));
    _playTapeAction = _tapeMenu->addAction(tr("播放磁带..."));
    _recordTapeAction = _tapeMenu->addAction(tr("录制磁带..."));
    _stopTapeAction = _tapeMenu->addAction(tr("停止录制"));
    connect(_playTapeAction, &QAction::triggered, this, [this] {
        chooseTapeFile(false);
    });
    connect(_recordTapeAction, &QAction::triggered, this, [this] {
        chooseTapeFile(true);
    });
    connect(_stopTapeAction, &QAction::triggered, this, [this] {
        _session->stopTapeRecording();
    });

    connect(_gameMenu, &QMenu::aboutToShow, this, &MainWindow::updateGameMenu);
}

void MainWindow::chooseGame()
{
    const QString filePath = QFileDialog::getOpenFileName(
        this,
        tr("打开游戏"),
        {},
        tr("游戏文件 (*.nes *.fds *.unf *.unif *.sfc *.smc *.gb *.gbc *.gba *.sms *.gg *.sg *.pce *.cue *.ws *.wsc *.zip *.7z);;所有文件 (*)"));
    if (!filePath.isEmpty()) {
        openGame(filePath);
    }
}

void MainWindow::chooseStateFile(bool save)
{
    const QString filePath = save
        ? QFileDialog::getSaveFileName(this, tr("保存状态"), {}, tr("Mesen 状态文件 (*.mss)"))
        : QFileDialog::getOpenFileName(this, tr("加载状态"), {}, tr("Mesen 状态文件 (*.mss);;所有文件 (*)"));
    if (filePath.isEmpty()) {
        return;
    }

    const bool succeeded = save ? _session->saveStateFile(filePath) : _session->loadStateFile(filePath);
    if (!succeeded) {
        QMessageBox::warning(this, save ? tr("保存失败") : tr("加载失败"), tr("无法处理状态文件：%1").arg(QDir::toNativeSeparators(filePath)));
    }
}

void MainWindow::rebuildStateMenus()
{
    _saveStateMenu->clear();
    _loadStateMenu->clear();
    for (uint32_t slot = 1; slot <= 10; ++slot) {
        const QFileInfo stateInfo(_session->stateFilePath(slot));
        const QString label = stateInfo.exists()
            ? tr("%1. %2").arg(slot).arg(QLocale().toString(stateInfo.lastModified(), QLocale::ShortFormat))
            : tr("%1. 空").arg(slot);
        QAction *saveAction = _saveStateMenu->addAction(label);
        QAction *loadAction = _loadStateMenu->addAction(label);
        loadAction->setEnabled(stateInfo.exists());
        connect(saveAction, &QAction::triggered, this, [this, slot] {
            _session->saveState(slot);
        });
        connect(loadAction, &QAction::triggered, this, [this, slot] {
            _session->loadState(slot);
        });
    }

    _saveStateMenu->addSeparator();
    _loadStateMenu->addSeparator();
    QAction *saveFileAction = _saveStateMenu->addAction(tr("保存到文件..."));
    QAction *loadFileAction = _loadStateMenu->addAction(tr("从文件加载..."));
    connect(saveFileAction, &QAction::triggered, this, [this] {
        chooseStateFile(true);
    });
    connect(loadFileAction, &QAction::triggered, this, [this] {
        chooseStateFile(false);
    });

    _loadStateMenu->insertSeparator(loadFileAction);
    const QFileInfo autoState(_session->stateFilePath(11));
    QAction *autoAction = new QAction(autoState.exists() ? tr("自动. %1").arg(QLocale().toString(autoState.lastModified(), QLocale::ShortFormat)) : tr("自动. 空"), _loadStateMenu);
    autoAction->setEnabled(autoState.exists());
    connect(autoAction, &QAction::triggered, this, [this] {
        _session->loadState(11);
    });
    _loadStateMenu->insertAction(loadFileAction, autoAction);
}

void MainWindow::rebuildRecentMenu()
{
    _recentMenu->clear();
    for (const QString &filePath : _recentFiles->items()) {
        QAction *action = _recentMenu->addAction(QFileInfo(filePath).fileName());
        action->setToolTip(QDir::toNativeSeparators(filePath));
        connect(action, &QAction::triggered, this, [this, filePath] {
            openGame(filePath);
        });
    }
}

void MainWindow::updateGameMenu()
{
    const bool running = _session->isRunning();
    _pauseAction->setEnabled(running);
    _pauseAction->setText(_session->isPaused() ? tr("继续") : tr("暂停"));
    for (QAction *action : _requiresGameActions) {
        action->setEnabled(running);
    }
    _gameSettingsAction->setVisible(_session->supportsGameSettings());
    _cheatsAction->setEnabled(_session->supportsCheats());

    bool anyDisk = false;
    for (int side = 0; side < _diskActions.size(); ++side) {
        const bool allowed = _session->isShortcutAllowed(EmulatorShortcut::FdsInsertDiskNumber, static_cast<uint32_t>(side));
        _diskActions[side]->setVisible(allowed);
        anyDisk |= allowed;
    }
    _diskMenu->menuAction()->setVisible(anyDisk);
    _ejectDiskAction->setVisible(_session->isShortcutAllowed(EmulatorShortcut::FdsEjectDisk));

    for (int index = 0; index < _vsActions.size(); ++index) {
        const EmulatorShortcut shortcut = static_cast<EmulatorShortcut>(static_cast<int>(EmulatorShortcut::VsInsertCoin1) + index);
        _vsActions[index]->setVisible(_session->isShortcutAllowed(shortcut));
    }
    _barcodeAction->setVisible(_session->isShortcutAllowed(EmulatorShortcut::InputBarcode));
    _playTapeAction->setVisible(_session->isShortcutAllowed(EmulatorShortcut::LoadTape));
    _recordTapeAction->setVisible(_session->isShortcutAllowed(EmulatorShortcut::RecordTape));
    _stopTapeAction->setVisible(_session->isShortcutAllowed(EmulatorShortcut::StopRecordTape));
    _tapeMenu->menuAction()->setVisible(_playTapeAction->isVisible() || _recordTapeAction->isVisible() || _stopTapeAction->isVisible());
}

void MainWindow::inputBarcode()
{
    bool accepted = false;
    const QString barcode = QInputDialog::getText(this, tr("输入条码"), tr("条码数字："), QLineEdit::Normal, {}, &accepted).trimmed();
    if (!accepted || barcode.isEmpty()) {
        return;
    }
    bool valid = false;
    const qulonglong value = barcode.toULongLong(&valid);
    if (!valid) {
        QMessageBox::warning(this, tr("无效条码"), tr("条码只能包含数字。"));
        return;
    }
    _session->inputBarcode(value, static_cast<uint32_t>(barcode.size()));
}

void MainWindow::chooseTapeFile(bool record)
{
    const QString filePath = record
        ? QFileDialog::getSaveFileName(this, tr("录制磁带"), {}, tr("磁带文件 (*.tp);;所有文件 (*)"))
        : QFileDialog::getOpenFileName(this, tr("播放磁带"), {}, tr("磁带文件 (*.tp);;所有文件 (*)"));
    if (!filePath.isEmpty()) {
        record ? _session->startTapeRecording(filePath) : _session->playTape(filePath);
    }
}

void MainWindow::openGame(const QString &filePath)
{
    QString errorMessage;
    if (!_session->loadRom(filePath, errorMessage)) {
        QMessageBox::critical(this, tr("无法打开游戏"), errorMessage);
        return;
    }

    _recentFiles->add(filePath);
    _contentStack->setCurrentWidget(_videoWidget);
    updateWindowTitle(filePath);
    statusBar()->showMessage(tr("正在运行 %1").arg(QFileInfo(filePath).fileName()));
}

void MainWindow::showRecentGames()
{
    _contentStack->setCurrentWidget(_recentGamesWidget);
    QTimer::singleShot(250, _recentGamesWidget, [this] {
        _recentGamesWidget->refresh();
    });
}

void MainWindow::loadRecentSession(const QString &filePath)
{
    QString errorMessage;
    if (!_session->loadRecentSession(filePath, errorMessage)) {
        QMessageBox::critical(this, tr("无法恢复游戏"), errorMessage);
        _recentGamesWidget->refresh();
        return;
    }

    const QString romPath = _session->currentRomPath();
    _recentFiles->add(romPath);
    _contentStack->setCurrentWidget(_videoWidget);
    updateWindowTitle(romPath);
    statusBar()->showMessage(tr("已恢复 %1").arg(QFileInfo(romPath).completeBaseName()));
}

void MainWindow::updateWindowTitle(const QString &filePath)
{
    const QString gameName = filePath.isEmpty() ? QString() : QFileInfo(filePath).completeBaseName();
    setWindowTitle(gameName.isEmpty() ? tr("Mesen") : tr("%1 - Mesen").arg(gameName));
}
