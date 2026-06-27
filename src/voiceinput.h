#ifndef VOICEINPUT_H
#define VOICEINPUT_H

#include <QObject>
#include <QProcess>
#include <QTimer>

class VoiceInput : public QObject
{
    Q_OBJECT
    // true while a recording session is active (START sent, not yet DONE)
    Q_PROPERTY(bool isRecording READ isRecording NOTIFY isRecordingChanged)
    // true once the daemon has loaded the model and is ready for commands
    Q_PROPERTY(bool isReady READ isReady NOTIFY isReadyChanged)

public:
    explicit VoiceInput(QObject *parent = nullptr);
    ~VoiceInput() override;

    bool isRecording() const;
    bool isReady() const;

    Q_INVOKABLE void startRecording();
    Q_INVOKABLE void stopRecording();
    Q_INVOKABLE void speak(const QString &text);

signals:
    void textRecognized(const QString &text);
    void errorOccurred(const QString &error);
    void isRecordingChanged();
    void isReadyChanged();

private:
    bool     m_isRecording = false;
    bool     m_isReady = false;
    bool     m_pendingStart = false; // user pressed mic before READY
    QString  m_stderrBuffer;
    QProcess *m_daemon = nullptr;
    QProcess *m_speakProcess = nullptr;
    QTimer   *m_watchdog = nullptr;

    void startDaemon();
    void startWatchdog();
    void sendCommand(const QString &cmd);
    void setRecording(bool value);
    void setReady(bool value);

    static bool isVoskLog(const QString &line);
    static QString findScript(const QString &name);
};

#endif // VOICEINPUT_H
