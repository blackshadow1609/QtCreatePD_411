#include "mainwindow.h"                                             //Ctrl+I - Авто форматирование выделеного кода
#include "ui_mainwindow.h"
#include <QStyle>
#include <QFileDialog>
#include <QTime>
#include <QStyleFactory>
#include <QStandardPaths>
#include <QProgressDialog>
#include <QDebug>
#include <QDir>
#include <QApplication>
#include <QEventLoop>
#include <QTextStream>
#include <QFileInfo>
#include <QToolBar>         // Добавлено для эквалайзера
#include <QDialog>          // Добавлено для эквалайзера
#include <QVBoxLayout>      // Добавлено для эквалайзера
#include <QHBoxLayout>      // Добавлено для эквалайзера
#include <QPushButton>      // Добавлено для эквалайзера
#include <cmath>            // Добавлено для математических функций

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_duration_player(this)
    , visualizationDialog(nullptr)
    , visualizationPlot(nullptr)
    , visualizationTimer(nullptr)
    , isPlaying(false)
{
    ui->setupUi(this);

    ui->horizontalSliderVolume->setRange(0, 100);

    // Player init:
    m_player = new QMediaPlayer();
    m_player->setVolume(10);                                                                //уровень громкости при запуске
    ui->labelVolume->setText(QString("Volume: ").append(QString::number(m_player->volume())));
    ui->horizontalSliderVolume->setValue(m_player->volume());

    connect(m_player, &QMediaPlayer::durationChanged, this, &MainWindow :: on_duration_changed);
    connect(m_player, &QMediaPlayer::positionChanged, this, &MainWindow :: on_position_changed);
    connect(this->ui->horizontalSliderTime, &QSlider::sliderMoved, this, &MainWindow::on_horizontalSliderTime_sliderMoved);

    //Playlist init
    m_playlist_model = new QStandardItemModel(this);
    initPlaylist();
    m_playlist = new QMediaPlaylist(m_player);
    m_player->setPlaylist(m_playlist);

    connect(this->ui->pushButtonPrev, &QPushButton::clicked, this->m_playlist, &QMediaPlaylist::previous);
    connect(this->ui->pushButtonNext, &QPushButton::clicked, this->m_playlist, &QMediaPlaylist::next);
    connect(this->m_playlist, &QMediaPlaylist::currentIndexChanged, this->ui->tableViewPlaylist, &QTableView::selectRow);
    connect(this->ui->tableViewPlaylist, &QTableView::doubleClicked,
            [this](const QModelIndex& index){m_playlist->setCurrentIndex(index.row()); this->m_player->play();}
    );
    connect(this->m_player, &QMediaPlayer::currentMediaChanged,
            [this](const QMediaContent& media)
    {
        this->ui->labelFilename->setText(media.canonicalUrl().toString());
        this->setWindowTitle(this->ui->labelFilename->text().split('/').last());
    }
    );
    shuffle = false;
    loop = false;
    /////////////////////////////////////////////////////////////////////////////////////
    connect(qApp, &QApplication::aboutToQuit, this, &MainWindow::savePlaylistOnExit);

    loadPlaylistOnStart();
    /////////////////////////////////////////////////////////////////////////////////////

    QToolBar *toolBar = addToolBar("Инструменты");

    QPushButton *eqButton = new QPushButton("Эквалайзер");
    toolBar->addWidget(eqButton);
    connect(eqButton, &QPushButton::clicked, this, &MainWindow::setupEqualizer);

    QPushButton *visButton = new QPushButton("Визуализация");
    toolBar->addWidget(visButton);
    connect(visButton, &QPushButton::clicked, this, &MainWindow::showVisualization);

    visualizationTimer = new QTimer(this);
    connect(visualizationTimer, &QTimer::timeout, this, &MainWindow::updateAudioVisualization);

    setupAudioVisualization();

    connect(m_player, &QMediaPlayer::stateChanged, this, &MainWindow::onPlayerStateChanged);
}

MainWindow::~MainWindow()
{
    if(visualizationTimer) {
        visualizationTimer->stop();
        delete visualizationTimer;
    }

    if(visualizationDialog) {
        visualizationDialog->deleteLater();
    }

    delete m_playlist_model;
    delete m_playlist;
    delete m_player;
    delete ui;
}

void MainWindow::initPlaylist()
{
    this->ui->tableViewPlaylist->setModel(m_playlist_model);
    m_playlist_model->setHorizontalHeaderLabels(QStringList()  << "Audio track" << "File path" << "Duration");
    this->ui->tableViewPlaylist->setEditTriggers(QAbstractItemView::NoEditTriggers);
    this->ui->tableViewPlaylist->setSelectionBehavior(QAbstractItemView::SelectRows);

    this->ui->tableViewPlaylist->hideColumn(1);
    int duration_width = 64;
    this->ui->tableViewPlaylist->setColumnWidth(2, duration_width);
    this->ui->tableViewPlaylist->setColumnWidth(0, this->ui->tableViewPlaylist->width()-duration_width*1.7);
}

void MainWindow::loadFileToPlaylist(const QString &filename)
{
    //////////////////////////////////////////////////////////////////////////////////
    int currentRow = m_playlist_model->rowCount();

    m_playlist->addMedia(QUrl(filename));
    QList<QStandardItem*> items;
    items.append(new QStandardItem(QDir(filename).dirName()));
    items.append(new QStandardItem(filename));
    items.append(new QStandardItem("hh:mm:ss"));
    m_playlist_model->appendRow(items);

    QMediaPlayer* durationPlayer = new QMediaPlayer(this);
    durationPlayer->setMedia(QUrl::fromLocalFile(filename));

    connect(durationPlayer, &QMediaPlayer::durationChanged,
            [this, currentRow, durationPlayer](qint64 duration) {
        this->on_duration_loaded(duration, currentRow);
        durationPlayer->deleteLater();
    });

    durationPlayer->play();
    durationPlayer->pause();
}
////////////////////////////////////////////////////////////////////////////////////
void MainWindow::on_pushButtonAdd_clicked()
{
    QStringList files = QFileDialog::getOpenFileNames
            (
                this,
                "Open file",
                "C:\\Users\\wwwbl\\Music",
                "Audio files (*.mp3 *.flac *.flacc);; mp3 (*.mp3);; Flac (*.flac *.flacc)"
                );

    for(QString file:files)
    {
        loadFileToPlaylist(file);
    }
}

void MainWindow::on_pushButtonPlay_clicked()
{
    this->m_player->play();
}

void MainWindow::on_pushButtonPause_clicked()
{
    m_player->state() == QMediaPlayer :: State :: PausedState ? m_player->play() : this->m_player->pause();
}

void MainWindow::on_pushButtonStop_clicked()
{
    this->m_player->stop();
}

void MainWindow::on_pushButtonMute_clicked()
{
    m_player->setMuted(!m_player->isMuted());
    ui->pushButtonMute->setIcon(style()->standardIcon(m_player->isMuted() ? QStyle :: SP_MediaVolumeMuted : QStyle::SP_MediaVolume));
}

void MainWindow::on_horizontalSliderVolume_valueChanged(int value)
{
    m_player->setVolume(value);
    ui->labelVolume->setText(QString("Volume: ").append(QString::number(value)));
}

void MainWindow::on_duration_changed(qint64 duration)
{
    this->ui->horizontalSliderTime->setRange(0, duration);
    this->ui->labelDuration->setText(QTime :: fromMSecsSinceStartOfDay(duration).toString("hh:mm:ss"));
}
//////////////////////////////////////////////////////////
void MainWindow::on_duration_loaded(qint64 duration, int row)
{
    if (duration > 0) {
        QStandardItem* durationItem = new QStandardItem(QTime::fromMSecsSinceStartOfDay(duration).toString("hh:mm:ss"));
        m_playlist_model->setItem(row, 2, durationItem);
    } else {
        QStandardItem* durationItem = new QStandardItem("hh:mm:ss");
        m_playlist_model->setItem(row, 2, durationItem);
    }
}
/////////////////////////////////////////////////////////
void MainWindow::on_position_changed(qint64 position)
{
    this->ui->labelposition->setText(QString(QTime :: fromMSecsSinceStartOfDay(position).toString("hh:mm:ss")));
    this->ui->horizontalSliderTime->setValue(position);
}

void MainWindow::on_horizontalSliderTime_sliderMoved(int position)
{
    this->m_player->setPosition(position);
}

void MainWindow::on_pushButtonShuffle_clicked()
{
    shuffle = !shuffle;
    this->ui->pushButtonShuffle->setCheckable(true);
    this->m_playlist->setPlaybackMode(shuffle ? QMediaPlaylist::PlaybackMode::Random : QMediaPlaylist::PlaybackMode::Sequential);
    this->ui->pushButtonShuffle->setChecked(shuffle);
}

void MainWindow::on_pushButtonLoop_clicked()
{
    loop = !loop;
    this->ui->pushButtonLoop->setCheckable(true);
    this->m_playlist->setPlaybackMode(loop ? QMediaPlaylist::PlaybackMode::Loop : QMediaPlaylist::PlaybackMode::Sequential);
    this->ui->pushButtonLoop->setChecked(loop);
}

void MainWindow::on_pushButtonDel_clicked()
{
    QItemSelectionModel* selection = nullptr;
    do
    {
        selection = ui->tableViewPlaylist->selectionModel();
        QModelIndexList indexes = selection->selectedRows();
        if(selection->selectedRows().count()>0)
        {
            m_playlist_model->removeRow(indexes.first().row());
            m_playlist->removeMedia(indexes.first().row());
        }
    }while(selection->selectedRows().count());
}

void MainWindow::on_pushButtonClr_clicked()
{
    m_playlist->clear();
    m_playlist_model->clear();
    initPlaylist();
}

void MainWindow::on_pushButtonDir_clicked()
{
    /////////////////////////////////////////////////////////////////////////////////////////

    QString dirname = QFileDialog::getExistingDirectory
            (
                this,
                "Add directory",
                QStandardPaths::writableLocation(QStandardPaths::MusicLocation),
                QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks
                );

    if (dirname.isEmpty()) {
        return;
    }
    QDir dir(dirname);
    QStringList filters;
    filters << "*.mp3" << "*.flac";
    dir.setNameFilters(filters);
    dir.setFilter(QDir::Files | QDir::NoDotAndDotDot | QDir::Readable);

    QFileInfoList files = dir.entryInfoList();

    //прогресс загрузки
    QProgressDialog progress("Loading files...", "Cancel", 0, files.count(), this);
    progress.setWindowModality(Qt::WindowModal);
    for (int i = 0; i < files.count(); i++) {
        if (progress.wasCanceled()) {
            break;
        }
        const QFileInfo& file = files.at(i);
        QString filePath = file.absoluteFilePath();
        loadFileToPlaylist(filePath);

        progress.setValue(i + 1);
        QApplication::processEvents(); //что б лучше отзывалась UI
    }

}
//////////////////////////////////////////////////////////////////////////////////
void MainWindow::savePlaylistOnExit()
{
    QString playlistPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir dir(playlistPath);
    if (!dir.exists()) {
        dir.mkpath(playlistPath);
    }

    QString savePath = playlistPath + "/playlist.m3u";

    m_playlist->save(QUrl::fromLocalFile(savePath), "m3u");
    qDebug() << "Playlist saved to:" << savePath;
}

void MainWindow::loadPlaylistOnStart()
{
    QString playlistPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/playlist.m3u";

    if (QFile::exists(playlistPath)) {
        qDebug() << "Loading playlist from:" << playlistPath;

        QFile file(playlistPath);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream in(&file);
            while (!in.atEnd()) {
                QString line = in.readLine().trimmed();

                if (line.startsWith("#") || line.isEmpty()) {
                    continue;
                }

                QString filePath = line;
                QFileInfo fileInfo(filePath);
                if (!fileInfo.isAbsolute()) {
                    QFileInfo playlistInfo(playlistPath);
                    QString dirPath = playlistInfo.absolutePath();
                    filePath = QDir(dirPath).absoluteFilePath(line);
                }

                if (QFile::exists(filePath)) {
                    loadFileToPlaylist(filePath);
                } else {
                    qDebug() << "File not found, skipping:" << filePath;
                }
            }
            file.close();
            qDebug() << "Playlist loaded, tracks:" << m_playlist->mediaCount();
        } else {
            qDebug() << "Failed to open playlist file:" << playlistPath;
        }
    } else {
        qDebug() << "No saved playlist found at:" << playlistPath;
    }
}

void MainWindow::onPlayerStateChanged(QMediaPlayer::State state)
{
    isPlaying = (state == QMediaPlayer::PlayingState);

    if (visualizationTimer) {
        if (isPlaying && visualizationDialog && visualizationDialog->isVisible()) {
            visualizationTimer->start(50);  // 20 FPS
        } else {
            visualizationTimer->stop();
        }
    }
}

void MainWindow::setupEqualizer()
{
    QDialog *equalizerDialog = new QDialog(this);
    equalizerDialog->setWindowTitle("Графический эквалайзер");
    equalizerDialog->setMinimumSize(800, 500);

    QVBoxLayout *layout = new QVBoxLayout(equalizerDialog);

    QCustomPlot *customPlot = new QCustomPlot();
    layout->addWidget(customPlot);

    int bands = 10;

    QVector<double> *frequencies = new QVector<double>(bands);
    QVector<double> *gains = new QVector<double>(bands);
    QVector<QCPBars*> *eqBars = new QVector<QCPBars*>();

    double freq = 32;
    for(int i = 0; i < bands; ++i) {
        (*frequencies)[i] = freq;
        (*gains)[i] = 0;
        freq *= 2;
    }

    customPlot->xAxis->setLabel("Частота (Гц)");
    customPlot->yAxis->setLabel("Усиление (дБ)");
    customPlot->xAxis->setRange(0, bands);
    customPlot->yAxis->setRange(-12, 12);

    for(int i = 0; i < bands; ++i) {
        QCPBars *bar = new QCPBars(customPlot->xAxis, customPlot->yAxis);
        bar->setData(QVector<double>{i + 0.5}, QVector<double>{(*gains)[i]});
        bar->setWidth(0.8);
        bar->setBrush(QColor(70, 130, 180));
        eqBars->append(bar);
    }

    QVector<double> ticks;
    QVector<QString> labels;
    for(int i = 0; i < bands; ++i) {
        ticks << i + 0.5;
        if((*frequencies)[i] < 1000) {
            labels << QString("%1").arg((*frequencies)[i], 0, 'f', 0);
        } else {
            labels << QString("%1k").arg((*frequencies)[i]/1000, 0, 'f', 1);
        }
    }
    customPlot->xAxis->setTickVector(ticks);
    customPlot->xAxis->setTickVectorLabels(labels);

    customPlot->xAxis->grid()->setVisible(true);
    customPlot->yAxis->grid()->setVisible(true);

    connect(customPlot, &QCustomPlot::mousePress, [=](QMouseEvent *event) {
        double x = customPlot->xAxis->pixelToCoord(event->pos().x());
        double y = customPlot->yAxis->pixelToCoord(event->pos().y());

        int band = qRound(x - 0.5);
        if(band >= 0 && band < bands) {
            (*gains)[band] = qBound(-12.0, y, 12.0);
            (*eqBars)[band]->data()->clear();
            (*eqBars)[band]->addData(band + 0.5, (*gains)[band]);
            customPlot->replot();

            qDebug() << "Band" << band << "(" << (*frequencies)[band] << "Hz):" << (*gains)[band] << "dB";
        }
    });

    connect(customPlot, &QCustomPlot::mouseMove, [=](QMouseEvent *event) {
        if(event->buttons() & Qt::LeftButton) {
            double x = customPlot->xAxis->pixelToCoord(event->pos().x());
            double y = customPlot->yAxis->pixelToCoord(event->pos().y());

            int band = qRound(x - 0.5);
            if(band >= 0 && band < bands) {
                (*gains)[band] = qBound(-12.0, y, 12.0);
                (*eqBars)[band]->data()->clear();
                (*eqBars)[band]->addData(band + 0.5, (*gains)[band]);
                customPlot->replot();
            }
        }
    });

    QHBoxLayout *buttonLayout = new QHBoxLayout();

    QPushButton *resetButton = new QPushButton("Сброс");
    connect(resetButton, &QPushButton::clicked, [=]() {
        for(int i = 0; i < bands; ++i) {
            (*gains)[i] = 0;
            (*eqBars)[i]->data()->clear();
            (*eqBars)[i]->addData(i + 0.5, (*gains)[i]);
        }
        customPlot->replot();
    });

    QPushButton *closeButton = new QPushButton("Закрыть");
    connect(closeButton, &QPushButton::clicked, equalizerDialog, &QDialog::close);

    buttonLayout->addWidget(resetButton);
    buttonLayout->addStretch();
    buttonLayout->addWidget(closeButton);
    layout->addLayout(buttonLayout);

    connect(equalizerDialog, &QDialog::finished, [=]() {
        delete frequencies;
        delete gains;
        delete eqBars;
        equalizerDialog->deleteLater();
    });

    customPlot->replot();
    equalizerDialog->exec();
}

void MainWindow::setupAudioVisualization()
{
    spectrumBands = 32;
    spectrumData.resize(spectrumBands);
    xValues.resize(spectrumBands);

    for(int i = 0; i < spectrumBands; ++i) {
        spectrumData[i] = 0;
        xValues[i] = i + 0.5;
    }
}

void MainWindow::showVisualization()
{
    if(!visualizationDialog) {
        visualizationDialog = new QDialog(this);
        visualizationDialog->setWindowTitle("Визуализация аудио");
        visualizationDialog->setMinimumSize(900, 400);
        visualizationDialog->setStyleSheet("background-color: #1a1a2e;");

        QVBoxLayout *layout = new QVBoxLayout(visualizationDialog);

        QLabel *titleLabel = new QLabel("Аудиовизуализатор");
        titleLabel->setStyleSheet("color: white; font-size: 16px; font-weight: bold; padding: 10px;");
        titleLabel->setAlignment(Qt::AlignCenter);
        layout->addWidget(titleLabel);

        visualizationPlot = new QCustomPlot();
        visualizationPlot->setMinimumHeight(300);
        layout->addWidget(visualizationPlot);

        QVector<QColor> barColors;
        for(int i = 0; i < spectrumBands; ++i) {
            if(i < spectrumBands/3) {
                int red = 255;
                int green = 100 - (i * 100) / (spectrumBands/3);
                int blue = 50;
                barColors.append(QColor(red, green, blue));
            }
            else if(i < 2*spectrumBands/3) {
                int red = 100 - ((i - spectrumBands/3) * 100) / (spectrumBands/3);
                int green = 255;
                int blue = 100;
                barColors.append(QColor(red, green, blue));
            }
            else {
                int red = 50;
                int green = 100 - ((i - 2*spectrumBands/3) * 100) / (spectrumBands/3);
                int blue = 255;
                barColors.append(QColor(red, green, blue));
            }
        }

        for(int i = 0; i < spectrumBands; ++i) {
            QCPBars *bar = new QCPBars(visualizationPlot->xAxis, visualizationPlot->yAxis);
            bar->setData(QVector<double>{xValues[i]}, QVector<double>{spectrumData[i]});
            bar->setWidth(0.8);
            bar->setPen(QPen(barColors[i].darker(), 1));
            bar->setBrush(QBrush(barColors[i]));
            spectrumBars.append(bar);
        }

        visualizationPlot->xAxis->setLabel("Частотные диапазоны");
        visualizationPlot->yAxis->setLabel("Уровень");
        visualizationPlot->xAxis->setRange(0, spectrumBands);
        visualizationPlot->yAxis->setRange(0, 100);

        QVector<double> ticks;
        QVector<QString> labels;
        ticks << spectrumBands/6 << spectrumBands/2 << 5*spectrumBands/6;
        labels << "Низкие" << "Средние" << "Высокие";
        visualizationPlot->xAxis->setTickVector(ticks);
        visualizationPlot->xAxis->setTickVectorLabels(labels);

        visualizationPlot->xAxis->grid()->setPen(QPen(QColor(100, 100, 100), 1, Qt::DotLine));
        visualizationPlot->yAxis->grid()->setPen(QPen(QColor(100, 100, 100), 1, Qt::DotLine));

        visualizationPlot->setBackground(QBrush(QColor(30, 30, 46)));
        visualizationPlot->xAxis->setBasePen(QPen(Qt::white, 1));
        visualizationPlot->yAxis->setBasePen(QPen(Qt::white, 1));
        visualizationPlot->xAxis->setTickPen(QPen(Qt::white, 1));
        visualizationPlot->yAxis->setTickPen(QPen(Qt::white, 1));
        visualizationPlot->xAxis->setSubTickPen(QPen(Qt::white, 1));
        visualizationPlot->yAxis->setSubTickPen(QPen(Qt::white, 1));
        visualizationPlot->xAxis->setTickLabelColor(Qt::white);
        visualizationPlot->yAxis->setTickLabelColor(Qt::white);
        visualizationPlot->xAxis->setLabelColor(QColor(200, 200, 255));
        visualizationPlot->yAxis->setLabelColor(QColor(200, 200, 255));

        QHBoxLayout *statusLayout = new QHBoxLayout();
        QLabel *statusLabel = new QLabel("Статус:");
        statusLabel->setStyleSheet("color: white;");
        QLabel *playStatusLabel = new QLabel("Пауза");
        playStatusLabel->setStyleSheet("color: #ff6b6b; font-weight: bold;");
        playStatusLabel->setObjectName("statusLabel");
        statusLayout->addWidget(statusLabel);
        statusLayout->addWidget(playStatusLabel);
        statusLayout->addStretch();
        layout->addLayout(statusLayout);

        if(m_player) {
            QObject::connect(m_player, &QMediaPlayer::stateChanged,
                             [playStatusLabel](QMediaPlayer::State state) {
                if(state == QMediaPlayer::PlayingState) {
                    playStatusLabel->setText("Воспроизведение");
                    playStatusLabel->setStyleSheet("color: #4ecdc4; font-weight: bold;");
                } else if(state == QMediaPlayer::PausedState) {
                    playStatusLabel->setText("Пауза");
                    playStatusLabel->setStyleSheet("color: #ff6b6b; font-weight: bold;");
                } else {
                    playStatusLabel->setText("Остановлено");
                    playStatusLabel->setStyleSheet("color: #95a5a6; font-weight: bold;");
                }
            });
        }

        QPushButton *closeButton = new QPushButton("Закрыть");
        closeButton->setStyleSheet(
                    "QPushButton {"
                    "background-color: #4a4a6d;"
                    "color: white;"
                    "border: none;"
                    "padding: 8px 16px;"
                    "border-radius: 4px;"
                    "}"
                    "QPushButton:hover {"
                    "background-color: #5a5a8d;"
                    "}"
                    );
        connect(closeButton, &QPushButton::clicked, visualizationDialog, &QDialog::close);
        layout->addWidget(closeButton, 0, Qt::AlignRight);

        connect(visualizationDialog, &QDialog::finished, [this]() {
            visualizationTimer->stop();
        });
    }

    visualizationDialog->show();
    visualizationDialog->raise();
    visualizationDialog->activateWindow();

    if (isPlaying) {
        visualizationTimer->start(50);
    }

    visualizationPlot->replot();
}

void MainWindow::updateAudioVisualization()
{
    if(!visualizationPlot || !visualizationDialog || !visualizationDialog->isVisible()) {
        return;
    }

    if (!isPlaying) {
        for(int i = 0; i < spectrumBands; ++i) {
            spectrumData[i] *= 0.95;
            if(i < spectrumBars.size()) {
                spectrumBars[i]->data()->clear();
                spectrumBars[i]->addData(xValues[i], spectrumData[i]);
            }
        }
        visualizationPlot->replot();
        return;
    }

    static int frame = 0;
    frame++;

    qint64 position = m_player->position();
    qint64 duration = m_player->duration();
    double progress = duration > 0 ? (double)position / duration : 0;

    double volumeLevel = m_player->volume() / 100.0;

    for(int i = 0; i < spectrumBands; ++i) {
        double frequency = (double)i / spectrumBands;

        double basePattern = 40 * (0.5 + 0.5 * sin(2 * M_PI * frequency * 5 + 0.1 * frame));

        double bass = 0;
        if(i < 4) {
            bass = 35 * (0.5 + 0.5 * sin(0.05 * frame + progress * 2 * M_PI));
        }

        double mid = 0;
        if(i >= 4 && i < 16) {
            mid = 25 * (0.5 + 0.5 * sin(0.1 * i + 0.15 * frame));
        }

        double high = 0;
        if(i >= 16) {
            high = 15 * (0.5 + 0.5 * sin(0.3 * i + 0.25 * frame));
        }

        // Ритмичные удары (на каждой четверти)
        double beat = 0;
        double beatPhase = fmod(progress * 4, 1.0);
        if(i < 8 && beatPhase < 0.1) {
            beat = 40 * (1.0 - beatPhase * 10);
        }

        double randomSpike = (rand() % 100 < 2) ? 30 * volumeLevel : 0;

        double newValue = (basePattern + bass + mid + high + beat + randomSpike) * volumeLevel;
        spectrumData[i] = spectrumData[i] * 0.6 + newValue * 0.4;

        spectrumData[i] = qBound(0.0, spectrumData[i], 100.0);

        if(i < spectrumBars.size()) {
            spectrumBars[i]->data()->clear();
            spectrumBars[i]->addData(xValues[i], spectrumData[i]);
        }
    }

    double maxValue = 0;
    for(int i = 0; i < spectrumBands; ++i) {
        if(spectrumData[i] > maxValue) {
            maxValue = spectrumData[i];
        }
    }

    visualizationPlot->yAxis->setRange(0, qMax(50.0, maxValue * 1.3));
    visualizationPlot->replot();

    if(visualizationDialog && m_player->currentMedia().isNull() == false) {
        QString title = m_player->currentMedia().canonicalUrl().fileName();
        if(title.length() > 30) {
            title = title.left(27) + "...";
        }
        visualizationDialog->setWindowTitle("Визуализация: " + title);
    }
}
