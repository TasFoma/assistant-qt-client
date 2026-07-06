#ifndef MUSICPLAYER_H
#define MUSICPLAYER_H

#include <QAudioOutput>
#include <QMediaPlayer>
#include <QObject>
#include <QStringList>

// Музыка из локальной папки: рекурсивный поиск треков, перемешивание
// при каждом запуске/смене папки, пауза/следующий/предыдущий.
// Папка и громкость запоминаются между запусками.
class MusicPlayer : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool playing READ playing NOTIFY stateChanged)
    Q_PROPERTY(QString trackTitle READ trackTitle NOTIFY stateChanged)
    Q_PROPERTY(int trackIndex READ trackIndex NOTIFY stateChanged)
    Q_PROPERTY(int trackCount READ trackCount NOTIFY stateChanged)
    Q_PROPERTY(QString folder READ folder NOTIFY stateChanged)
    Q_PROPERTY(qreal volume READ volume WRITE setVolume NOTIFY stateChanged)
public:
    explicit MusicPlayer(QObject *parent = nullptr);

    bool playing() const;
    QString trackTitle() const;
    int trackIndex() const { return m_playlist.isEmpty() ? 0 : m_index + 1; }
    int trackCount() const { return m_playlist.size(); }
    QString folder() const { return m_folder; }
    qreal volume() const;
    void setVolume(qreal v);

    Q_INVOKABLE void setFolder(const QString &folder); // сканирует и играет
    Q_INVOKABLE void playPause();
    Q_INVOKABLE void next();
    Q_INVOKABLE void previous();
    Q_INVOKABLE void reshuffle();

signals:
    void stateChanged();

private:
    void scanAndShuffle();
    void playCurrent();

    QMediaPlayer m_player;
    QAudioOutput m_audio;
    QStringList m_playlist;
    int m_index = 0;
    QString m_folder;
};

#endif // MUSICPLAYER_H
