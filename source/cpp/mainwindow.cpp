#include "mainwindow.h"

#include "catalog.h"
#include "player.h"

#include <QApplication>
#include <QCloseEvent>
#include <QFrame>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QPushButton>
#include <QRandomGenerator>
#include <QScreen>
#include <QSettings>
#include <QShortcut>
#include <QSize>
#include <QSlider>
#include <QTimer>
#include <QVBoxLayout>

#ifdef Q_OS_WIN
#include <QSystemTrayIcon>
#include <dwmapi.h>
#include <windows.h>
#endif

namespace {

QString joinName(const Track &track)
{
    if (track.artist.isEmpty())
        return track.displayTitle();
    return track.displayArtist() + QStringLiteral(" - ") + track.displayTitle();
}

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("SoundHub"));

    const QRect available = QGuiApplication::primaryScreen()->availableGeometry();
    resize(qMin(460, available.width() - 40), qMin(720, available.height() - 60));
    setMinimumSize(340, 500);

    m_settings = new QSettings(QStringLiteral("SoundHub"), QStringLiteral("SoundHub"), this);
    m_catalog = new Catalog(this);
    m_player = new Player(this);

    buildUi();
    applyStyle();

#ifdef Q_OS_WIN
    applyDarkTitleBar();
#endif

    m_statusTimer = new QTimer(this);
    m_statusTimer->setSingleShot(true);
    m_statusTimer->setInterval(5000);
    connect(m_statusTimer, &QTimer::timeout, m_status, &QLabel::hide);

    connect(m_catalog, &Catalog::loaded, this, [this](int count) {
        populate();
        restoreState();
        if (count == 0)
            setStatus(tr("В репозитории пока нет треков."));
    });
    connect(m_catalog, &Catalog::failed, this, [this](const QString &message) {
        setStatus(message);
    });

    connect(m_player, &Player::playingChanged, this, [this](bool playing) {
        updatePlayButton();
        if (!playing && m_current >= 0) {
            const QVector<Track> &tracks = m_catalog->tracks();
            if (m_current < tracks.size())
                m_nowTitle->setText(tr("Пауза: %1").arg(joinName(tracks[m_current])));
        }
    });
    connect(m_player, &Player::positionChanged, this, [this](qint64 ms) {
        if (m_seeking)
            return;
        m_seek->blockSignals(true);
        m_seek->setValue(static_cast<int>(ms));
        m_seek->blockSignals(false);
        m_timeCurrent->setText(formatTime(ms));
    });
    connect(m_player, &Player::durationChanged, this, [this](qint64 ms) {
        m_seek->setRange(0, static_cast<int>(ms));
        m_timeTotal->setText(formatTime(ms));
    });
    connect(m_player, &Player::finished, this, [this] { stepTrack(1, false); });
    connect(m_player, &Player::errorOccurred, this, [this](const QString &message) {
        setStatus(tr("Ошибка воспроизведения: %1").arg(message));
    });

    m_volume->setValue(m_settings->value(QStringLiteral("volume"), 80).toInt());
    m_player->setVolume(m_volume->value());

    m_catalog->reload();

#ifdef Q_OS_WIN
    m_tray = new QSystemTrayIcon(QIcon(QStringLiteral(":/icon.png")), this);
    auto *menu = new QMenu(this);
    auto *showAction = menu->addAction(tr("Показать"));
    connect(showAction, &QAction::triggered, this, &MainWindow::showFromTray);
    auto *quitAction = menu->addAction(tr("Выход"));
    connect(quitAction, &QAction::triggered, qApp, &QApplication::quit);
    m_tray->setContextMenu(menu);
    m_tray->setToolTip(QStringLiteral("SoundHub"));
    connect(m_tray, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::Trigger)
            showFromTray();
    });
    m_tray->show();
#endif
}

void MainWindow::buildUi()
{
    auto *central = new QWidget;
    central->setObjectName(QStringLiteral("root"));

    auto *mainLayout = new QVBoxLayout(central);
    mainLayout->setContentsMargins(15, 15, 15, 15);
    mainLayout->setSpacing(10);

    m_search = new QLineEdit;
    m_search->setObjectName(QStringLiteral("searchBar"));
    m_search->setPlaceholderText(tr("Поиск трека..."));
    m_search->setClearButtonEnabled(true);
    m_search->setFixedHeight(38);
    mainLayout->addWidget(m_search);

    m_status = new QLabel;
    m_status->setObjectName(QStringLiteral("status"));
    m_status->setWordWrap(true);
    m_status->hide();
    mainLayout->addWidget(m_status);

    m_list = new QListWidget;
    m_list->setObjectName(QStringLiteral("trackList"));
    m_list->setFrameShape(QFrame::NoFrame);
    m_list->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);
    m_list->setUniformItemSizes(true);
    m_list->setSpacing(0);
    mainLayout->addWidget(m_list, 1);

    auto *playerFrame = new QFrame;
    playerFrame->setObjectName(QStringLiteral("playerFrame"));
    auto *playerLayout = new QVBoxLayout(playerFrame);
    playerLayout->setContentsMargins(15, 14, 15, 14);
    playerLayout->setSpacing(8);

    m_nowTitle = new QLabel(tr("Трек не выбран"));
    m_nowTitle->setObjectName(QStringLiteral("nowTitle"));
    m_nowTitle->setAlignment(Qt::AlignCenter);
    m_nowTitle->setWordWrap(true);
    playerLayout->addWidget(m_nowTitle);

    auto *seekRow = new QHBoxLayout;
    seekRow->setSpacing(8);
    m_timeCurrent = new QLabel(QStringLiteral("0:00"));
    m_timeCurrent->setObjectName(QStringLiteral("timeLabel"));
    m_seek = new QSlider(Qt::Horizontal);
    m_seek->setObjectName(QStringLiteral("seek"));
    m_seek->setRange(0, 0);
    m_timeTotal = new QLabel(QStringLiteral("0:00"));
    m_timeTotal->setObjectName(QStringLiteral("timeLabel"));
    seekRow->addWidget(m_timeCurrent);
    seekRow->addWidget(m_seek, 1);
    seekRow->addWidget(m_timeTotal);
    playerLayout->addLayout(seekRow);

    auto *controls = new QHBoxLayout;
    controls->setSpacing(10);
    controls->addStretch(1);

    auto makeButton = [](const QString &iconName, int size) {
        auto *button = new QPushButton;
        button->setObjectName(QStringLiteral("controlButton"));
        button->setCursor(Qt::PointingHandCursor);
        button->setFixedSize(size, size);
        button->setIcon(QIcon(QStringLiteral(":/ui/%1.png").arg(iconName)));
        button->setIconSize(QSize(size / 2, size / 2));
        button->setFocusPolicy(Qt::NoFocus);
        return button;
    };

    m_shuffleButton = makeButton(QStringLiteral("shuffle"), 42);
    m_shuffleButton->setCheckable(true);
    controls->addWidget(m_shuffleButton);

    auto *prevButton = makeButton(QStringLiteral("prev"), 42);
    controls->addWidget(prevButton);

    m_playButton = makeButton(QStringLiteral("play"), 52);
    controls->addWidget(m_playButton);

    auto *nextButton = makeButton(QStringLiteral("next"), 42);
    controls->addWidget(nextButton);

    m_repeatButton = makeButton(QStringLiteral("repeat"), 42);
    m_repeatButton->setCheckable(true);
    controls->addWidget(m_repeatButton);

    controls->addStretch(1);
    playerLayout->addLayout(controls);

    auto *volumeRow = new QHBoxLayout;
    volumeRow->setSpacing(8);
    auto *volumeIcon = new QLabel;
    volumeIcon->setPixmap(QIcon(QStringLiteral(":/ui/volume.png")).pixmap(16, 16));
    m_volume = new QSlider(Qt::Horizontal);
    m_volume->setObjectName(QStringLiteral("volume"));
    m_volume->setRange(0, 100);
    m_volume->setFixedWidth(160);
    volumeRow->addStretch(1);
    volumeRow->addWidget(volumeIcon);
    volumeRow->addWidget(m_volume);
    volumeRow->addStretch(1);
    playerLayout->addLayout(volumeRow);

    mainLayout->addWidget(playerFrame);
    setCentralWidget(central);

    connect(m_search, &QLineEdit::textChanged, this, [this] { populate(); });
    connect(m_list, &QListWidget::itemClicked, this, [this](QListWidgetItem *item) {
        playRow(m_list->row(item));
    });

    connect(prevButton, &QPushButton::clicked, this, [this] { stepTrack(-1, true); });
    connect(nextButton, &QPushButton::clicked, this, [this] { stepTrack(1, true); });
    connect(m_playButton, &QPushButton::clicked, this, &MainWindow::togglePlay);
    connect(m_shuffleButton, &QPushButton::toggled, this, [this](bool checked) {
        m_shuffle = checked;
        if (checked && m_repeatButton->isChecked())
            m_repeatButton->setChecked(false);
    });
    connect(m_repeatButton, &QPushButton::toggled, this, [this](bool checked) {
        m_repeat = checked;
        if (checked && m_shuffleButton->isChecked())
            m_shuffleButton->setChecked(false);
    });
    connect(m_seek, &QSlider::sliderPressed, this, [this] { m_seeking = true; });
    connect(m_seek, &QSlider::sliderReleased, this, [this] {
        m_seeking = false;
        m_player->seek(m_seek->value());
    });
    connect(m_seek, &QSlider::sliderMoved, this, [this](int value) {
        m_timeCurrent->setText(formatTime(value));
    });
    connect(m_volume, &QSlider::valueChanged, this, [this](int value) {
        m_player->setVolume(value);
        m_settings->setValue(QStringLiteral("volume"), value);
    });

    auto *spaceShortcut = new QShortcut(QKeySequence(Qt::Key_Space), this);
    connect(spaceShortcut, &QShortcut::activated, this, [this] {
        if (!m_search->hasFocus())
            togglePlay();
    });
    auto *nextShortcut = new QShortcut(QKeySequence(QStringLiteral("Ctrl+Right")), this);
    connect(nextShortcut, &QShortcut::activated, this, [this] { stepTrack(1, true); });
    auto *prevShortcut = new QShortcut(QKeySequence(QStringLiteral("Ctrl+Left")), this);
    connect(prevShortcut, &QShortcut::activated, this, [this] { stepTrack(-1, true); });
}

void MainWindow::applyStyle()
{
    setStyleSheet(QStringLiteral(R"(
        QMainWindow, QWidget#root {
            background: #121212;
            color: #FFFFFF;
            font-family: 'Segoe UI', 'Roboto', Arial, sans-serif;
        }
        QLineEdit#searchBar {
            background: #1F1F1F;
            border: 1px solid #2A2A2A;
            border-radius: 17px;
            padding: 8px 16px;
            color: #FFFFFF;
            font-size: 13px;
            selection-background-color: #1ED760;
            selection-color: #000000;
        }
        QLineEdit#searchBar:focus {
            border-color: #1ED760;
        }
        QLabel#status {
            color: #FF6B6B;
            font-size: 11px;
            padding: 2px 6px;
        }
        QListWidget#trackList {
            background: #181818;
            border: none;
            border-radius: 10px;
            outline: none;
            padding: 6px;
        }
        QListWidget#trackList::item {
            padding: 11px 12px;
            border-radius: 6px;
            color: #FFFFFF;
            font-size: 12px;
        }
        QListWidget#trackList::item:hover {
            background: #242424;
        }
        QListWidget#trackList::item:selected {
            background: #1ED760;
            color: #000000;
            font-weight: 700;
        }
        QScrollBar:vertical {
            background: transparent;
            width: 10px;
            margin: 0;
        }
        QScrollBar::handle:vertical {
            background: #3A3A4A;
            border-radius: 5px;
            min-height: 20px;
        }
        QScrollBar::handle:vertical:hover {
            background: #1ED760;
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical,
        QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {
            background: none;
            height: 0;
        }
        QFrame#playerFrame {
            background: #181818;
            border: none;
            border-radius: 12px;
        }
        QLabel#nowTitle {
            color: #1ED760;
            font-size: 15px;
            font-weight: 800;
        }
        QLabel#timeLabel {
            color: #B3B3B3;
            font-size: 10px;
            font-weight: 700;
        }
        QPushButton#controlButton {
            background: #1ED760;
            border: none;
            border-radius: 21px;
        }
        QPushButton#controlButton:hover {
            background: #3BE477;
        }
        QPushButton#controlButton:checked {
            background: #15A64A;
        }
        QSlider::groove:horizontal {
            height: 6px;
            background: #2A2A2A;
            border-radius: 3px;
        }
        QSlider::sub-page:horizontal {
            background: #1ED760;
            border-radius: 3px;
        }
        QSlider::add-page:horizontal {
            background: #2A2A2A;
            border-radius: 3px;
        }
        QSlider::handle:horizontal {
            background: #FFFFFF;
            width: 12px;
            height: 12px;
            margin: -3px 0;
            border-radius: 6px;
        }
        QSlider::handle:horizontal:hover {
            background: #1ED760;
        }
    )"));
}

void MainWindow::populate()
{
    const QString query = m_search->text().trimmed();
    const QString selectedId = (m_current >= 0 && m_current < m_catalog->tracks().size())
        ? m_catalog->tracks()[m_current].id
        : QString();

    m_list->clear();
    m_visible.clear();

    const QVector<Track> &tracks = m_catalog->tracks();
    int selectedRow = -1;

    for (int i = 0; i < tracks.size(); ++i) {
        const Track &track = tracks[i];
        const QString name = joinName(track);
        if (!query.isEmpty() && !name.contains(query, Qt::CaseInsensitive))
            continue;

        auto *item = new QListWidgetItem(name);
        item->setData(Qt::UserRole, track.id);
        item->setSizeHint(QSize(0, 42));
        item->setToolTip(name);
        m_list->addItem(item);
        m_visible.append(i);

        if (!selectedId.isEmpty() && track.id == selectedId)
            selectedRow = m_visible.size() - 1;
    }

    if (selectedRow >= 0) {
        m_list->setCurrentRow(selectedRow);
    } else if (m_current >= 0) {
        m_list->clearSelection();
    }
}

void MainWindow::playRow(int row)
{
    if (row < 0 || row >= m_visible.size())
        return;
    playCatalogIndex(m_visible[row], true);
}

void MainWindow::playCatalogIndex(int index, bool)
{
    const QVector<Track> &tracks = m_catalog->tracks();
    if (index < 0 || index >= tracks.size())
        return;

    m_current = index;
    const Track &track = tracks[index];

    m_nowTitle->setText(joinName(track));
    m_seek->setRange(0, 0);
    m_timeCurrent->setText(QStringLiteral("0:00"));
    m_timeTotal->setText(QStringLiteral("0:00"));

    m_player->play(track.url);
    m_settings->setValue(QStringLiteral("last_track"), track.id);

    const int row = m_visible.indexOf(index);
    if (row >= 0)
        m_list->setCurrentRow(row);

#ifdef Q_OS_WIN
    if (m_tray)
        m_tray->setToolTip(QStringLiteral("SoundHub: %1").arg(joinName(track)));
#endif
}

void MainWindow::stepTrack(int delta, bool manual)
{
    const QVector<Track> &tracks = m_catalog->tracks();
    const int count = tracks.size();
    if (count == 0)
        return;

    if (m_repeat && !manual && m_current >= 0) {
        playCatalogIndex(m_current, manual);
        return;
    }

    int next = m_current;
    if (m_shuffle && count > 1) {
        do {
            next = QRandomGenerator::global()->bounded(count);
        } while (next == m_current);
    } else if (m_current < 0) {
        next = delta >= 0 ? 0 : count - 1;
    } else {
        next = (m_current + delta + count) % count;
    }

    playCatalogIndex(next, manual);
}

void MainWindow::togglePlay()
{
    if (m_current < 0) {
        if (!m_visible.isEmpty())
            playRow(m_list->currentRow() >= 0 ? m_list->currentRow() : 0);
        else if (!m_catalog->tracks().isEmpty())
            playCatalogIndex(0, true);
        return;
    }
    m_player->togglePause();
}

void MainWindow::updatePlayButton()
{
    const bool playing = m_player->isPlaying();
    m_playButton->setIcon(QIcon(playing ? QStringLiteral(":/ui/pause.png")
                                        : QStringLiteral(":/ui/play.png")));
}

void MainWindow::restoreState()
{
    const QString lastId = m_settings->value(QStringLiteral("last_track")).toString();
    if (lastId.isEmpty())
        return;

    const QVector<Track> &tracks = m_catalog->tracks();
    for (int i = 0; i < tracks.size(); ++i) {
        if (tracks[i].id != lastId)
            continue;
        m_current = i;
        m_nowTitle->setText(tr("Пауза: %1").arg(joinName(tracks[i])));
        const int row = m_visible.indexOf(i);
        if (row >= 0)
            m_list->setCurrentRow(row);
        break;
    }
}

void MainWindow::setStatus(const QString &text)
{
    m_status->setText(text);
    m_status->setVisible(!text.isEmpty());
    if (!text.isEmpty())
        m_statusTimer->start();
}

QString MainWindow::formatTime(qint64 ms)
{
    if (ms <= 0)
        return QStringLiteral("0:00");
    const qint64 total = ms / 1000;
    return QStringLiteral("%1:%2")
        .arg(total / 60)
        .arg(total % 60, 2, 10, QLatin1Char('0'));
}

#ifdef Q_OS_WIN
void MainWindow::applyDarkTitleBar()
{
    const HWND handle = reinterpret_cast<HWND>(winId());
    if (!handle)
        return;

    BOOL dark = TRUE;
    if (FAILED(DwmSetWindowAttribute(handle, 20, &dark, sizeof(dark))))
        DwmSetWindowAttribute(handle, 19, &dark, sizeof(dark));

    const COLORREF caption = RGB(18, 18, 18);
    DwmSetWindowAttribute(handle, 35, &caption, sizeof(caption));

    const COLORREF text = RGB(255, 255, 255);
    DwmSetWindowAttribute(handle, 36, &text, sizeof(text));

    const COLORREF border = RGB(42, 42, 42);
    DwmSetWindowAttribute(handle, 34, &border, sizeof(border));
}
#endif

void MainWindow::showFromTray()
{
    showNormal();
    raise();
    activateWindow();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
#ifdef Q_OS_WIN
    if (m_tray && m_tray->isVisible()) {
        hide();
        event->ignore();
        return;
    }
#endif
    saveState();
    QMainWindow::closeEvent(event);
}

void MainWindow::saveState()
{
    m_settings->setValue(QStringLiteral("volume"), m_volume->value());
    if (m_current >= 0 && m_current < m_catalog->tracks().size())
        m_settings->setValue(QStringLiteral("last_track"), m_catalog->tracks()[m_current].id);
}

