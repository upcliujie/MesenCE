#pragma once

#include <QList>
#include <QMainWindow>

#include <memory>

class QAction;
class EmulationSession;
class QEvent;
class QMenu;
class QStackedWidget;
class OpenGLVideoWidget;
class RecentFiles;
class RecentGamesWidget;

class MainWindow final : public QMainWindow
{
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    void openGame(const QString &filePath);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void createFileMenu();
    void createGameMenu();
    void createSettingsMenu();
    void updateSettingsMenu();
    void chooseGame();
    void chooseStateFile(bool save);
    void rebuildStateMenus();
    void rebuildRecentMenu();
    void updateGameMenu();
    void inputBarcode();
    void chooseTapeFile(bool record);
    void updateWindowTitle(const QString &filePath = {});
    void toggleFullscreen();
    void updateFullscreenChrome(const QPoint &position);
    void showRecentGames();
    void loadRecentSession(const QString &filePath);

    OpenGLVideoWidget *_videoWidget = nullptr;
    RecentGamesWidget *_recentGamesWidget = nullptr;
    QStackedWidget *_contentStack = nullptr;
    std::unique_ptr<EmulationSession> _session;
    std::unique_ptr<RecentFiles> _recentFiles;
    QMenu *_saveStateMenu = nullptr;
    QMenu *_loadStateMenu = nullptr;
    QMenu *_recentMenu = nullptr;
    QMenu *_gameMenu = nullptr;
    QMenu *_diskMenu = nullptr;
    QMenu *_tapeMenu = nullptr;
    QMenu *_speedMenu = nullptr;
    QMenu *_filterMenu = nullptr;
    QMenu *_aspectRatioMenu = nullptr;
    QMenu *_turboSpeedMenu = nullptr;
    QAction *_loadLastSessionAction = nullptr;
    QAction *_pauseAction = nullptr;
    QAction *_gameSettingsAction = nullptr;
    QAction *_ejectDiskAction = nullptr;
    QAction *_barcodeAction = nullptr;
    QAction *_playTapeAction = nullptr;
    QAction *_recordTapeAction = nullptr;
    QAction *_stopTapeAction = nullptr;
    QAction *_showFpsAction = nullptr;
    QAction *_bilinearAction = nullptr;
    QAction *_verticalSyncAction = nullptr;
    QAction *_cheatsAction = nullptr;
    QList<QAction *> _speedActions;
    QList<QAction *> _turboSpeedActions;
    QList<QAction *> _filterActions;
    QList<QAction *> _aspectRatioActions;
    QList<QAction *> _requiresGameActions;
    QList<QAction *> _diskActions;
    QList<QAction *> _vsActions;
};
