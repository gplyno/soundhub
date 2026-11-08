#pragma once

#include <QMainWindow>

class Catalog;
class Player;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QSlider;
class QTimer;
class QSettings;
#ifdef Q_OS_WIN
class QSystemTrayIcon;
#endif

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

    void showFromTray();

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    void buildUi();
    void applyStyle();
    void populate();
    void playRow(int row);
    void playCatalogIndex(int index, bool manual);
    void stepTrack(int delta, bool manual);
    void togglePlay();
    void refreshNowPlaying();
    void updatePlayButton();
    void setStatus(const QString &text);
    void restoreState();
    void saveState();
#ifdef Q_OS_WIN
    void applyDarkTitleBar();
#endif
    static QString formatTime(qint64 ms);

    Catalog *m_catalog = nullptr;
    Player *m_player = nullptr;
    QSettings *m_settings = nullptr;

    QLineEdit *m_search = nullptr;
    QListWidget *m_list = nullptr;
    QLabel *m_status = nullptr;
    QTimer *m_statusTimer = nullptr;

    QLabel *m_nowTitle = nullptr;
    QLabel *m_timeCurrent = nullptr;
    QLabel *m_timeTotal = nullptr;
    QSlider *m_seek = nullptr;
    QSlider *m_volume = nullptr;
    QPushButton *m_playButton = nullptr;
    QPushButton *m_shuffleButton = nullptr;
    QPushButton *m_repeatButton = nullptr;

    QVector<int> m_visible;   // list row -> catalog index
    int m_current = -1;       // catalog index of the playing track
    bool m_seeking = false;
    bool m_shuffle = false;
    bool m_repeat = false;

#ifdef Q_OS_WIN
    QSystemTrayIcon *m_tray = nullptr;
#endif
};
