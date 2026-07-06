#include "musicplayer.h"

#include <QDirIterator>
#include <QFileInfo>
#include <QSettings>
#include <QUrl>
#include <QDebug>

#include <algorithm>
#include <random>

MusicPlayer::MusicPlayer(QObject *parent) : QObject(parent)
{
    m_player.setAudioOutput(&m_audio);
    m_audio.setVolume(QSettings("MyCompanion", "client")
                          .value("music_volume", 0.7).toReal());

    connect(&m_player, &QMediaPlayer::playbackStateChanged,
            this, &MusicPlayer::stateChanged);
    // Трек закончился — сразу следующий
    connect(&m_player, &QMediaPlayer::mediaStatusChanged, this,
            [this](QMediaPlayer::MediaStatus status) {
                if (status == QMediaPlayer::EndOfMedia)
                    next();
            });
    connect(&m_player, &QMediaPlayer::errorOccurred, this,
            [this](QMediaPlayer::Error, const QString &error) {
                qWarning() << "Музыка:" << error << "—" << trackTitle();
                next(); // битый файл пропускаем
            });

    // Восстанавливаем папку с прошлого раза (без автозапуска)
    const QString saved = QSettings("MyCompanion", "client")
                              .value("music_folder").toString();
    if (!saved.isEmpty()) {
        m_folder = saved;
        scanAndShuffle();
    }
}

bool MusicPlayer::playing() const
{
    return m_player.playbackState() == QMediaPlayer::PlayingState;
}

QString MusicPlayer::trackTitle() const
{
    if (m_playlist.isEmpty())
        return QString();
    return QFileInfo(m_playlist[m_index]).completeBaseName();
}

qreal MusicPlayer::volume() const { return m_audio.volume(); }

void MusicPlayer::setVolume(qreal v)
{
    m_audio.setVolume(v);
    QSettings("MyCompanion", "client").setValue("music_volume", v);
    emit stateChanged();
}

void MusicPlayer::setFolder(const QString &folder)
{
    QString path = folder;
    if (path.startsWith(QStringLiteral("file:")))
        path = QUrl(path).toLocalFile();
    m_folder = path;
    QSettings("MyCompanion", "client").setValue("music_folder", m_folder);
    scanAndShuffle();
    if (!m_playlist.isEmpty())
        playCurrent();
}

void MusicPlayer::scanAndShuffle()
{
    m_playlist.clear();
    m_index = 0;
    QDirIterator it(m_folder,
                    {"*.mp3", "*.flac", "*.wav", "*.m4a", "*.ogg", "*.wma"},
                    QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext())
        m_playlist << it.next();

    static std::mt19937 rng{std::random_device{}()};
    std::shuffle(m_playlist.begin(), m_playlist.end(), rng);

    qInfo() << "Музыка: найдено треков:" << m_playlist.size() << "в" << m_folder;
    emit stateChanged();
}

void MusicPlayer::playCurrent()
{
    if (m_playlist.isEmpty())
        return;
    m_player.setSource(QUrl::fromLocalFile(m_playlist[m_index]));
    m_player.play();
    emit stateChanged();
}

void MusicPlayer::playPause()
{
    if (m_playlist.isEmpty())
        return;
    if (playing())
        m_player.pause();
    else if (m_player.source().isEmpty())
        playCurrent();
    else
        m_player.play();
    emit stateChanged();
}

void MusicPlayer::next()
{
    if (m_playlist.isEmpty())
        return;
    m_index = (m_index + 1) % m_playlist.size();
    playCurrent();
}

void MusicPlayer::previous()
{
    if (m_playlist.isEmpty())
        return;
    m_index = (m_index - 1 + m_playlist.size()) % m_playlist.size();
    playCurrent();
}

void MusicPlayer::reshuffle()
{
    scanAndShuffle();
    if (!m_playlist.isEmpty())
        playCurrent();
}
