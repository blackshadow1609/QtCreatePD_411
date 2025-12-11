#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QStyle>
#include <QFileDialog>
#include <QTime>
#include <QStyleFactory>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_duration_player(this)  // Инициализация m_duration_player
{
    ui->setupUi(this);

    //      Buttons style:
    ui->pushButtonAdd->setIcon(style()->standardIcon(QStyle::SP_DriveCDIcon));
    ui->pushButtonPrev->setIcon(style()->standardIcon(QStyle::SP_MediaSeekBackward));
    ui->pushButtonPlay->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
    ui->pushButtonPause->setIcon(style()->standardIcon(QStyle::SP_MediaPause));
    ui->pushButtonStop->setIcon(style()->standardIcon(QStyle::SP_MediaStop));
    ui->pushButtonNext->setIcon(style()->standardIcon(QStyle::SP_MediaSeekForward));
    ui->pushButtonShuffle->setIcon(style()->standardIcon(QStyle::SP_BrowserReload));            //единственная похожая по смыслу из доступных
    ui->pushButtonLoop->setIcon(style()->standardIcon(QStyle::SP_BrowserReload));

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

}



MainWindow::~MainWindow()
{
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
    m_playlist->addMedia(QUrl(filename));
    QList<QStandardItem*> items;
    items.append(new QStandardItem(QDir(filename).dirName()));
    items.append(new QStandardItem(filename));

    // Получаем длительность трека
    m_duration_player.setMedia(QUrl(filename));
    m_duration_player.play(); // Начинаем воспроизведение для получения длительности
    items.append(new QStandardItem(QTime::fromMSecsSinceStartOfDay(m_duration_player.duration()).toString("hh:mm:ss")));
    m_duration_player.pause(); // Останавливаем воспроизведение

    m_playlist_model->appendRow(items);
}


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

