#ifndef VOICEINPUT_H
#define VOICEINPUT_H

#include <QObject>
#include <QProcess>
#include <QTimer>

class VoiceInput : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool isRecording READ isRecording NOTIFY isRecordingChanged)
    Q_PROPERTY(bool isReady     READ isReady     NOTIFY isReadyChanged)
    Q_PROPERTY(bool isSpeaking  READ isSpeaking  NOTIFY isSpeakingChanged)

public:
    explicit VoiceInput(QObject *parent = nullptr);
    ~VoiceInput() override;

    bool isRecording() const;
    bool isReady()     const;
    bool isSpeaking()  const;

    Q_INVOKABLE void startRecording();
    Q_INVOKABLE void stopRecording();
    // rate: SAPI rate -10..10 (0=normal, 2=~1.3x, 4=~1.7x, 6=~2x)
    Q_INVOKABLE void speak(const QString &text, int rate = 2);
    Q_INVOKABLE void stopSpeaking();

signals:
    void textRecognized(const QString &text);
    void errorOccurred(const QString &error);
    void isRecordingChanged();
    void isReadyChanged();
    void isSpeakingChanged();

private:
    bool     m_isRecording  = false;
    bool     m_isReady      = false;
    bool     m_isSpeaking   = false;
    bool     m_pendingStart = false;
    QString  m_stderrBuffer;
    QProcess *m_daemon       = nullptr;
    QProcess *m_speakProcess = nullptr;
    QTimer   *m_watchdog     = nullptr;

    void startDaemon();
    void startWatchdog();
    void sendCommand(const QString &cmd);
    void setRecording(bool value);
    void setReady(bool value);
    void setSpeaking(bool value);

    static bool    isVoskLog(const QString &line);
    static QString findScript(const QString &name);
};

#endif // VOICEINPUT_H
